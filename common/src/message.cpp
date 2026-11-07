#include "common/message.hpp"

#include <chrono>

#include "common/config.hpp"

namespace px4mqtt {

using nlohmann::json;

int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string to_payload(const Telemetry& t) {
  json j = {
      {"drone_id", t.drone_id},
      {"boot_id", t.boot_id},
      {"seq", t.seq},
      {"timestamp_ms", t.timestamp_ms},
      {"field", t.field},
      {"value", t.value},
  };
  return j.dump();
}

Telemetry from_payload(const std::string& payload) {
  json j = json::parse(payload);
  Telemetry t;
  t.drone_id = j.at("drone_id").get<std::string>();
  t.boot_id = j.at("boot_id").get<int64_t>();
  t.seq = j.at("seq").get<uint64_t>();
  t.timestamp_ms = j.at("timestamp_ms").get<int64_t>();
  t.field = j.at("field").get<std::string>();
  t.value = j.at("value");
  return t;
}

std::string status_payload(bool online) {
  json j = {{"drone_id", kDroneId}, {"state", online ? "online" : "offline"}};
  return j.dump();
}

}  // namespace px4mqtt
