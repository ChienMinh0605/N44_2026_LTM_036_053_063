// Test phần tính toán của analyze.cpp (thí nghiệm E) bằng CSV nhỏ trong thư mục tạm.
//   ctest --test-dir build -R analyze
#undef NDEBUG  // assert phải chạy cả khi build Release
#define ANALYZE_NO_MAIN
#include "../analyze.cpp"

#include <cassert>

namespace {

const std::string kSentHead = "boot_id,topic,seq,qos,timestamp_ms\n";
const std::string kLogHead = "recv_time_ms,topic,boot_id,seq,timestamp_ms\n";

// Thư mục tạm riêng cho từng ca, xoá khi xong.
struct TempDir {
  fs::path path;
  explicit TempDir(const std::string& name) : path(fs::temp_directory_path() / ("px4mqtt_analyze_" + name)) {
    fs::remove_all(path);
    fs::create_directories(path);
  }
  ~TempDir() { fs::remove_all(path); }
  void write(const std::string& file, const std::string& text) const { std::ofstream(path / file) << text; }
};

template <class F>
bool throws(F f) {
  try {
    f();
  } catch (const std::runtime_error&) {
    return true;
  }
  return false;
}

// Bản tin publish lúc 1000 (bắt đầu), mốc cắt là 1000 + 10000 = 11000.
const std::string kSent = kSentHead +
                          "7,position,0,1,1000\n"     // trong 10 s đầu: bỏ
                          "7,position,1,1,11000\n"    // đúng mốc cắt: tính
                          "7,position,2,1,11200\n"
                          "7,battery,2,1,11200\n"     // cùng seq khác topic: khoá khác
                          "7,battery,3,1,12000\n";    // không tới Logger

void percentile_rule() {
  assert(percentile({}, 95) == 0);
  assert(percentile({5}, 95) == 5);
  std::vector<double> v;
  for (int i = 1; i <= 20; ++i) v.push_back(i);
  assert(percentile(v, 95) == 20);  // phần tử min(19, 20*95/100=19)
  assert(percentile(v, 50) == 11);  // phần tử 10, không nội suy
  v.resize(10);
  assert(percentile(v, 50) == 6);
  assert(percentile(v, 95) == 10);  // 10*95/100 = 9
}

void receive_rate() {
  TempDir d("rate");
  d.write("sent.csv", kSent);
  d.write("logger.csv", kLogHead +
                            "11010,position,7,1,11000\n"
                            "11250,position,7,2,11200\n"
                            "11230,battery,7,2,11200\n");
  const Result r = calculate(d.path, "N0", 1, 1);
  assert(r.sent == 4 && r.received == 3 && r.duplicates == 0);
  assert(r.rate == 75.0);
  assert(r.latency == (std::vector<double>{10, 30, 50}));  // đã sắp xếp
  assert(r.median == 30 && r.p95 == 50);
}

void duplicates() {
  TempDir d("dup");
  d.write("sent.csv", kSent);
  d.write("logger.csv", kLogHead +
                            "11010,position,7,1,11000\n"
                            "11900,position,7,1,11000\n"   // trùng (boot_id, topic, seq): đếm 1 trùng
                            "11930,position,7,1,11000\n");  // lần nữa: 2 trùng
  const Result r = calculate(d.path, "N1", 1, 1);
  assert(r.received == 1 && r.duplicates == 2);
  assert(r.latency.size() == 1 && r.latency[0] == 10);  // độ trễ chỉ lấy bản đầu
}

// Bản tin ngoài tập đã gửi (10 s đầu, phiên Gateway cũ, retained) không tính là nhận hay trùng.
void ignore_unmatched() {
  TempDir d("unmatched");
  d.write("sent.csv", kSent);
  d.write("logger.csv", kLogHead +
                            "1005,position,7,0,1000\n"
                            "11300,position,6,1,11000\n"
                            "11300,position,6,1,11000\n"
                            "11500,velocity,7,1,11000\n");
  const Result r = calculate(d.path, "N0", 1, 1);
  assert(r.sent == 4 && r.received == 0 && r.duplicates == 0 && r.rate == 0);
  assert(r.median == 0 && r.p95 == 0);
}

void cutoff_from_min_timestamp() {
  TempDir d("cutoff");
  // Dòng đầu file không phải dòng sớm nhất: mốc bắt đầu là giá trị nhỏ nhất (500).
  d.write("sent.csv", kSentHead + "7,position,5,0,12000\n7,position,0,0,500\n7,position,1,0,10499\n7,position,2,0,10500\n");
  d.write("logger.csv", kLogHead);
  const Result r = calculate(d.path, "N2", 0, 2);
  assert(r.sent == 2);  // 10500 và 12000
}

void qos_mismatch() {
  TempDir d("qos");
  d.write("sent.csv", kSent);
  d.write("logger.csv", kLogHead);
  assert(throws([&] { calculate(d.path, "N0", 0, 1); }));  // thư mục ghi QoS 1, gọi với QoS 0
}

void duplicate_sender() {
  TempDir d("sender");
  d.write("sent.csv", kSentHead + "7,position,0,1,1000\n7,position,1,1,11000\n7,position,1,1,11000\n");
  d.write("logger.csv", kLogHead);
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
}

void bad_csv() {
  TempDir d("bad");
  d.write("logger.csv", kLogHead);
  d.write("sent.csv", "boot_id,topic,seq,timestamp_ms\n7,position,1,11000\n");  // thiếu cột qos
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
  d.write("sent.csv", kSentHead + "7,position,1,1\n");  // thiếu ô
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
  d.write("sent.csv", kSentHead + "7,position,1,1,11000ms\n");  // thời gian không phải số nguyên
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
  d.write("sent.csv", kSentHead);  // chỉ có tiêu đề
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
  fs::remove(d.path / "logger.csv");
  assert(throws([&] { calculate(d.path, "N0", 1, 1); }));
}

void csv_parsing() {
  assert((csv_split(R"(a,"b,c", d )") == std::vector<std::string>{"a", "b,c", "d"}));
  assert((csv_split(R"("say ""hi""",)") == std::vector<std::string>{"say \"hi\"", ""}));
  assert(throws([] { csv_split(R"(a,"b)"); }));
  TempDir d("bom");  // Excel trên Windows lưu CSV có BOM UTF-8 ở đầu
  d.write("x.csv", "\xEF\xBB\xBF" "seq\r\n3\r\n");
  const auto rows = read_csv(d.path / "x.csv", {"seq"});
  assert(rows.size() == 1 && rows[0].at("seq") == "3");
}

const std::map<std::string, void (*)()> kCases = {
    {"percentile", percentile_rule},
    {"receive_rate", receive_rate},
    {"duplicates", duplicates},
    {"ignore_unmatched", ignore_unmatched},
    {"cutoff", cutoff_from_min_timestamp},
    {"qos_mismatch", qos_mismatch},
    {"duplicate_sender", duplicate_sender},
    {"bad_csv", bad_csv},
    {"csv_parsing", csv_parsing},
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
