// Logger: subscribe drone/001/# và ghi mỗi bản tin thành một dòng CSV để đo đạc.
// Đây là "máy đo" của đề tài: mọi chỉ số (tỷ lệ nhận, tin trùng, độ trễ) đều tính
// từ file này đối chiếu với sent.csv của Gateway.
//
//   ./build/logger/logger --qos 1 --scenario N1 --run 2 --out data/E/N1_qos1_run2
//   ./build/logger/logger --clean      (xoá phiên cũ của logger-01 trên Broker rồi thoát)

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "common/config.hpp"
#include "common/csv_writer.hpp"
#include "common/message.hpp"
#include "common/mqtt_link.hpp"
#include "common/options.hpp"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }  // chỉ bật cờ; việc dọn dẹp làm ở luồng chính
}  // namespace

int main(int argc, char** argv) {
  using namespace px4mqtt;
  const Options opt = parse_options(argc, argv);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  // --clean: kết nối với clean_session = true. Theo MQTT 3.1.1, Broker xoá phiên cũ
  // của client_id này (subscription + hàng đợi tin QoS 1 còn tồn) rồi mới nhận kết nối.
  // Chạy trước mỗi lần đo để Logger không nhận lại tin của lần đo trước.
  if (opt.clean) {
    LinkOptions o;
    o.client_id = kLoggerClientId;
    o.clean_session = true;
    MqttLink link(o);
    if (!link.connect([] { return g_stop.load(); })) return 1;
    link.disconnect();
    std::cout << "Da xoa phien cu cua " << kLoggerClientId << "\n";
    return 0;
  }

  CsvWriter csv(opt.out_dir + "/logger.csv",
                "recv_time_ms,topic,boot_id,seq,timestamp_ms,latency_ms,qos,scenario,run");
  std::atomic<long> received{0}, bad{0};

  LinkOptions o;
  o.client_id = kLoggerClientId;
  o.clean_session = false;  // Broker giữ tin QoS 1 cho Logger khi vắng mặt (thí nghiệm F)
  o.subscribe_filter = kTopicAll;
  o.subscribe_qos = opt.qos;
  o.events_csv = opt.out_dir + "/logger_events.csv";
  MqttLink link(o);

  link.on_message([&](const ReceivedMessage& m) {
    // Lấy giờ nhận NGAY ĐẦU hàm để độ trễ không bị cộng thêm thời gian xử lý.
    const int64_t recv = now_ms();
    if (m.topic == kTopicStatus) {
      std::cout << "status: " << m.payload << (m.retained ? " (retained)" : "") << "\n";
      return;  // status không phải telemetry, không tính vào số liệu
    }
    // Lệnh và ack (drone/001/cmd, drone/001/cmd/ack) cũng không phải telemetry.
    if (m.topic.rfind(kTopicCmd, 0) == 0) return;
    try {
      const Telemetry t = from_payload(m.payload);
      csv.write_line(std::to_string(recv) + "," + m.topic + "," + std::to_string(t.boot_id) + "," +
                     std::to_string(t.seq) + "," + std::to_string(t.timestamp_ms) + "," +
                     std::to_string(recv - t.timestamp_ms) + "," + std::to_string(m.qos) + "," +
                     opt.scenario + "," + std::to_string(opt.run));
      ++received;
    } catch (const std::exception& e) {
      ++bad;  // JSON sai định dạng: bỏ qua, không làm Logger dừng
      std::cerr << "Bo qua tin sai dinh dang tren " << m.topic << ": " << e.what() << "\n";
    }
  });

  std::cout << "Logger qos=" << opt.qos << " scenario=" << opt.scenario << " run=" << opt.run
            << " -> " << opt.out_dir << "/logger.csv\n";
  if (!link.connect([] { return g_stop.load(); })) return 1;  // Ctrl+C khi Broker chưa chạy

  // Luồng chính chỉ chờ Ctrl+C, cứ 5 giây in số tin đã nhận để biết Logger còn chạy.
  long last = 0;
  auto next = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!g_stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (std::chrono::steady_clock::now() >= next) {
      const long now = received;
      std::cout << "da nhan " << now << " tin (+" << (now - last) / 5 << "/s)\n";
      last = now;
      next += std::chrono::seconds(5);
    }
  }

  link.disconnect();
  std::cout << "Dung. Tong cong " << received << " tin, " << bad << " tin sai dinh dang.\n";
  return 0;
}
