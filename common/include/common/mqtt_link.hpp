// MqttLink: phần MQTT dùng chung cho Gateway và Logger (mục 3 của kế hoạch).
//
// Gói gọn mọi thứ liên quan tới Paho vào một chỗ:
//   - kết nối tới Broker bằng MQTT 3.1.1, user/password, keep-alive
//   - tự kết nối lại khi mất kết nối (chờ 1 -> 2 -> 4 -> 8 giây)
//   - bộ đệm khi mất kết nối (Gateway bật, thí nghiệm C so bật/tắt)
//   - Last Will (Gateway dùng cho topic status)
//   - tự subscribe lại sau mỗi lần kết nối (Logger)
//   - ghi events.csv: lúc nào mất kết nối, lúc nào kết nối lại
//
// Người dùng lớp này (Gateway, Logger) không cần include Paho.
//
// Cách dùng điển hình (Gateway):
//   LinkOptions o;  o.client_id = kGatewayClientId;  o.buffer_offline = true;
//   o.will_topic = kTopicStatus;  o.will_payload = status_payload(false);
//   o.events_csv = "data/.../events.csv";
//   MqttLink link(o);
//   link.on_connected([&] { link.publish(kTopicStatus, status_payload(true), 1, true); });
//   link.connect();
//   SendStatus s = link.publish(kTopicBattery, payload, 1);   // ghi s vào sent.csv
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace px4mqtt {

// Số phận của một bản tin ngay lúc gọi publish(), để ghi cột trạng thái trong sent.csv.
enum class SendStatus {
  Sent,      // đang kết nối, tin được giao cho Paho để gửi đi ngay
  Buffered,  // đang mất kết nối, tin nằm trong bộ đệm chờ gửi bù
  Dropped    // đang mất kết nối và bộ đệm tắt: tin bị bỏ
};
const char* to_string(SendStatus s);  // "sent" / "buffered" / "dropped"

struct LinkOptions {
  std::string client_id;

  // false: Broker nhớ phiên (subscription, tin QoS 1 chờ gửi) khi client vắng mặt.
  // Logger cần false cho thí nghiệm F; Gateway cũng nên false để tin QoS 1 đang
  // chờ PUBACK được gửi lại sau khi kết nối lại.
  bool clean_session = false;

  // Bộ đệm khi mất kết nối (chỉ có ý nghĩa với bên publish).
  bool buffer_offline = true;

  // Last Will. Để trống will_topic nếu không dùng.
  std::string will_topic;
  std::string will_payload;
  int will_qos = 1;
  bool will_retained = true;

  // Bộ lọc topic để subscribe sau mỗi lần kết nối. Để trống nếu không subscribe.
  std::string subscribe_filter;
  int subscribe_qos = 1;

  // Đường dẫn file events.csv. Để trống nếu không ghi.
  std::string events_csv;
};

struct ReceivedMessage {
  std::string topic;
  std::string payload;
  int qos = 0;
  bool retained = false;
  bool duplicate = false;  // cờ DUP: Broker đang gửi lại tin này
};

class MqttLink {
 public:
  explicit MqttLink(LinkOptions opts);
  ~MqttLink();
  MqttLink(const MqttLink&) = delete;
  MqttLink& operator=(const MqttLink&) = delete;

  // Đặt các hàm xử lý TRƯỚC khi gọi connect().
  // on_connected chạy trên một luồng riêng của MqttLink, được phép gọi publish() bên trong.
  // on_message chạy trên luồng của Paho: làm việc ngắn gọn, không chờ lâu bên trong.
  void on_connected(std::function<void()> fn);
  void on_message(std::function<void(const ReceivedMessage&)> fn);

  // Kết nối lần đầu. Chặn cho tới khi kết nối được: Broker chưa chạy thì thử lại
  // theo nhịp 1, 2, 4, 8, 8... giây. Sau lần đầu, Paho tự lo việc kết nối lại.
  // should_stop: hàm trả về true khi người dùng bấm Ctrl+C, để thoát khỏi vòng chờ.
  // Trả về false nếu bị dừng trước khi kết nối được.
  bool connect(const std::function<bool()>& should_stop = nullptr);

  // Gửi một bản tin. Không chờ PUBACK; trả về trạng thái để ghi sent.csv.
  SendStatus publish(const std::string& topic, const std::string& payload, int qos,
                     bool retained = false);

  bool is_connected() const;

  // Ngắt kết nối đúng cách (gửi DISCONNECT, Broker sẽ KHÔNG phát Last Will).
  // Chờ tối đa timeout_ms để các tin còn dở được gửi xong.
  void disconnect(int timeout_ms = 3000);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace px4mqtt
