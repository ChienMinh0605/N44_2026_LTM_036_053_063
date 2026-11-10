// Ghi file CSV an toàn khi nhiều luồng cùng ghi (luồng của Paho và luồng chính).
//
// Mở file ở chế độ GHI NỐI TIẾP, không xoá nội dung cũ: ở thí nghiệm F, Logger bị
// dừng rồi chạy lại vào cùng thư mục, dữ liệu trước lúc dừng phải được giữ lại.
// Dòng tiêu đề chỉ ghi khi file còn rỗng.
#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace px4mqtt {

class CsvWriter {
 public:
  CsvWriter() = default;

  CsvWriter(const std::string& path, const std::string& header) { open(path, header); }

  void open(const std::string& path, const std::string& header) {
    namespace fs = std::filesystem;
    const fs::path p(path);
    if (p.has_parent_path()) fs::create_directories(p.parent_path());
    const bool empty = !fs::exists(p) || fs::file_size(p) == 0;
    std::lock_guard<std::mutex> lk(mu_);
    out_.open(path, std::ios::app);
    if (!out_) throw std::runtime_error("Khong mo duoc file " + path);
    if (empty) out_ << header << '\n' << std::flush;
  }

  bool is_open() const { return out_.is_open(); }

  // Ghi một dòng (không kèm ký tự xuống dòng). Flush ngay để Ctrl+C không làm mất dữ liệu.
  void write_line(const std::string& line) {
    std::lock_guard<std::mutex> lk(mu_);
    if (!out_.is_open()) return;
    out_ << line << '\n' << std::flush;
  }

 private:
  std::mutex mu_;
  std::ofstream out_;
};

}  // namespace px4mqtt
