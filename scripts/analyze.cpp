// Phan B: Phan tich CSV Gateway/Logger, xuat summary va bieu do SVG khong can Python.
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace fs=std::filesystem;
namespace {
struct Key {std::string boot,topic,seq; bool operator<(const Key& rhs)const {return std::tie(boot,topic,seq)<std::tie(rhs.boot,rhs.topic,rhs.seq);} };
struct Result {std::string scenario;int qos=0,run=0;size_t sent=0,received=0,duplicates=0;double rate=0,median=0,p95=0;std::vector<double> latency;};
std::string trim(std::string s) {auto x=s.find_first_not_of(" \r\n\t"); if(x==std::string::npos) return "";auto y=s.find_last_not_of(" \r\n\t");return s.substr(x,y-x+1);}
std::vector<std::string> csv_split(const std::string& s) {
  std::vector<std::string> out;std::string cur;bool quoted=false;
  for(size_t i=0;i<s.size();++i) {char c=s[i];if(c=='"'){if(quoted&&i+1<s.size()&&s[i+1]=='"'){cur+='"';++i;}else quoted=!quoted;}else if(c==','&&!quoted){out.push_back(trim(cur));cur.clear();}else cur+=c;}
  if(quoted) throw std::runtime_error("Unclosed CSV quote");
  out.push_back(trim(cur));
  return out;
}
using Row=std::map<std::string,std::string>;
std::vector<Row> read_csv(const fs::path& file,const std::vector<std::string>& required) {
  std::ifstream in(file);if(!in) throw std::runtime_error("Cannot read: "+file.string());
  std::string line;if(!std::getline(in,line))throw std::runtime_error("Empty CSV: "+file.string());
  auto heads=csv_split(line);if(!heads.empty()&&heads[0].size()>2&&static_cast<unsigned char>(heads[0][0])==0xef)heads[0].erase(0,3);
  for(auto& h:required)if(std::find(heads.begin(),heads.end(),h)==heads.end())throw std::runtime_error("Missing column "+h+" in "+file.string());
  std::vector<Row> rows;size_t rowno=1;
  while(std::getline(in,line)) {++rowno;if(trim(line).empty())continue;auto parts=csv_split(line);if(parts.size()!=heads.size())throw std::runtime_error("CSV columns mismatch at "+file.string()+":"+std::to_string(rowno));Row row;for(size_t i=0;i<heads.size();++i)row[heads[i]]=parts[i];rows.push_back(std::move(row));}
  return rows;
}
long long number(const std::string& s) {size_t len=0;auto x=std::stoll(s,&len);if(len!=s.size())throw std::runtime_error("Invalid integer: "+s);return x;}
double percentile(const std::vector<double>& sorted,double q) {if(sorted.empty())return 0;double r=q*(sorted.size()-1);size_t i=static_cast<size_t>(r);double d=r-i;return sorted[i]*(1-d)+sorted[std::min(i+1,sorted.size()-1)]*d;}
Result calculate(const fs::path& dir,const std::string& scenario,int qos,int run) {
  auto sent=read_csv(dir/"sent.csv",{"boot_id","topic","seq","qos","timestamp_ms"});
  auto got=read_csv(dir/"logger.csv",{"recv_time_ms","topic","boot_id","seq","timestamp_ms"});
  if(sent.empty())throw std::runtime_error("Empty sent.csv in "+dir.string());
  long long start=number(sent.front().at("timestamp_ms"));
  for(auto& r:sent)start=std::min(start,number(r.at("timestamp_ms")));
  const long long cutoff=start+10000; // Bo 10 giay dau theo thoi diem publish
  std::set<Key> expected;
  Result result;result.scenario=scenario;result.qos=qos;result.run=run;
  for(auto& r:sent)if(number(r.at("timestamp_ms"))>=cutoff) {
    if(number(r.at("qos"))!=qos)throw std::runtime_error("sent QoS mismatch in "+dir.string());
    Key k{r.at("boot_id"),r.at("topic"),r.at("seq")};
    if(!expected.insert(k).second)throw std::runtime_error("Duplicated sender identity in "+dir.string());
    result.sent++;
  }
  std::set<Key> received;
  for(auto& r:got) {
    Key k{r.at("boot_id"),r.at("topic"),r.at("seq")};
    if(expected.count(k)==0)continue; // ignore retained/old session/unmatched data
    if(!received.insert(k).second) {result.duplicates++;continue;}
    double latency=static_cast<double>(number(r.at("recv_time_ms"))-number(r.at("timestamp_ms")));
    result.latency.push_back(latency);
  }
  result.received=received.size();
  result.rate=result.sent?100.0*result.received/result.sent:0;
  std::sort(result.latency.begin(),result.latency.end());
  result.median=percentile(result.latency,.5);
  result.p95=percentile(result.latency,.95);
  return result;
}
void csv_header(std::ostream& o) {o<<"scenario,qos,run,sent,received_unique,receive_rate_pct,duplicate_messages,median_ms,p95_ms\n";}
void csv_result(std::ostream& o,const Result& r){o<<r.scenario<<','<<r.qos<<','<<r.run<<','<<r.sent<<','<<r.received<<','<<std::fixed<<std::setprecision(4)<<r.rate<<','<<r.duplicates<<','<<r.median<<','<<r.p95<<'\n';}
std::pair<double,double> mean_sd(std::vector<double> v) {if(v.empty())return {0,0};double m=std::accumulate(v.begin(),v.end(),0.)/v.size();double s=0;for(double x:v)s+=(x-m)*(x-m);return {m,std::sqrt(s/(v.size()>1?v.size()-1:1))};}
void write_svg(const fs::path& target,const std::vector<Result>& results) {
  std::ofstream out(target);if(!out)throw std::runtime_error("Cannot write SVG");
  constexpr int W=1000,H=620,L=90,R=35,T=65,B=85;
  double maxx=1;for(auto& r:results)if(!r.latency.empty())maxx=std::max(maxx,r.latency.back());
  auto xpos=[&](double x){return L+(W-L-R)*std::max(0.,std::min(1.,x/maxx));};
  auto ypos=[&](double y){return H-B-(H-T-B)*y;};
  out<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<W<<"\" height=\""<<H<<"\" viewBox=\"0 0 "<<W<<' '<<H<<"\">\n";
  out<<"<rect width=\"100%\" height=\"100%\" fill=\"white\"/><text x=\"90\" y=\"35\" font-size=\"22\">MQTT latency CDF - Experiment E</text>\n";
  out<<"<g stroke=\"#ddd\">";
  for(int i=0;i<=5;i++){double y=ypos(i/5.);out<<"<line x1=\""<<L<<"\" y1=\""<<y<<"\" x2=\""<<W-R<<"\" y2=\""<<y<<"\"/>";}
  out<<"</g><path d=\"M "<<L<<' '<<T<<" V "<<H-B<<" H "<<W-R<<"\" fill=\"none\" stroke=\"black\"/>\n";
  out<<"<text x=\"450\" y=\"595\" font-size=\"16\">Latency (ms)</text><text x=\"20\" y=\"280\" font-size=\"16\" transform=\"rotate(-90 20 280)\">CDF</text>";
  const std::vector<std::string> colors={"#2459a4","#d04b39","#188f75","#8b4fa4","#b77700","#566675","#e04b88","#4996bb"};
  // Aggregate empirical CDF by scenario+QoS across all runs, with legend.
  std::map<std::pair<std::string,int>,std::vector<double>> by;
  for(auto& r:results){auto& v=by[{r.scenario,r.qos}];v.insert(v.end(),r.latency.begin(),r.latency.end());}
  int idx=0;
  for(auto& [k,v]:by){if(v.empty())continue;std::sort(v.begin(),v.end());std::string color=colors[idx%colors.size()];
    out<<"<polyline fill=\"none\" stroke=\""<<color<<"\" stroke-width=\"2\" points=\"";
    for(size_t i=0;i<v.size();i++){if(i%std::max<size_t>(1,v.size()/2000)!=0 && i+1!=v.size())continue;out<<xpos(v[i])<<','<<ypos(static_cast<double>(i+1)/v.size())<<' ';}
    out<<"\"/>";
    out<<"<rect x=\""<<L+(idx%4)*220<<"\" y=\""<<H-48+(idx/4)*18<<"\" width=\"12\" height=\"4\" fill=\""<<color<<"\"/>";
    out<<"<text x=\""<<L+18+(idx%4)*220<<"\" y=\""<<H-43+(idx/4)*18<<"\" font-size=\"13\">"<<k.first<<" QoS "<<k.second<<"</text>";idx++;
  }
  for(int i=0;i<=5;i++)out<<"<text x=\""<<L-36<<"\" y=\""<<ypos(i/5.)+5<<"\" font-size=\"13\">"<<i/5.<<"</text>";
  for(int i=0;i<=5;i++)out<<"<text x=\""<<xpos(maxx*i/5.)-16<<"\" y=\""<<H-B+24<<"\" font-size=\"13\">"<<std::fixed<<std::setprecision(0)<<maxx*i/5.<<"</text>";
  out<<"</svg>\n";
}
void write_rate_svg(const fs::path& file,const std::vector<Result>& results) {
  std::ofstream f(file);if(!f)throw std::runtime_error("Cannot write SVG");
  const int w=900,h=560,left=85,top=70,bottom=105;
  f<<"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""<<w<<"\" height=\""<<h<<"\">";
  f<<"<rect width=\"100%\" height=\"100%\" fill=\"white\"/>";
  f<<"<text x=\"80\" y=\"37\" font-size=\"23\">MQTT receive rate by network condition</text>";
  for(int tick=0;tick<=5;tick++){
    double rate=tick*20.,y=top+(h-top-bottom)*(1-rate/100.);
    f<<"<line x1=\""<<left<<"\" y1=\""<<y<<"\" x2=\"850\" y2=\""<<y<<"\" stroke=\"#ddd\"/>";
    f<<"<text x=\"36\" y=\""<<y+5<<"\" font-size=\"14\">"<<rate<<"%</text>";
  }
  for(int cond=0;cond<4;cond++){
    std::string name="N"+std::to_string(cond);
    for(int qos=0;qos<2;qos++) {
      std::vector<double> rates;
      for(const auto&r:results)if(r.scenario==name&&r.qos==qos)rates.push_back(r.rate);
      if(rates.empty())continue;
      double avg=mean_sd(rates).first;
      double x=155+cond*175+qos*58;
      double height=(h-top-bottom)*avg/100.,y=h-bottom-height;
      const char* color=qos==0?"#3478b6":"#e28a37";
      f<<"<rect x=\""<<x<<"\" y=\""<<y<<"\" width=\"50\" height=\""<<height<<"\" fill=\""<<color<<"\"/>";
      f<<"<text x=\""<<x<<"\" y=\""<<std::max(65.,y-8)<<"\" font-size=\"12\">"<<std::fixed<<std::setprecision(1)<<avg<<"%</text>";
    }
    f<<"<text x=\""<<170+cond*175<<"\" y=\""<<h-76<<"\" font-size=\"18\">"<<name<<"</text>";
  }
  f<<"<rect x=\"330\" y=\"505\" width=\"14\" height=\"14\" fill=\"#3478b6\"/>";
  f<<"<text x=\"350\" y=\"518\">QoS 0</text><rect x=\"440\" y=\"505\" width=\"14\" height=\"14\" fill=\"#e28a37\"/>";
  f<<"<text x=\"460\" y=\"518\">QoS 1</text></svg>";
}
void usage(){std::cerr<<"Usage: b_analyze --input data/E [--out data/plots] [--strict]\n";}
}
int main(int argc,char**argv) {
  try {
    fs::path base="data/E",output="data/plots";bool strict=false;
    for(int i=1;i<argc;i++) {std::string a=argv[i];if(a=="--input"&&i+1<argc)base=argv[++i];else if(a=="--out"&&i+1<argc)output=argv[++i];else if(a=="--strict")strict=true;else {usage();return 2;}}
    if(!fs::exists(base))throw std::runtime_error("Input path missing: "+base.string());
    fs::create_directories(output);std::vector<Result> results;
    for(int run=1;run<=3;run++)for(const std::string scenario:{"N0","N1","N2","N3"})for(int qos=0;qos<=1;qos++) {
      fs::path dir=base/(scenario+"_qos"+std::to_string(qos)+"_run"+std::to_string(run));
      if(!fs::exists(dir/"sent.csv")||!fs::exists(dir/"logger.csv")) {if(strict)throw std::runtime_error("Missing CSV: "+dir.string());else {std::cerr<<"Skip incomplete: "<<dir<<"\n";continue;}}
      results.push_back(calculate(dir,scenario,qos,run));
    }
    if(results.empty())throw std::runtime_error("No complete experiment directories found");
    std::ofstream per(output/"per_run.csv");csv_header(per);for(auto&r:results){csv_result(per,r);csv_result(std::cout,r);}per.close();
    std::ofstream summary(output/"summary.csv");summary<<"scenario,qos,n,receive_rate_mean_pct,receive_rate_sd_pct,duplicates_mean,duplicates_sd,median_mean_ms,median_sd_ms,p95_mean_ms,p95_sd_ms\n";
    for(const std::string scenario:{"N0","N1","N2","N3"})for(int qos=0;qos<=1;qos++) {
      std::vector<double> rates,dups,med,p95;
      for(auto&r:results)if(r.scenario==scenario&&r.qos==qos){rates.push_back(r.rate);dups.push_back(r.duplicates);med.push_back(r.median);p95.push_back(r.p95);}
      if(rates.empty()) continue;
      auto a=mean_sd(rates),b=mean_sd(dups),c=mean_sd(med),d=mean_sd(p95);
      summary<<scenario<<','<<qos<<','<<rates.size()<<','<<a.first<<','<<a.second<<','<<b.first<<','<<b.second<<','<<c.first<<','<<c.second<<','<<d.first<<','<<d.second<<'\n';
    }
    summary.close();write_svg(output/"cdf.svg",results);write_rate_svg(output/"receive_rate.svg",results);
    std::cout<<"Analyzed "<<results.size()<<" runs. Outputs: "<<output<<"\n";
    if(results.size()!=24)std::cerr<<"WARNING: only "<<results.size()<<"/24 runs (not final experiment E).\n";
  }catch(const std::exception&e){std::cerr<<"Analysis error: "<<e.what()<<"\n";return 1;}
}
