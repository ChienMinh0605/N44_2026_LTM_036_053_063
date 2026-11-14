// Bộ kiểm tra an toàn cho kênh lệnh (docs/command_channel.md).
//
// Các tính năng MQTT có lợi cho telemetry lại nguy hiểm với lệnh điều khiển:
//   - Retained: Gateway kết nối lại là nhận lại lệnh cũ          -> kiểm tra 1: từ chối retained
//   - Broker giữ tin QoS 1 khi Gateway vắng: lệnh tới muộn      -> kiểm tra 2: lệnh có hạn dùng (TTL)
//   - QoS 1 "ít nhất một lần": lệnh có thể tới hai lần           -> kiểm tra 3: bỏ lệnh trùng cmd_id
//   - Nhiều client cùng publish được                             -> kiểm tra 4: chỉ một người giữ quyền
// Lệnh qua đủ 4 kiểm tra mới được chuyển cho drone (TelemetrySource::execute).
#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "telemetry_source.hpp"

namespace px4mqtt {

struct CommandOutcome {
  nlohmann::json ack;    // nội dung gửi lên drone/001/cmd/ack
  std::string log_line;  // một dòng cmd_log.csv
};

class CommandHandler {
 public:
  static constexpr const char* kLogHeader =
      "gw_recv_ms,from,cmd_id,action,sent_ms,age_ms,retained,dup_flag,result,reason";

  // safety = false: bỏ cả 4 kiểm tra (chỉ để làm thí nghiệm so sánh).
  CommandHandler(bool safety, TelemetrySource& drone) : safety_(safety), drone_(drone) {}

  // payload: JSON của lệnh; recv_ms: lúc Gateway nhận; retained/dup: cờ của gói MQTT.
  CommandOutcome handle(const std::string& payload, int64_t recv_ms, bool retained, bool dup);

 private:
  bool remember(const std::string& key);  // false nếu đã thấy key này

  bool safety_;
  TelemetrySource& drone_;
  std::string owner_;           // người đang giữ quyền điều khiển
  int64_t owner_last_ms_ = 0;   // lần cuối người giữ quyền gửi lệnh
  std::unordered_set<std::string> seen_;  // các (from, cmd_id) đã xử lý
  std::deque<std::string> seen_order_;    // để giới hạn bộ nhớ
};

}  // namespace px4mqtt
