// Experiment E runner (Linux only): orchestrates existing A's Broker/Logger/Gateway.
// Runs one condition per invocation; does not modify network beyond netem.sh.
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

namespace fs=std::filesystem;
namespace {
volatile sig_atomic_t interrupted=0;
void signal_handler(int){interrupted=1;}
std::string root;
std::string netem;
int run_sync(const std::vector<std::string>& args,const std::string& redirect="") {
  pid_t p=fork();if(p<0)throw std::runtime_error("fork failed");
  if(p==0){
    if(!redirect.empty()){
      int fd=open(redirect.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
      if(fd<0)_exit(126);
      dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);
    }
    std::vector<char*> av;for(const auto& s:args)av.push_back(const_cast<char*>(s.c_str()));av.push_back(nullptr);
    execvp(av[0],av.data());_exit(127);
  }
  int status=0;while(waitpid(p,&status,0)<0){if(errno==EINTR)continue;throw std::runtime_error("waitpid failed");}
  return WIFEXITED(status)?WEXITSTATUS(status):128+(WIFSIGNALED(status)?WTERMSIG(status):0);
}
pid_t spawn(const std::vector<std::string>& args,const fs::path& logfile){
  pid_t p=fork();if(p<0)throw std::runtime_error("fork failed");
  if(p==0){
    setpgid(0,0);
    int fd=open(logfile.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd<0)_exit(126);
    dup2(fd,STDOUT_FILENO);dup2(fd,STDERR_FILENO);close(fd);
    std::vector<char*>av;for(auto&s:args)av.push_back(const_cast<char*>(s.c_str()));av.push_back(nullptr);
    execvp(av[0],av.data());_exit(127);
  }
  setpgid(p,p);return p;
}
void stop_child(pid_t& p){
  if(p<=0)return;
  if(kill(-p,SIGINT)!=0)kill(p,SIGINT);
  for(int i=0;i<40;i++){int status;pid_t r=waitpid(p,&status,WNOHANG);if(r==p||r<0){p=-1;return;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
  kill(-p,SIGTERM);kill(p,SIGTERM);waitpid(p,nullptr,0);p=-1;
}
struct Guard {
  pid_t logger=-1,gateway=-1;
  ~Guard(){stop_child(gateway);stop_child(logger);if(!netem.empty()) {try {run_sync({netem,"N0"});}catch(...) {}}}
};
void wait_for(int seconds,pid_t logger=-1,pid_t gateway=-1) {
  auto until=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
  while(!interrupted&&std::chrono::steady_clock::now()<until){
    for(auto p:{logger,gateway})if(p>0){int status=0;if(waitpid(p,&status,WNOHANG)==p)throw std::runtime_error("Process terminated unexpectedly: pid "+std::to_string(p));}
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if(interrupted)throw std::runtime_error("Experiment interrupted");
}
}
int main(int argc,char**argv){
  try {
    if(argc!=4){std::cerr<<"Usage: b_experiment_runner N0|N1|N2|N3 0|1 run_number\n";return 2;}
    std::string scenario=argv[1],qos=argv[2],run=argv[3];
    if((scenario!="N0"&&scenario!="N1"&&scenario!="N2"&&scenario!="N3")||(qos!="0"&&qos!="1")||run.empty()||run.find_first_not_of("0123456789")!=std::string::npos||std::stoi(run)<1)throw std::runtime_error("Invalid scenario/qos/run");
    std::signal(SIGINT,signal_handler);std::signal(SIGTERM,signal_handler);
    fs::path cwd=fs::current_path();root=cwd.string();
    netem=(cwd/"scripts/netem.sh").string();
    fs::path logger=(cwd/"build/logger/logger"), gateway=(cwd/"build/gateway/gateway");
    if(!fs::exists(logger)||!fs::exists(gateway)||!fs::exists(netem))throw std::runtime_error("Run from repo root, build gateway/logger first");
    fs::path dir=cwd/"data/E"/(scenario+"_qos"+qos+"_run"+run);
    if(fs::exists(dir/"sent.csv")||fs::exists(dir/"logger.csv"))throw std::runtime_error("Refusing to append to existing run: "+dir.string());
    fs::create_directories(dir);
    Guard guard;
    // Step 1: fresh PID via netem.sh; reset previous condition
    if(run_sync({netem,"N0"})!=0)throw std::runtime_error("Broker/netem not ready");
    // Step 2: remove stale MQTT persistent session BEFORE applying netem
    if(run_sync({logger.string(),"--clean"},(dir/"clean.log").string())!=0)throw std::runtime_error("Logger --clean failed");
    // Step 3: apply current network condition, verify qdisc
    if(run_sync({netem,scenario},(dir/"netem.txt").string())!=0)throw std::runtime_error("netem failed: check sudo privileges / kernel support");
    if(run_sync({netem,"show"},(dir/"netem_verify.txt").string())!=0)throw std::runtime_error("netem verify failed");
    // Step 4: 50 ICMP packets; output saved, not fatal for some hosts that block ping
    int ping=run_sync({"ping","-c","50","-i","0.1","172.28.0.10"},(dir/"ping.txt").string());
    if(ping!=0)std::cerr<<"WARNING ping failed; review "<<(dir/"ping.txt")<<"\n";
    // Step 5: Logger starts first and subscribes before gateway publishes
    std::string out=dir.string();
    guard.logger=spawn({logger.string(),"--qos",qos,"--scenario",scenario,"--run",run,"--out",out},dir/"logger.log");
    wait_for(3,guard.logger);
    guard.gateway=spawn({gateway.string(),"--source","fake","--qos",qos,"--buffer","on","--scenario",scenario,"--run",run,"--out",out},dir/"gateway.log");
    // Step 6: 180 seconds publisher; logger remains running 15 sec after gateway stops
    wait_for(180,guard.logger,guard.gateway);
    stop_child(guard.gateway);
    wait_for(15,guard.logger);
    stop_child(guard.logger);
    // Guard will restore N0 even if exceptions occur
    std::ofstream summary(dir/"scenario.txt");summary<<"scenario="<<scenario<<"\nqos="<<qos<<"\nrun="<<run<<"\npublish_duration_sec=180\nlogger_extra_sec=15\n";
    std::cout<<"Completed: "<<dir<<"\n";
    return 0;
  }catch(const std::exception&e){std::cerr<<"Experiment failed: "<<e.what()<<"\n";return 1;}
}
