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
#include <sstream>

#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
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
    action_.emplace(system.value());

    // Xin PX4 gửi mỗi loại ít nhất 5 lần/giây, bằng nhịp phát của Gateway.
    telemetry_->set_rate_position(5.0);
    telemetry_->set_rate_velocity_ned(5.0);
    telemetry_->set_rate_battery(5.0);

    telemetry_->subscribe_position([this](Telemetry::Position p) {
      std::lock_guard<std::mutex> lk(mu_);
      snap_.lat = p.latitude_deg;
      snap_.lon = p.longitude_deg;
      snap_.rel_alt_m = p.relative_altitude_m;
      abs_alt_m_ = p.absolute_altitude_m;
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

  // Kênh lệnh -> MAVSDK Action (CHƯA build thử: máy viết code không có MAVSDK).
  // Các hàm Action ở đây là bản đồng bộ, chờ PX4 trả lời (thường dưới 1 giây).
  std::string execute(const DroneCommand& c) override {
    using mavsdk::Action;
    if (!action_) return "not_ready";
    Action::Result r = Action::Result::Unknown;
    if (c.action == "takeoff") {
      const float a = std::isnan(c.alt) ? 10.0f : static_cast<float>(c.alt);
      if (a < 1.0f || a > 120.0f) return "out_of_range";
      action_->set_takeoff_altitude(a);
      r = action_->arm();
      if (r == Action::Result::Success) r = action_->takeoff();
    } else if (c.action == "land") {
      r = action_->land();
    } else if (c.action == "rtl") {
      r = action_->return_to_launch();
    } else if (c.action == "hold") {
      r = action_->hold();
    } else if (c.action == "goto") {
      if (std::isnan(c.lat) || std::isnan(c.lon)) return "invalid";
      double rel_now, abs_now;
      {
        std::lock_guard<std::mutex> lk(mu_);
        rel_now = snap_.rel_alt_m;
        abs_now = abs_alt_m_;
      }
      // goto_location cần độ cao so với mực nước biển (AMSL), lệnh dùng độ cao tương đối.
      const double rel = std::isnan(c.alt) ? rel_now : c.alt;
      if (rel < 1.0 || rel > 120.0) return "out_of_range";
      r = action_->goto_location(c.lat, c.lon, static_cast<float>(abs_now - rel_now + rel), NAN);
    } else {
      return "invalid";
    }
    if (r == Action::Result::Success) return "";
    std::ostringstream os;
    os << "px4_" << r;  // ví dụ px4_CommandDenied
    return os.str();
  }

  TelemetrySnapshot latest() override {
    std::lock_guard<std::mutex> lk(mu_);
    return snap_;
  }

 private:
  std::string url_;
  mavsdk::Mavsdk mavsdk_;
  std::optional<mavsdk::Telemetry> telemetry_;
  std::optional<mavsdk::Action> action_;
  double abs_alt_m_ = 0;  // độ cao AMSL, cho lệnh goto
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
