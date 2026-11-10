#include "common/mqtt_link.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include <mqtt/async_client.h>

#include "common/config.hpp"
#include "common/csv_writer.hpp"
#include "common/message.hpp"

namespace px4mqtt {

const char* to_string(SendStatus s) {
  switch (s) {
    case SendStatus::Sent: return "sent";
    case SendStatus::Buffered: return "buffered";
    case SendStatus::Dropped: return "dropped";
  }
  return "?";
}

struct MqttLink::Impl {
  LinkOptions opts;
  mqtt::async_client client;
  mqtt::connect_options conn_opts;
  CsvWriter events;
  std::function<void()> user_connected;
  std::function<void(const ReceivedMessage&)> user_message;

  // Hàm on_connected của người dùng chạy trên luồng riêng này, KHÔNG chạy trong
  // callback của Paho. Lý do: gọi publish() ngay trong callback "connected" làm lần
  // kết nối đầu không bao giờ báo hoàn tất (connect() treo mãi) với Paho 1.2/1.3.
  std::mutex worker_mu;
  std::condition_variable worker_cv;
  int pending_connected = 0;
  bool stopping = false;
  std::thread worker;

  void worker_loop() {
    std::unique_lock<std::mutex> lk(worker_mu);
    while (true) {
      worker_cv.wait(lk, [this] { return stopping || pending_connected > 0; });
      if (stopping) return;
      pending_connected = 0;  // nhiều lần kết nối dồn lại thì chỉ cần chạy một lần
      lk.unlock();
      if (user_connected) user_connected();
      lk.lock();
    }
  }

  ~Impl() {
    {
      std::lock_guard<std::mutex> lk(worker_mu);
      stopping = true;
    }
    worker_cv.notify_all();
    if (worker.joinable()) worker.join();
  }

  static mqtt::create_options make_create_options(const LinkOptions& o) {
    mqtt::create_options c(MQTTVERSION_3_1_1);
    // Bộ đệm khi mất kết nối: giữ cả tin QoS 0 lẫn QoS 1, đầy thì bỏ tin cũ nhất.
    c.set_send_while_disconnected(o.buffer_offline);
    c.set_max_buffered_messages(kMaxBufferedMessages);
    c.set_delete_oldest_messages(true);
    return c;
  }

  explicit Impl(LinkOptions o)
      // Không truyền đối tượng persistence: bộ đệm nằm trong RAM, đủ cho đề tài.
      : opts(std::move(o)), client(kBrokerUri, opts.client_id, make_create_options(opts)) {
    conn_opts.set_mqtt_version(MQTTVERSION_3_1_1);
    conn_opts.set_user_name(kMqttUser);
    conn_opts.set_password(kMqttPassword);
    conn_opts.set_keep_alive_interval(kKeepAliveSec);
    conn_opts.set_clean_session(opts.clean_session);
    conn_opts.set_connect_timeout(5);
    // Sau lần kết nối đầu, Paho tự thử lại: chờ 1 s, rồi gấp đôi mỗi lần, tối đa 8 s.
    conn_opts.set_automatic_reconnect(kReconnectMinSec, kReconnectMaxSec);
    if (!opts.will_topic.empty()) {
      conn_opts.set_will(mqtt::will_options(opts.will_topic, opts.will_payload, opts.will_qos,
                                            opts.will_retained));
    }
    if (!opts.events_csv.empty()) events.open(opts.events_csv, "time_ms,client_id,event,detail");
    worker = std::thread([this] { worker_loop(); });

    client.set_connected_handler([this](const std::string& cause) {
      log_event("connected", cause);
      // Với clean_session = false, Broker vẫn nhớ subscription, nhưng subscribe lại
      // cũng không hại gì và giúp chắc chắn khi Broker mất dữ liệu phiên.
      if (!opts.subscribe_filter.empty()) client.subscribe(opts.subscribe_filter, opts.subscribe_qos);
      {
        std::lock_guard<std::mutex> lk(worker_mu);
        ++pending_connected;
      }
      worker_cv.notify_one();
    });
    client.set_connection_lost_handler([this](const std::string& cause) {
      log_event("connection_lost", cause);
    });
    client.set_message_callback([this](mqtt::const_message_ptr m) {
      if (!user_message) return;
      ReceivedMessage r;
      r.topic = m->get_topic();
      r.payload = m->to_string();
      r.qos = m->get_qos();
      r.retained = m->is_retained();
      r.duplicate = m->is_duplicate();
      user_message(r);
    });
  }

  void log_event(const std::string& event, const std::string& detail) {
    std::cerr << "[" << opts.client_id << "] " << event << (detail.empty() ? "" : ": " + detail) << "\n";
    if (!events.is_open()) return;
    std::string d = detail;
    for (char& ch : d)
      if (ch == ',' || ch == '\n') ch = ' ';  // giữ đúng số cột CSV
    events.write_line(std::to_string(now_ms()) + "," + opts.client_id + "," + event + "," + d);
  }
};

MqttLink::MqttLink(LinkOptions opts) : impl_(std::make_unique<Impl>(std::move(opts))) {}

MqttLink::~MqttLink() {
  try {
    if (impl_->client.is_connected()) impl_->client.disconnect()->wait_for(std::chrono::seconds(2));
  } catch (...) {
    // Hàm huỷ không được ném exception.
  }
}

void MqttLink::on_connected(std::function<void()> fn) { impl_->user_connected = std::move(fn); }

void MqttLink::on_message(std::function<void(const ReceivedMessage&)> fn) {
  impl_->user_message = std::move(fn);
}

void MqttLink::connect() {
  // Paho chỉ tự kết nối lại SAU khi đã kết nối thành công một lần, nên lần đầu tự thử lại ở đây.
  int wait = kReconnectMinSec;
  while (true) {
    try {
      impl_->log_event("connect_attempt", kBrokerUri);
      auto tok = impl_->client.connect(impl_->conn_opts);
      // Chờ có giới hạn: nếu token không bao giờ hoàn tất thì vẫn không treo vô hạn.
      tok->wait_for(std::chrono::seconds(8));
      if (impl_->client.is_connected()) return;  // callback connected đã ghi events.csv
      impl_->log_event("connect_failed", "het thoi gian cho");
    } catch (const mqtt::exception& e) {
      impl_->log_event("connect_failed", e.what());
    }
    if (impl_->client.is_connected()) return;  // Paho có thể đã tự kết nối được trong lúc chờ
    std::this_thread::sleep_for(std::chrono::seconds(wait));
    wait = std::min(wait * 2, kReconnectMaxSec);
  }
}

SendStatus MqttLink::publish(const std::string& topic, const std::string& payload, int qos,
                             bool retained) {
  // Xem trạng thái TRƯỚC khi gửi để biết tin đi ngay hay vào bộ đệm.
  const bool connected = impl_->client.is_connected();
  try {
    impl_->client.publish(topic, payload.data(), payload.size(), qos, retained);
  } catch (const mqtt::exception&) {
    // Bộ đệm tắt mà đang mất kết nối: Paho từ chối nhận tin. Ghi "dropped" và chạy tiếp.
    return SendStatus::Dropped;
  }
  return connected ? SendStatus::Sent : SendStatus::Buffered;
}

bool MqttLink::is_connected() const { return impl_->client.is_connected(); }

void MqttLink::disconnect(int timeout_ms) {
  try {
    if (impl_->client.is_connected()) {
      impl_->client.disconnect(timeout_ms)->wait();
      impl_->log_event("disconnected", "");
    }
  } catch (const mqtt::exception& e) {
    impl_->log_event("disconnect_failed", e.what());
  }
}

}  // namespace px4mqtt
