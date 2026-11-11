// Nguồn dữ liệu giả: mô phỏng drone đứng yên (hover) như điều kiện đo trong kế hoạch.
// Vị trí và độ cao dao động rất nhẹ quanh một điểm, pin hao 0,05%/giây.
// Vì phép đo (seq, timestamp_ms) bắt đầu từ Gateway, dùng dữ liệu giả không làm thay đổi
// kết quả thí nghiệm; nó chỉ thay cho PX4 khi không có máy chạy được Gazebo.

#include <algorithm>
#include <chrono>
#include <cmath>

#include "telemetry_source.hpp"

namespace px4mqtt {
namespace {

class FakeSource : public TelemetrySource {
 public:
  bool start() override {
    t0_ = std::chrono::steady_clock::now();
    return true;
  }

  // Tính trực tiếp theo thời gian, không có biến dùng chung nên không cần mutex.
  TelemetrySnapshot latest() override {
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0_).count();
    TelemetrySnapshot s;
    s.lat = 47.3977 + 0.000002 * std::sin(t / 3.0);
    s.lon = 8.5456 + 0.000002 * std::cos(t / 3.0);
    s.rel_alt_m = 25.5 + 0.1 * std::sin(t / 2.0);
    s.ground_mps = 0.1 + 0.05 * std::fabs(std::sin(t));
    s.battery_pct = std::max(0.0, 100.0 - 0.05 * t);
    s.voltage_v = 13.2 + 3.6 * s.battery_pct / 100.0;  // pin 4S: ~13,2 V lúc cạn, ~16,8 V lúc đầy
    s.valid = true;
    return s;
  }

 private:
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace

std::unique_ptr<TelemetrySource> make_fake_source() { return std::make_unique<FakeSource>(); }

}  // namespace px4mqtt
