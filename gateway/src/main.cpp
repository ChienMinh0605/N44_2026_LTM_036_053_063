// Gateway: PX4 (MAVLink/UDP) -> JSON -> MQTT.
// Khung chương trình; phần TODO nhóm tự viết theo mục 2, 3, 5 của kế hoạch.
//
// Chia việc:
//   - Nửa MAVSDK (đọc telemetry, timer 200 ms): người có máy chạy PX4.
//   - Nửa MQTT (Paho, bộ đệm, Last Will, sent.csv, events.csv): bạn A,
//     viết trong common/ để Logger dùng lại.

#include <iostream>

#include "common/config.hpp"
#include "common/message.hpp"
#include "common/options.hpp"

int main(int argc, char** argv) {
  const px4mqtt::Options opt = px4mqtt::parse_options(argc, argv);
  const int64_t boot_id = px4mqtt::now_ms();

  std::cout << "Gateway boot_id=" << boot_id << " source=" << opt.source << " qos=" << opt.qos
            << " buffer=" << (opt.buffer ? "on" : "off") << " scenario=" << opt.scenario
            << " run=" << opt.run << "\n";

  // TODO (MQTT) Tạo mqtt::async_client tới kBrokerUri với kGatewayClientId:
  //   - create_options: send_while_disconnected = opt.buffer,
  //     max_buffered_messages = kMaxBufferedMessages, delete_oldest_messages = true
  //   - connect_options: user/password, keep-alive kKeepAliveSec,
  //     automatic reconnect kReconnectMinSec..kReconnectMaxSec, MQTT 3.1.1
  //   - Last Will: topic kTopicStatus, status_payload(false), retained
  //   - callback connected: publish status_payload(true) retained; ghi events.csv
  //   - callback connection_lost: ghi events.csv

  // TODO (MAVSDK) Nếu opt.source == "px4":
  //   - Mavsdk, add_any_connection(kMavlinkUrl), chờ có system
  //   - Telemetry: set_rate_* >= 5 Hz; subscribe position, velocity_ned, battery
  //   - callback chỉ lưu giá trị mới nhất vào biến dùng chung (có mutex)
  // Nếu opt.source == "fake": sinh dữ liệu giả cùng định dạng.

  // TODO (vòng lặp) Mỗi kPublishPeriodMs:
  //   - đọc giá trị mới nhất, tạo Telemetry cho 4 topic (seq riêng từng topic,
  //     boot_id, timestamp_ms = now_ms()), publish với opt.qos
  //   - ghi một dòng sent.csv cho mỗi tin: boot_id, topic, seq, qos, timestamp_ms, trạng thái
  //   - bộ đệm tắt mà đang mất kết nối: bắt mqtt::exception, ghi trạng thái "bị bỏ"

  // TODO (tắt êm) Bắt Ctrl+C (SIGINT): publish status_payload(false) retained,
  //   chờ gửi xong rồi disconnect.

  (void)boot_id;
  return 0;
}
