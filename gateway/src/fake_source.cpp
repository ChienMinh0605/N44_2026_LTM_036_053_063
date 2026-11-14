// Nguồn dữ liệu giả = drone giả lập đơn giản, nhận được lệnh điều khiển.
//
// Khi không có lệnh: drone đứng yên (hover) ở 25,5 m như điều kiện đo trong kế hoạch,
// vị trí và độ cao dao động rất nhẹ, pin hao dần. Vì vậy các thí nghiệm A–F không đổi.
// Khi có lệnh: drone di chuyển dần tới đích với tốc độ giới hạn (lên/xuống 2 m/s,
// ngang 5 m/s), nên trên app thấy được độ cao, vị trí thay đổi theo thời gian.
// Đây không phải mô phỏng vật lý; chỉ đủ để kiểm tra kênh lệnh khi không có PX4.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>

#include "telemetry_source.hpp"

namespace px4mqtt {
namespace {

constexpr double kHomeLat = 47.3977, kHomeLon = 8.5456;
constexpr double kHoverAlt = 25.5;
constexpr double kClimbMps = 2.0;      // tốc độ lên/xuống
constexpr double kCruiseMps = 5.0;     // tốc độ ngang
constexpr double kMaxAlt = 120.0;      // giới hạn độ cao
constexpr double kGeofenceM = 1000.0;  // không bay xa điểm xuất phát quá 1 km
constexpr double kMPerDegLat = 111320.0;

double m_per_deg_lon(double lat) { return kMPerDegLat * std::cos(lat * M_PI / 180.0); }

class FakeSource : public TelemetrySource {
 public:
  bool start() override {
    t0_ = last_ = std::chrono::steady_clock::now();
    return true;
  }

  TelemetrySnapshot latest() override {
    std::lock_guard<std::mutex> lk(mu_);
    step();
    TelemetrySnapshot s;
    const bool hovering = !moving_ && alt_ > 0.1;
    // Dao động nhỏ khi đứng yên, giống dữ liệu giả trước đây.
    const double t = std::chrono::duration<double>(last_ - t0_).count();
    s.lat = lat_ + (hovering ? 0.000002 * std::sin(t / 3.0) : 0);
    s.lon = lon_ + (hovering ? 0.000002 * std::cos(t / 3.0) : 0);
    s.rel_alt_m = alt_ + (hovering ? 0.1 * std::sin(t / 2.0) : 0);
    s.ground_mps = speed_ + (hovering ? 0.1 + 0.05 * std::fabs(std::sin(t)) : 0);
    s.battery_pct = battery_;
    s.voltage_v = 13.2 + 3.6 * battery_ / 100.0;  // pin 4S: ~13,2 V lúc cạn, ~16,8 V lúc đầy
    s.valid = true;
    return s;
  }

  std::string execute(const DroneCommand& c) override {
    std::lock_guard<std::mutex> lk(mu_);
    step();
    const bool in_air = alt_ > 0.1;
    if (c.action == "takeoff") {
      if (in_air) return "invalid_state";  // đang bay thì không cất cánh lại
      const double a = std::isnan(c.alt) ? 10.0 : c.alt;
      if (a < 1.0 || a > kMaxAlt) return "out_of_range";
      set_target(lat_, lon_, a);
    } else if (c.action == "land") {
      if (!in_air) return "invalid_state";
      set_target(lat_, lon_, 0.0);
    } else if (c.action == "rtl") {
      if (!in_air) return "invalid_state";
      set_target(kHomeLat, kHomeLon, std::max(alt_, 10.0));
      rtl_land_ = true;  // tới nơi thì tự hạ cánh
    } else if (c.action == "goto") {
      if (!in_air) return "invalid_state";
      if (std::isnan(c.lat) || std::isnan(c.lon)) return "invalid";
      const double a = std::isnan(c.alt) ? alt_ : c.alt;
      if (a < 1.0 || a > kMaxAlt || distance_m(kHomeLat, kHomeLon, c.lat, c.lon) > kGeofenceM)
        return "out_of_range";
      set_target(c.lat, c.lon, a);
    } else if (c.action == "hold") {
      set_target(lat_, lon_, alt_);
    } else {
      return "invalid";
    }
    return "";
  }

 private:
  static double distance_m(double la1, double lo1, double la2, double lo2) {
    return std::hypot((la2 - la1) * kMPerDegLat, (lo2 - lo1) * m_per_deg_lon(la1));
  }

  void set_target(double la, double lo, double a) {
    tlat_ = la;
    tlon_ = lo;
    talt_ = a;
    rtl_land_ = false;
    moving_ = true;
  }

  // Cập nhật trạng thái theo thời gian đã trôi qua kể từ lần trước.
  void step() {
    const auto now = std::chrono::steady_clock::now();
    const double dt = std::chrono::duration<double>(now - last_).count();
    last_ = now;
    if (alt_ > 0.1) battery_ = std::max(0.0, battery_ - 0.05 * dt);  // chỉ hao khi đang bay
    speed_ = 0;
    if (!moving_) return;

    // Ngang: tiến về đích tối đa kCruiseMps * dt mét.
    const double dn = (tlat_ - lat_) * kMPerDegLat, de = (tlon_ - lon_) * m_per_deg_lon(lat_);
    const double d = std::hypot(dn, de), go = std::min(d, kCruiseMps * dt);
    if (d > 1e-3) {
      lat_ += (dn / d) * go / kMPerDegLat;
      lon_ += (de / d) * go / m_per_deg_lon(lat_);
      speed_ = dt > 0 ? go / dt : 0;
    }
    // Đứng: tiến về độ cao đích tối đa kClimbMps * dt mét.
    const double da = talt_ - alt_;
    alt_ += std::clamp(da, -kClimbMps * dt, kClimbMps * dt);

    if (d < 0.5 && std::fabs(talt_ - alt_) < 0.05) {
      if (rtl_land_) {
        set_target(lat_, lon_, 0.0);  // đã về tới nhà: hạ cánh
      } else {
        moving_ = false;
        if (talt_ <= 0.0) alt_ = 0.0;  // đã chạm đất
      }
    }
  }

  std::mutex mu_;
  std::chrono::steady_clock::time_point t0_, last_;
  double lat_ = kHomeLat, lon_ = kHomeLon, alt_ = kHoverAlt;
  double tlat_ = kHomeLat, tlon_ = kHomeLon, talt_ = kHoverAlt;
  double speed_ = 0, battery_ = 100.0;
  bool moving_ = false, rtl_land_ = false;
};

}  // namespace

std::unique_ptr<TelemetrySource> make_fake_source() { return std::make_unique<FakeSource>(); }

}  // namespace px4mqtt
