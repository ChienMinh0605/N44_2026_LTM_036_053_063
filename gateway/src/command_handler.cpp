#include "command_handler.hpp"

#include <cmath>

#include "common/config.hpp"

namespace px4mqtt {

using nlohmann::json;

bool CommandHandler::remember(const std::string& key) {
  if (!seen_.insert(key).second) return false;
  seen_order_.push_back(key);
  if (seen_order_.size() > 10000) {  // chỉ nhớ 10000 lệnh gần nhất
    seen_.erase(seen_order_.front());
    seen_order_.pop_front();
  }
  return true;
}

CommandOutcome CommandHandler::handle(const std::string& payload, int64_t recv_ms, bool retained,
                                      bool dup) {
  std::string from, action, reason;
  int64_t cmd_id = -1, sent_ms = 0, ttl_ms = kCmdDefaultTtlMs;
  DroneCommand cmd;

  // Đọc JSON. Thiếu trường bắt buộc thì từ chối "invalid".
  try {
    const json j = json::parse(payload);
    from = j.at("from").get<std::string>();
    cmd_id = j.at("cmd_id").get<int64_t>();
    action = j.at("action").get<std::string>();
    sent_ms = j.at("sent_ms").get<int64_t>();
    if (j.contains("ttl_ms")) ttl_ms = j["ttl_ms"].get<int64_t>();
    cmd.action = action;
    if (j.contains("alt")) cmd.alt = j["alt"].get<double>();
    if (j.contains("lat")) cmd.lat = j["lat"].get<double>();
    if (j.contains("lon")) cmd.lon = j["lon"].get<double>();
    if (from.empty() || ttl_ms <= 0) reason = "invalid";
  } catch (const std::exception&) {
    reason = "invalid";
  }
  const int64_t age_ms = sent_ms > 0 ? recv_ms - sent_ms : -1;

  if (reason.empty() && safety_) {
    // Kiểm tra 1: lệnh retained. Broker gửi lại tin retained mỗi khi Gateway subscribe
    // (tức là mỗi lần kết nối lại), nên một lệnh retained sẽ bị chạy lại nhiều lần.
    if (retained) reason = "retained";
    // Kiểm tra 2: hạn dùng. sent_ms là giờ máy người gửi, nên hai máy cần đồng bộ
    // đồng hồ (NTP); trong thí nghiệm người gửi chạy cùng máy với Gateway.
    else if (age_ms > ttl_ms) reason = "expired";
    // Kiểm tra 3: trùng lệnh. QoS 1 gửi lại sau khi kết nối lại (cờ DUP) giữ nguyên cmd_id.
    else if (!remember(from + "#" + std::to_string(cmd_id))) reason = "duplicate";
    // Kiểm tra 4: quyền điều khiển. Quyền tự nhả nếu người giữ im lặng quá kCmdLeaseMs.
    else if (!owner_.empty() && owner_ != from && recv_ms - owner_last_ms_ <= kCmdLeaseMs)
      reason = "not_owner";
    else {
      owner_ = from;
      owner_last_ms_ = recv_ms;
    }
  }

  // Qua đủ kiểm tra: "release" chỉ nhả quyền, các lệnh khác chuyển cho drone.
  if (reason.empty()) {
    if (action == "release") {
      if (owner_ == from) owner_.clear();
    } else {
      reason = drone_.execute(cmd);
    }
  }

  CommandOutcome out;
  out.ack = {{"cmd_id", cmd_id},       {"from", from},
             {"action", action},       {"result", reason.empty() ? "accepted" : "rejected"},
             {"reason", reason},       {"sent_ms", sent_ms},
             {"gw_recv_ms", recv_ms},  {"owner", owner_}};
  std::string a = action;
  for (char& ch : a)
    if (ch == ',' || ch == '\n') ch = ' ';  // giữ đúng số cột CSV
  std::string f = from;
  for (char& ch : f)
    if (ch == ',' || ch == '\n') ch = ' ';
  out.log_line = std::to_string(recv_ms) + "," + f + "," + std::to_string(cmd_id) + "," + a + "," +
                 std::to_string(sent_ms) + "," + std::to_string(age_ms) + "," +
                 (retained ? "1" : "0") + "," + (dup ? "1" : "0") + "," +
                 (reason.empty() ? "accepted" : "rejected") + "," + reason;
  return out;
}

}  // namespace px4mqtt
