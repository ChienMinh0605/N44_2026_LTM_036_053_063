// Gateway: lấy telemetry (PX4 thật hoặc dữ liệu giả) -> JSON -> MQTT.
//
//   ./build/gateway/gateway --source fake --qos 1 --buffer on --scenario N0 --run 1 --out data/E/N0_qos1_run1
//
// Mỗi 200 ms phát 4 topic (position, altitude, velocity, battery) = 20 tin/giây.
// Mỗi tin ghi một dòng sent.csv để đối chiếu với logger.csv khi tính tỷ lệ nhận.
// Ctrl+C: publish "offline" (retained) rồi mới ngắt, vì ngắt đúng cách thì Broker
// không phát Last Will.
//
// Chia việc:
//   - Nguồn PX4 (px4_source.cpp): người có máy chạy PX4.
//   - Phần MQTT: common/mqtt_link (Chiến). Vòng lặp phát và nguồn giả: file này, fake_source.cpp.

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <thread>

#include "common/config.hpp"
#include "common/csv_writer.hpp"
#include "common/message.hpp"
#include "common/mqtt_link.hpp"
#include "common/options.hpp"
#include "telemetry_source.hpp"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }  // chỉ bật cờ; dọn dẹp ở luồng chính
}  // namespace

int main(int argc, char** argv) {
  using namespace px4mqtt;
  using nlohmann::json;
  const Options opt = parse_options(argc, argv);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  // boot_id khác nhau mỗi lần chạy, để seq đếm lại từ 0 không bị nhầm là tin trùng.
  const int64_t boot_id = now_ms();

  // 1. Nguồn dữ liệu
  std::unique_ptr<TelemetrySource> source =
      opt.source == "px4" ? make_px4_source(kMavlinkUrl) : make_fake_source();
  if (!source) {
    std::cerr << "Gateway nay duoc build khong co MAVSDK, chi chay duoc --source fake\n";
    return 1;
  }
  if (!source->start()) return 1;

  // 2. Kết nối MQTT (bộ đệm, Last Will, tự kết nối lại đều nằm trong MqttLink)
  LinkOptions lo;
  lo.client_id = kGatewayClientId;
  lo.clean_session = false;  // tin QoS 1 chưa có PUBACK được gửi lại sau khi kết nối lại
  lo.buffer_offline = opt.buffer;
  lo.will_topic = kTopicStatus;
  lo.will_payload = status_payload(false);
  lo.events_csv = opt.out_dir + "/events.csv";
  MqttLink link(lo);
  // Chạy sau MỖI lần kết nối (cả lần đầu và sau khi kết nối lại).
  link.on_connected([&] { link.publish(kTopicStatus, status_payload(true), 1, true); });

  CsvWriter sent(opt.out_dir + "/sent.csv", "boot_id,topic,seq,qos,timestamp_ms,status");

  std::cout << "Gateway boot_id=" << boot_id << " source=" << opt.source << " qos=" << opt.qos
            << " buffer=" << (opt.buffer ? "on" : "off") << " scenario=" << opt.scenario
            << " run=" << opt.run << " -> " << opt.out_dir << "\n";
  if (!link.connect([] { return g_stop.load(); })) return 1;  // Ctrl+C khi Broker chưa chạy

  // 3. Vòng lặp phát: đúng nhịp 200 ms, dùng mốc thời gian tuyệt đối để không bị trôi nhịp.
  const std::array<const char*, 4> topics = {kTopicPosition, kTopicAltitude, kTopicVelocity,
                                             kTopicBattery};
  const std::array<const char*, 4> fields = {"position", "altitude", "velocity", "battery"};
  std::array<uint64_t, 4> seq = {0, 0, 0, 0};  // seq riêng cho từng topic
  std::map<std::string, long> count;           // đếm sent/buffered/dropped để in tiến độ
  auto next_tick = std::chrono::steady_clock::now();
  auto next_report = next_tick + std::chrono::seconds(5);

  while (!g_stop) {
    const TelemetrySnapshot s = source->latest();
    if (s.valid) {
      const std::array<json, 4> values = {
          json{{"lat", s.lat}, {"lon", s.lon}},
          json{{"rel_m", s.rel_alt_m}},
          json{{"ground_mps", s.ground_mps}},
          json{{"percent", s.battery_pct}, {"voltage", s.voltage_v}},
      };
      for (size_t i = 0; i < topics.size(); ++i) {
        Telemetry t;
        t.drone_id = kDroneId;
        t.boot_id = boot_id;
        t.seq = seq[i]++;
        t.timestamp_ms = now_ms();  // ngay trước khi publish
        t.field = fields[i];
        t.value = values[i];
        const SendStatus st = link.publish(topics[i], to_payload(t), opt.qos);
        ++count[to_string(st)];
        sent.write_line(std::to_string(boot_id) + "," + topics[i] + "," + std::to_string(t.seq) +
                        "," + std::to_string(opt.qos) + "," + std::to_string(t.timestamp_ms) + "," +
                        to_string(st));
      }
    }

    if (std::chrono::steady_clock::now() >= next_report) {
      std::cout << "sent=" << count["sent"] << " buffered=" << count["buffered"]
                << " dropped=" << count["dropped"] << (link.is_connected() ? "" : "  (MAT KET NOI)")
                << "\n";
      next_report += std::chrono::seconds(5);
    }
    next_tick += std::chrono::milliseconds(kPublishPeriodMs);
    std::this_thread::sleep_until(next_tick);
  }

  // 4. Tắt êm: tự báo offline, chờ gửi xong rồi DISCONNECT.
  std::cout << "Dang dung Gateway...\n";
  link.publish(kTopicStatus, status_payload(false), 1, true);
  link.disconnect();
  std::cout << "Tong: sent=" << count["sent"] << " buffered=" << count["buffered"]
            << " dropped=" << count["dropped"] << "\n";
  return 0;
}
