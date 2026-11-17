// Test 4 kiểm tra an toàn của kênh lệnh (command_handler.cpp), không cần Broker.
//   cmake --build build && ctest --test-dir build
#undef NDEBUG  // assert phải chạy cả khi build Release
#include <algorithm>
#include <cassert>
#include <string>

#include "command_handler.hpp"
#include "telemetry_source.hpp"

using namespace px4mqtt;

// Drone thay cho PX4 trong test: lệnh nào qua được 4 kiểm tra cũng thực hiện được.
struct AcceptAll : TelemetrySource {
  bool start() override { return true; }
  TelemetrySnapshot latest() override { return {}; }
  std::string execute(const DroneCommand&) override { return ""; }
};

int main() {
  AcceptAll drone;
  CommandHandler h(true, drone);
  const int64_t t = 1'000'000;
  auto cmd = [](const std::string& from, int id, int64_t sent) {
    return R"({"from":")" + from + R"(","cmd_id":)" + std::to_string(id) +
           R"(,"action":"hold","sent_ms":)" + std::to_string(sent) + "}";
  };
  auto reason = [&](const std::string& payload, int64_t recv, bool retained = false) {
    return h.handle(payload, recv, retained, false).ack["reason"].get<std::string>();
  };

  assert(reason(cmd("op1", 1, t), t) == "");                     // hợp lệ: chấp nhận, op1 giữ quyền
  assert(reason(cmd("op1", 2, t), t, true) == "retained");       // kiểm tra 1
  assert(reason(cmd("op1", 3, t - 4000), t) == "expired");       // kiểm tra 2: quá TTL mặc định 3 s
  assert(reason(cmd("op1", 1, t), t) == "duplicate");            // kiểm tra 3: trùng cmd_id
  assert(reason(cmd("op2", 1, t + 100), t + 100) == "not_owner"); // kiểm tra 4: op1 còn giữ quyền
  assert(reason(cmd("op2", 2, t + 6000), t + 6000) == "");       // op1 im lặng quá 5 s: quyền được nhả
  assert(reason("not json", t) == "invalid");

  // Tên người gửi có dấu phẩy không được làm lệch cột cmd_log.csv (10 cột = 9 dấu phẩy)
  const std::string line = h.handle(cmd("a,b", 7, t), t, false, false).log_line;
  assert(std::count(line.begin(), line.end(), ',') == 9);

  CommandHandler unsafe(false, drone);  // tắt kiểm tra (thí nghiệm H): lệnh retained vẫn chạy
  assert(unsafe.handle(cmd("op1", 9, t), t, true, false).ack["reason"] == "");
  return 0;
}
