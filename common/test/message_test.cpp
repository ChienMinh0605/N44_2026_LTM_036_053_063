// Test định dạng JSON telemetry (message.cpp), không cần Broker hay PX4.
//   ctest --test-dir build -R message
// Chạy một ca: build/common/message_test <tên ca>
#undef NDEBUG  // assert phải chạy cả khi build Release
#include <cassert>
#include <iostream>
#include <map>
#include <string>

#include "common/message.hpp"

using namespace px4mqtt;
using nlohmann::json;

namespace {

Telemetry sample() {
  Telemetry t;
  t.drone_id = "001";
  t.boot_id = 1793541590000;  // Unix ms thật, vượt 32 bit
  t.seq = 125;
  t.timestamp_ms = 1793541600000;
  t.field = "battery";
  t.value = {{"percent", 87.5}, {"voltage", 15.8}};
  return t;
}

bool throws(const std::string& payload) {
  try {
    from_payload(payload);
  } catch (const json::exception&) {
    return true;
  }
  return false;
}

void roundtrip() {
  const Telemetry a = sample();
  const Telemetry b = from_payload(to_payload(a));
  assert(b.drone_id == a.drone_id);
  assert(b.boot_id == a.boot_id);
  assert(b.seq == a.seq);
  assert(b.timestamp_ms == a.timestamp_ms);
  assert(b.field == a.field);
  assert(b.value == a.value);
}

// App Flutter và Logger đọc đúng 6 khoá này, đổi tên là hỏng phía nhận.
void keys() {
  const json j = json::parse(to_payload(sample()));
  assert(j.size() == 6);
  for (const char* k : {"drone_id", "boot_id", "seq", "timestamp_ms", "field", "value"}) assert(j.contains(k));
  assert(j["value"]["percent"] == 87.5);
}

void malformed() {
  assert(throws("not json"));
  assert(throws(""));
  assert(throws(R"({"drone_id":"001","seq":)"));  // bị cắt giữa chừng
}

void missing_field() {
  json j = json::parse(to_payload(sample()));
  j.erase("seq");
  assert(throws(j.dump()));
}

void wrong_type() {
  json j = json::parse(to_payload(sample()));
  j["seq"] = "125";  // chuỗi thay cho số
  assert(throws(j.dump()));
  assert(throws("[1,2,3]"));  // JSON hợp lệ nhưng không phải object
}

void status() {
  const json on = json::parse(status_payload(true));
  const json off = json::parse(status_payload(false));
  assert(on["drone_id"] == "001" && on["state"] == "online");
  assert(off["state"] == "offline");
}

const std::map<std::string, void (*)()> kCases = {
    {"roundtrip", roundtrip}, {"keys", keys},       {"malformed", malformed},
    {"missing_field", missing_field}, {"wrong_type", wrong_type}, {"status", status},
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    for (auto& [name, fn] : kCases) fn();
    return 0;
  }
  auto it = kCases.find(argv[1]);
  if (it == kCases.end()) {
    std::cerr << "Khong co ca test: " << argv[1] << "\n";
    return 1;
  }
  it->second();
  return 0;
}
