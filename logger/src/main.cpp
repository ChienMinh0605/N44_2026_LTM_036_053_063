// Logger: subscribe drone/001/# và ghi CSV để đo đạc (bạn A).
// Khung chương trình; phần TODO viết theo mục 3b và 6 của kế hoạch.

#include <iostream>

#include "common/config.hpp"
#include "common/message.hpp"
#include "common/options.hpp"

int main(int argc, char** argv) {
  const px4mqtt::Options opt = px4mqtt::parse_options(argc, argv);

  std::cout << "Logger qos=" << opt.qos << " clean=" << (opt.clean ? "yes" : "no")
            << " scenario=" << opt.scenario << " run=" << opt.run << "\n";

  // TODO Kết nối tới kBrokerUri với client id cố định kLoggerClientId, MQTT 3.1.1.
  //   - Nếu opt.clean: kết nối với clean_session = true rồi ngắt luôn (xóa phiên cũ), thoát.
  //   - Ngược lại: clean_session = false, subscribe kTopicAll với opt.qos.

  // TODO Mỗi tin nhận được:
  //   - bỏ qua kTopicStatus khi ghi số liệu
  //   - from_payload(), recv_time_ms = now_ms(), latency_ms = recv_time_ms - timestamp_ms
  //   - ghi CSV 9 cột:
  //     recv_time_ms, topic, boot_id, seq, timestamp_ms, latency_ms, qos, scenario, run

  // TODO Chạy tới khi bị dừng (Ctrl+C), đóng file CSV gọn gàng.

  return 0;
}
