// Nguồn dữ liệu PX4 thật qua MAVSDK (phần của người có máy chạy PX4).
//
// LƯU Ý: bản này viết theo API MAVSDK v2 nhưng CHƯA được build thử, vì máy viết nó
// không cài MAVSDK. Người có MAVSDK build và sửa nếu tên hàm lệch phiên bản.
//
// Nguyên tắc (mục 2 kế hoạch): callback của MAVSDK chạy ở luồng khác, chỉ ghi đè
// "giá trị mới nhất" có mutex; việc publish do timer 200 ms của Gateway làm.

#include "telemetry_source.hpp"

#ifdef HAVE_MAVSDK

#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <optional>

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

namespace px4mqtt {
namespace {

class Px4Source : public TelemetrySource {
 public:
  explicit Px4Source(std::string url)
      : url_(std::move(url)),
        mavsdk_(mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}) {}

  bool start() override {
    using namespace mavsdk;
    if (mavsdk_.add_any_connection(url_) != ConnectionResult::Success) {
      std::cerr << "Khong mo duoc ket noi MAVLink " << url_ << "\n";
      return false;
    }
    std::cerr << "Dang cho PX4 tren " << url_ << " ...\n";
    auto system = mavsdk_.first_autopilot(10.0);  // chờ tối đa 10 s
    if (!system) {
      std::cerr << "Khong thay PX4 sau 10 s\n";
      return false;
    }
    telemetry_.emplace(system.value());

    // Xin PX4 gửi mỗi loại ít nhất 5 lần/giây, bằng nhịp phát của Gateway.
    telemetry_->set_rate_position(5.0);
    telemetry_->set_rate_velocity_ned(5.0);
    telemetry_->set_rate_battery(5.0);

    telemetry_->subscribe_position([this](Telemetry::Position p) {
      std::lock_guard<std::mutex> lk(mu_);
      snap_.lat = p.latitude_deg;
      snap_.lon = p.longitude_deg;
      snap_.rel_alt_m = p.relative_altitude_m;
      snap_.valid = true;
    });
    telemetry_->subscribe_velocity_ned([this](Telemetry::VelocityNed v) {
      std::lock_guard<std::mutex> lk(mu_);
      snap_.ground_mps = std::hypot(v.north_m_s, v.east_m_s);  // tốc độ ngang
    });
    telemetry_->subscribe_battery([this](Telemetry::Battery b) {
      std::lock_guard<std::mutex> lk(mu_);
      snap_.battery_pct = b.remaining_percent;  // MAVSDK v2: 0..100 (v1: 0..1)
      snap_.voltage_v = b.voltage_v;
    });
    return true;
  }

  TelemetrySnapshot latest() override {
    std::lock_guard<std::mutex> lk(mu_);
    return snap_;
  }

 private:
  std::string url_;
  mavsdk::Mavsdk mavsdk_;
  std::optional<mavsdk::Telemetry> telemetry_;
  std::mutex mu_;
  TelemetrySnapshot snap_;
};

}  // namespace

std::unique_ptr<TelemetrySource> make_px4_source(const std::string& mavlink_url) {
  return std::make_unique<Px4Source>(mavlink_url);
}

}  // namespace px4mqtt

#else  // không có MAVSDK

namespace px4mqtt {
std::unique_ptr<TelemetrySource> make_px4_source(const std::string&) { return nullptr; }
}  // namespace px4mqtt

#endif
