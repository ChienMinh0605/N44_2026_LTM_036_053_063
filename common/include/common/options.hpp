// Tham số dòng lệnh chung cho Gateway và Logger, để script thí nghiệm
// điều khiển được mà không phải sửa code (mục 5 của kế hoạch).
//
//   --source px4|fake   (Gateway) lấy dữ liệu từ PX4 hay sinh dữ liệu giả
//   --qos 0|1           mức QoS khi publish / subscribe
//   --buffer on|off     (Gateway) bật/tắt bộ đệm khi mất kết nối
//   --clean             (Logger) chỉ kết nối với clean_session = true để xóa phiên cũ rồi thoát
//   --scenario N0..N3   tên điều kiện mạng, ghi vào CSV
//   --run <số>          lần chạy thứ mấy, ghi vào CSV
//   --out <thư mục>     nơi ghi file CSV (mặc định: data)
#pragma once

#include <cstdlib>
#include <iostream>
#include <string>

namespace px4mqtt {

struct Options {
  std::string source = "px4";
  int qos = 1;
  bool buffer = true;
  bool clean = false;
  std::string scenario = "N0";
  int run = 1;
  std::string out_dir = "data";
};

inline void print_usage(const char* prog) {
  std::cerr << "Cach dung: " << prog
            << " [--source px4|fake] [--qos 0|1] [--buffer on|off] [--clean]"
               " [--scenario N0] [--run 1] [--out data]\n";
}

inline Options parse_options(int argc, char** argv) {
  Options o;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        std::cerr << "Thieu gia tri cho " << a << "\n";
        print_usage(argv[0]);
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--source") {
      o.source = next();
    } else if (a == "--qos") {
      o.qos = std::stoi(next());
    } else if (a == "--buffer") {
      o.buffer = (next() == "on");
    } else if (a == "--clean") {
      o.clean = true;
    } else if (a == "--scenario") {
      o.scenario = next();
    } else if (a == "--run") {
      o.run = std::stoi(next());
    } else if (a == "--out") {
      o.out_dir = next();
    } else {
      std::cerr << "Tham so khong hop le: " << a << "\n";
      print_usage(argv[0]);
      std::exit(2);
    }
  }
  if ((o.qos != 0 && o.qos != 1) || (o.source != "px4" && o.source != "fake")) {
    print_usage(argv[0]);
    std::exit(2);
  }
  return o;
}

}  // namespace px4mqtt
