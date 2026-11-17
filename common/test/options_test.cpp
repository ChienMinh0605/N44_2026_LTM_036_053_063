// Test tham số dòng lệnh (options.hpp), không cần Broker hay PX4.
//   ctest --test-dir build -R options
// Tham số sai làm parse_options gọi std::exit(2): các ca đó chạy
// "options_test parse <tham số>" và CMake đánh dấu WILL_FAIL.
#undef NDEBUG  // assert phải chạy cả khi build Release
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "common/options.hpp"

using namespace px4mqtt;

namespace {

Options parse(std::vector<std::string> args) {
  args.insert(args.begin(), "options_test");
  std::vector<char*> argv;
  for (auto& a : args) argv.push_back(a.data());
  return parse_options(static_cast<int>(argv.size()), argv.data());
}

void defaults() {
  const Options o = parse({});
  assert(o.qos == 1 && o.buffer && o.cmd_safety && !o.clean);
  assert(o.scenario == "N0" && o.run == 1 && o.out_dir == "data");
}

void all_flags() {
  const Options o = parse({"--qos", "0", "--buffer", "off", "--cmd-safety", "off", "--clean",
                           "--scenario", "N3", "--run", "3", "--out", "data/E/N3_qos0_run3"});
  assert(o.qos == 0 && !o.buffer && !o.cmd_safety && o.clean);
  assert(o.scenario == "N3" && o.run == 3 && o.out_dir == "data/E/N3_qos0_run3");
}

// Chỉ đúng chữ "on" mới bật; giá trị khác (kể cả "ON") là tắt.
void on_off() {
  assert(parse({"--buffer", "on"}).buffer);
  assert(!parse({"--buffer", "ON"}).buffer);
  assert(parse({"--cmd-safety", "on"}).cmd_safety);
}

void last_wins() {
  assert(parse({"--qos", "0", "--qos", "1"}).qos == 1);
}

const std::map<std::string, void (*)()> kCases = {
    {"defaults", defaults}, {"all_flags", all_flags}, {"on_off", on_off}, {"last_wins", last_wins},
};

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 2 && std::string(argv[1]) == "parse") {
    parse_options(argc - 1, argv + 1);  // tham số sai: thoát mã 2 ngay trong hàm này
    return 0;
  }
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
