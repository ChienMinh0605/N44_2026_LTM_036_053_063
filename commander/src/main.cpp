// Commander: gửi lệnh điều khiển lên drone/001/cmd, chờ ack trên drone/001/cmd/ack,
// đo thời gian khứ hồi (thí nghiệm H). Dùng thay cho nút bấm trên app khi thử và đo.
//
//   ./build/commander/commander --action takeoff --alt 10
//   ./build/commander/commander --action goto --lat 47.3980 --lon 8.5460
//   ./build/commander/commander --action hold --count 100 --interval-ms 200 --qos 1 --out data/H/N0_qos1_run1
//
// Tuỳ chọn để thử từng kiểm tra an toàn của Gateway:
//   --retain        gửi lệnh dạng retained (kiểm tra 1)
//   --age-ms N      ghi sent_ms lùi N ms, giả như lệnh bị giữ lâu trên đường (kiểm tra 2)
//   --repeat-id     gửi mỗi lệnh hai lần cùng cmd_id (kiểm tra 3)
//   --from TEN      tên người điều khiển; hai tên khác nhau để thử quyền điều khiển (kiểm tra 4)

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "common/config.hpp"
#include "common/csv_writer.hpp"
#include "common/message.hpp"
#include "common/mqtt_link.hpp"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop = true; }

struct Args {
  std::string action = "hold", from = "op1", out, scenario = "N0";
  double alt = NAN, lat = NAN, lon = NAN;
  int qos = 1, count = 1, interval_ms = 1000, ttl_ms = px4mqtt::kCmdDefaultTtlMs, run = 1;
  int age_ms = 0, wait_ms = 3000;
  bool retain = false, repeat_id = false;
};

void usage() {
  std::cerr << "Cach dung: commander --action takeoff|land|rtl|goto|hold|release [--alt M]"
               " [--lat D --lon D] [--qos 0|1] [--ttl-ms N] [--count N] [--interval-ms N]"
               " [--from TEN] [--retain] [--age-ms N] [--repeat-id] [--wait-ms N]"
               " [--out THU_MUC] [--scenario N0] [--run 1]\n";
}

Args parse(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) {
        usage();
        std::exit(2);
      }
      return argv[++i];
    };
    if (k == "--action") a.action = next();
    else if (k == "--alt") a.alt = std::stod(next());
    else if (k == "--lat") a.lat = std::stod(next());
    else if (k == "--lon") a.lon = std::stod(next());
    else if (k == "--qos") a.qos = std::stoi(next());
    else if (k == "--ttl-ms") a.ttl_ms = std::stoi(next());
    else if (k == "--count") a.count = std::stoi(next());
    else if (k == "--interval-ms") a.interval_ms = std::stoi(next());
    else if (k == "--from") a.from = next();
    else if (k == "--retain") a.retain = true;
    else if (k == "--age-ms") a.age_ms = std::stoi(next());
    else if (k == "--repeat-id") a.repeat_id = true;
    else if (k == "--wait-ms") a.wait_ms = std::stoi(next());
    else if (k == "--out") a.out = next();
    else if (k == "--scenario") a.scenario = next();
    else if (k == "--run") a.run = std::stoi(next());
    else {
      usage();
      std::exit(2);
    }
  }
  if ((a.qos != 0 && a.qos != 1) || a.count < 1) {
    usage();
    std::exit(2);
  }
  return a;
}

struct Record {
  int64_t sent_ms = 0;
  int64_t ack_recv_ms = -1, gw_recv_ms = -1, gw_ack_ms = -1;
  std::string result = "timeout", reason;
  int acks = 0;  // số ack nhận được cho lệnh này (lớn hơn 1 nếu lệnh bị gửi trùng)
};
}  // namespace

int main(int argc, char** argv) {
  using namespace px4mqtt;
  using nlohmann::json;
  const Args a = parse(argc, argv);
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  std::mutex mu;
  std::map<int64_t, Record> recs;  // theo cmd_id

  LinkOptions lo;
  lo.client_id = "commander-" + a.from + "-" + std::to_string(now_ms());
  lo.clean_session = true;  // công cụ chạy ngắn, không cần Broker giữ phiên
  lo.subscribe_filter = kTopicCmdAck;
  lo.subscribe_qos = 1;
  MqttLink link(lo);
  link.on_message([&](const ReceivedMessage& m) {
    const int64_t recv = now_ms();
    try {
      const json j = json::parse(m.payload);
      if (j.value("from", "") != a.from) return;  // ack của người khác
      std::lock_guard<std::mutex> lk(mu);
      auto it = recs.find(j.at("cmd_id").get<int64_t>());
      if (it == recs.end()) return;
      Record& r = it->second;
      if (++r.acks > 1) return;  // giữ ack đầu tiên
      r.ack_recv_ms = recv;
      r.gw_recv_ms = j.value("gw_recv_ms", int64_t{-1});
      r.gw_ack_ms = j.value("ack_ms", int64_t{-1});
      r.result = j.value("result", "");
      r.reason = j.value("reason", "");
    } catch (const std::exception&) {
    }
  });
  if (!link.connect([] { return g_stop.load(); })) return 1;

  // cmd_id bắt đầu từ giờ hiện tại để mỗi lần chạy không trùng với lần trước.
  const int64_t id0 = now_ms();
  auto next = std::chrono::steady_clock::now();
  for (int i = 0; i < a.count && !g_stop; ++i) {
    const int64_t id = id0 + i;
    json c = {{"cmd_id", id}, {"from", a.from}, {"action", a.action}, {"ttl_ms", a.ttl_ms}};
    if (!std::isnan(a.alt)) c["alt"] = a.alt;
    if (!std::isnan(a.lat)) c["lat"] = a.lat;
    if (!std::isnan(a.lon)) c["lon"] = a.lon;
    const int64_t sent = now_ms();
    c["sent_ms"] = sent - a.age_ms;
    {
      std::lock_guard<std::mutex> lk(mu);
      recs[id].sent_ms = sent;
    }
    link.publish(kTopicCmd, c.dump(), a.qos, a.retain);
    if (a.repeat_id) link.publish(kTopicCmd, c.dump(), a.qos, false);
    next += std::chrono::milliseconds(a.interval_ms);
    if (i + 1 < a.count) std::this_thread::sleep_until(next);
  }

  // Chờ ack cho các lệnh cuối.
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(a.wait_ms);
  while (!g_stop && std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard<std::mutex> lk(mu);
      const bool all = std::all_of(recs.begin(), recs.end(), [&](const auto& p) {
        return p.second.acks >= (a.repeat_id ? 2 : 1);
      });
      if (all) break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (a.retain)  // xoá lệnh retained khỏi Broker, tránh để lại "mìn" cho lần chạy sau
    link.publish(kTopicCmd, "", 1, true);
  link.disconnect();

  // In kết quả và ghi CSV.
  std::lock_guard<std::mutex> lk(mu);
  std::map<std::string, int> tally;
  std::vector<int64_t> rtts;
  CsvWriter csv;
  if (!a.out.empty())
    csv.open(a.out + "/cmd_rtt.csv",
             "cmd_id,action,qos,sent_ms,ack_recv_ms,rtt_ms,down_ms,up_ms,result,reason,acks,"
             "scenario,run");
  for (const auto& [id, r] : recs) {
    const bool ok = r.ack_recv_ms >= 0;
    const int64_t rtt = ok ? r.ack_recv_ms - r.sent_ms : -1;
    // down: người gửi -> Gateway; up: Gateway gửi ack -> người gửi (cùng đồng hồ máy)
    const int64_t down = ok ? r.gw_recv_ms - r.sent_ms : -1;
    const int64_t up = ok ? r.ack_recv_ms - r.gw_ack_ms : -1;
    ++tally[r.result + (r.reason.empty() ? "" : ":" + r.reason)];
    if (ok) rtts.push_back(rtt);
    if (a.count <= 5)
      std::cout << "cmd_id=" << id << " " << a.action << " -> " << r.result
                << (r.reason.empty() ? "" : " (" + r.reason + ")") << ", khu hoi " << rtt
                << " ms" << (r.acks > 1 ? ", nhan " + std::to_string(r.acks) + " ack" : "")
                << "\n";
    if (csv.is_open())
      csv.write_line(std::to_string(id) + "," + a.action + "," + std::to_string(a.qos) + "," +
                     std::to_string(r.sent_ms) + "," + std::to_string(r.ack_recv_ms) + "," +
                     std::to_string(rtt) + "," + std::to_string(down) + "," +
                     std::to_string(up) + "," + r.result + "," + r.reason + "," +
                     std::to_string(r.acks) + "," + a.scenario + "," + std::to_string(a.run));
  }
  std::cout << "Tong: gui " << recs.size() << " lenh |";
  for (const auto& [k, v] : tally) std::cout << " " << k << "=" << v;
  if (!rtts.empty()) {
    std::sort(rtts.begin(), rtts.end());
    std::cout << " | khu hoi trung vi " << rtts[rtts.size() / 2] << " ms, p95 "
              << rtts[std::min(rtts.size() - 1, rtts.size() * 95 / 100)] << " ms";
  }
  std::cout << "\n";
  return 0;
}
