// Nguồn dữ liệu của Gateway. Vòng lặp phát chỉ cần hỏi "giá trị mới nhất là gì",
// không cần biết dữ liệu đến từ PX4 thật hay được sinh giả.
//   - FakeSource (fake_source.cpp): drone đứng yên (hover), pin hao dần. Dùng cho thí nghiệm.
//   - Px4Source  (px4_source.cpp): đọc MAVLink qua MAVSDK. Chỉ có khi build với MAVSDK.
#pragma once

#include <cmath>
#include <memory>
#include <string>

namespace px4mqtt {

// Giá trị mới nhất của 4 loại số liệu, đã đổi sang đơn vị dùng trong JSON (mục 2 kế hoạch).
struct TelemetrySnapshot {
  double lat = 0, lon = 0;   // độ
  double rel_alt_m = 0;      // mét, so với điểm cất cánh
  double ground_mps = 0;     // m/s, tốc độ ngang
  double battery_pct = 0;    // %
  double voltage_v = 0;      // V
  bool valid = false;        // false: chưa nhận được dữ liệu nào
};

// Một lệnh điều khiển đã qua các kiểm tra an toàn của Gateway (command_handler).
// Trường nào không có trong JSON thì để NaN.
struct DroneCommand {
  std::string action;  // takeoff | land | rtl | goto | hold
  double alt = NAN;    // mét so với điểm cất cánh (takeoff, goto)
  double lat = NAN;    // độ (goto)
  double lon = NAN;    // độ (goto)
};

class TelemetrySource {
 public:
  virtual ~TelemetrySource() = default;
  // Bắt đầu nhận dữ liệu. Trả về false nếu không khởi động được.
  virtual bool start() = 0;
  // Giá trị mới nhất. Gọi từ luồng của vòng lặp phát, nên phải an toàn đa luồng.
  virtual TelemetrySnapshot latest() = 0;
  // Thực hiện lệnh. Trả về chuỗi rỗng nếu chấp nhận, ngược lại là lý do từ chối
  // (ví dụ "invalid_state", "out_of_range"). Gọi từ luồng xử lý lệnh.
  virtual std::string execute(const DroneCommand&) { return "not_supported"; }
};

std::unique_ptr<TelemetrySource> make_fake_source();

// Trả về nullptr nếu Gateway được build không có MAVSDK.
std::unique_ptr<TelemetrySource> make_px4_source(const std::string& mavlink_url);

}  // namespace px4mqtt
