// B: Publisher gia lap DOC LAP, cho Flutter app thu MQTT truoc khi Gateway san sang.
// De do E, su dung gateway --source fake vi Gateway ghi sent.csv va events.csv.
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>
#include "common/config.hpp"
#include "common/message.hpp"
#include "common/mqtt_link.hpp"

namespace {
std::atomic<bool> stop_flag{false};
void stop_handler(int) { stop_flag.store(true); }
int number_arg(const std::string &s) { size_t n=0; int v=std::stoi(s,&n); if(n!=s.size()) throw std::invalid_argument("invalid number"); return v; }
}

int main(int argc, char** argv) {
  using namespace px4mqtt;
  try {
    int qos=1, duration=60;
    for(int i=1;i<argc;i++) {
      std::string a=argv[i];
      if((a=="--qos" || a=="--duration") && i+1<argc) {
        int v=number_arg(argv[++i]); if(a=="--qos") qos=v; else duration=v;
      } else if(a=="--help") { std::cout<<"Usage: b_fake_publisher [--qos 0|1] [--duration seconds] (0=until Ctrl+C)\n"; return 0; }
      else { std::cerr<<"Invalid argument: "<<a<<"\n"; return 2; }
    }
    if((qos!=0&&qos!=1)||duration<0) return 2;
    std::signal(SIGINT,stop_handler);
    std::signal(SIGTERM,stop_handler);
    const int64_t boot=now_ms();
    LinkOptions opts;
    opts.client_id=std::string("b-fake-")+std::to_string(boot);
    opts.clean_session=true;
    opts.buffer_offline=true;
    opts.will_topic=kTopicStatus;
    opts.will_payload=status_payload(false);
    MqttLink mqtt(opts);
    mqtt.on_connected([&mqtt] { mqtt.publish(kTopicStatus,status_payload(true),1,true); });
    std::cout<<"B fake publisher: broker="<<kBrokerUri<<", qos="<<qos<<"\n";
    if(!mqtt.connect([]{return stop_flag.load();})) return 1;
    std::array<const char*,4> topics={kTopicPosition,kTopicAltitude,kTopicVelocity,kTopicBattery};
    std::array<const char*,4> fields={"position","altitude","velocity","battery"};
    std::array<uint64_t,4> seq={0,0,0,0};
    const auto start=std::chrono::steady_clock::now();
    auto next=start;
    unsigned long total=0;
    while(!stop_flag && (duration==0 || std::chrono::steady_clock::now()-start<std::chrono::seconds(duration))) {
      const double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
      std::array<nlohmann::json,4> value={
        nlohmann::json{{"lat",47.3977+0.000002*std::sin(sec/3)},{"lon",8.5456+0.000002*std::cos(sec/3)}},
        nlohmann::json{{"rel_m",25.5+0.1*std::sin(sec/2)}},
        nlohmann::json{{"ground_mps",0.1+0.05*std::abs(std::sin(sec))}},
        nlohmann::json{{"percent",std::max(0.0,87.0-sec*0.01)},{"voltage",15.8}}
      };
      for(size_t i=0;i<4;i++) {
        Telemetry t;
        t.drone_id=kDroneId; t.boot_id=boot; t.seq=seq[i]++; t.timestamp_ms=now_ms();
        t.field=fields[i]; t.value=value[i];
        auto st=mqtt.publish(topics[i],to_payload(t),qos);
        if(st==SendStatus::Dropped) std::cerr<<"Dropped: "<<topics[i]<<" seq="<<t.seq<<"\n";
        ++total;
      }
      if(total%100==0) std::cout<<"Published attempts="<<total<<"\n";
      next+=std::chrono::milliseconds(kPublishPeriodMs);
      std::this_thread::sleep_until(next);
    }
    if(mqtt.is_connected()) mqtt.publish(kTopicStatus,status_payload(false),1,true);
    mqtt.disconnect();
    std::cout<<"Finished: "<<total<<" telemetry messages attempted\n";
  } catch(const std::exception& e) {std::cerr<<"Error: "<<e.what()<<"\n"; return 1;}
  return 0;
}
