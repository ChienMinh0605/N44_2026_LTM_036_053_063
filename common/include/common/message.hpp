// Định dạng bản tin JSON theo mục 2 của kế hoạch:
// { "drone_id": "001", "boot_id": 1793541590000, "seq": 125,
//   "timestamp_ms": 1793541600000, "field": "battery",
//   "value": { "percent": 87.0, "voltage": 15.8 } }
#pragma once

#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace px4mqtt {

struct Telemetry {
  std::string drone_id;
  int64_t boot_id = 0;       // Unix ms lúc Gateway khởi động
  uint64_t seq = 0;          // tăng dần riêng cho từng topic, bắt đầu từ 0
  int64_t timestamp_ms = 0;  // Unix ms lúc gọi publish (hoặc lúc vào bộ đệm)
  std::string field;         // position / altitude / velocity / battery
  nlohmann::json value;      // nội dung riêng của từng topic
};

// Thời gian hiện tại, Unix milliseconds (đồng hồ hệ thống).
int64_t now_ms();

// Telemetry -> chuỗi JSON để publish.
std::string to_payload(const Telemetry& t);

// Chuỗi JSON nhận được -> Telemetry. Ném nlohmann::json::exception nếu sai định dạng.
Telemetry from_payload(const std::string& payload);

// Bản tin cho topic status: { "drone_id": "001", "state": "online" | "offline" }
std::string status_payload(bool online);

}  // namespace px4mqtt
