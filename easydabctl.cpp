// EasyDAB emergency CLI, C++17 / libcurl
// Protocol references: p_066000.js, p_064000.js, p_062000.js
#include <curl/curl.h>
#include <unistd.h>
#include <termios.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>

using Bytes=std::vector<uint8_t>;
struct Options {
 std::string host="10.26.76.125", user="admin", password, password_env;
 int timeout=3; bool yes=false,json=false,raw=false;
 std::vector<std::string> cmd;
};
static size_t collect(char *p,size_t a,size_t b,void *ud) {
 auto *v=static_cast<Bytes*>(ud); const size_t n=a*b;
 if(n>65536 || v->size()+n>65536) return 0;
 v->insert(v->end(),reinterpret_cast<uint8_t*>(p),reinterpret_cast<uint8_t*>(p)+n);
 return n;
}
static std::string getpass_tty(const std::string &label) {
 FILE *tty=fopen("/dev/tty","r+");
 if(!tty) throw std::runtime_error("No TTY to prompt for password; use --password-env NAME");
 int fd=fileno(tty); termios old{}, now{};
 if(tcgetattr(fd,&old)!=0){fclose(tty);throw std::runtime_error("tcgetattr failed");}
 now=old;now.c_lflag &= ~ECHO;
 fprintf(tty,"%s",label.c_str());fflush(tty);
 if(tcsetattr(fd,TCSAFLUSH,&now)!=0){fclose(tty);throw std::runtime_error("tcsetattr failed");}
 std::string s; int c;
 while((c=fgetc(tty))!=EOF && c!='\n' && c!='\r') {
  if(s.size()>=1024) break;
  s.push_back(static_cast<char>(c));
 }
 tcsetattr(fd,TCSAFLUSH,&old); fprintf(tty,"\n");fclose(tty);
 return s;
}
static void usage(){
 std::cout<<"EasyDAB emergency CLI (no flash writes)\n"
 "Usage: easydabctl [--host IP] [--user USER] [--password-env VAR] [--timeout SEC] [--json] [--raw] [--yes] COMMAND\n"
 "Commands:\n"
 "  status                 Read RF/ETI/buffer/flags\n"
 "  amplitude              Read DAC amplitude\n"
 "  config                 Read non-secret network/ETI settings\n"
 "  rf on|off              Enable/disable RF frontend (changes device)\n"
 "  eti client|server|off  Enable corresponding ETI socket state (changes device)\n"
 "  eti reconnect          Reconnect stored TCP-client configuration (changes device)\n"
 "  amplitude set 1..255   Set DAC amplitude runtime only (changes device)\n"
 "  restart                Restart EasyDAB (changes device)\n"
 "All writes require --yes. No configuration flash erase or POST is implemented.\n"
 "--password-env NAME reads the password from an environment variable; otherwise prompts via /dev/tty.\n";
}
static Options parse(int argc,char**argv){
 Options o;
 for(int i=1;i<argc;i++){
  std::string a=argv[i];
  auto value=[&]()->std::string{if(++i>=argc)throw std::runtime_error("Missing value for "+a);return argv[i];};
  if(a=="--host")o.host=value();
  else if(a=="--user")o.user=value();
  else if(a=="--password-env")o.password_env=value();
  else if(a=="--timeout") {auto v=value();size_t n=0;int t=std::stoi(v,&n);if(n!=v.size()||t<1||t>30)throw std::runtime_error("Timeout must be 1..30");o.timeout=t;}
  else if(a=="--yes")o.yes=true;
  else if(a=="--json")o.json=true;
  else if(a=="--raw")o.raw=true;
  else if(a=="-h"||a=="--help"){usage();std::exit(0);}
  else if(!a.empty()&&a[0]=='-')throw std::runtime_error("Unknown option: "+a);
  else o.cmd.push_back(a);
 }
 if(o.cmd.empty())throw std::runtime_error("Missing command (try --help)");
 if(o.host.empty()||o.host.find_first_not_of("0123456789.")!=std::string::npos || o.host.size()>15)
  throw std::runtime_error("--host must be an IPv4 address");
 if(o.user.empty() || o.user.find_first_of("\r\n:")!=std::string::npos)throw std::runtime_error("Invalid username");
 if(!o.password_env.empty()){
  const char *p=std::getenv(o.password_env.c_str());
  if(!p)throw std::runtime_error("Password environment variable not set");
  o.password=p;
 } else o.password=getpass_tty("EasyDAB password: ");
 return o;
}
static Bytes fetch(const Options &o, const std::string &path){
 CURL *curl=curl_easy_init();if(!curl)throw std::runtime_error("curl_easy_init failed");
 Bytes data; long code=0;
 std::string url="http://"+o.host+"/"+path;
 curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
 curl_easy_setopt(curl,CURLOPT_USERNAME,o.user.c_str());
 curl_easy_setopt(curl,CURLOPT_PASSWORD,o.password.c_str());
 curl_easy_setopt(curl,CURLOPT_HTTPAUTH,CURLAUTH_BASIC);
 curl_easy_setopt(curl,CURLOPT_HTTP_VERSION,CURL_HTTP_VERSION_1_0);
 curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,2L);
 curl_easy_setopt(curl,CURLOPT_TIMEOUT,static_cast<long>(o.timeout));
 curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
 curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
 curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,collect);
 curl_easy_setopt(curl,CURLOPT_WRITEDATA,&data);
 curl_easy_setopt(curl,CURLOPT_USERAGENT,"easydabctl/0.1");
 curl_slist *h=nullptr;h=curl_slist_append(h,"Connection: close");
 curl_easy_setopt(curl,CURLOPT_HTTPHEADER,h);
 const CURLcode rc=curl_easy_perform(curl);
 curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
 std::string err=curl_easy_strerror(rc);
 curl_slist_free_all(h);curl_easy_cleanup(curl);
 if(rc!=CURLE_OK)throw std::runtime_error("HTTP request to /"+path+" failed: "+err+" (write outcome may be uncertain)");
 if(code!=200)throw std::runtime_error("HTTP /"+path+" returned status "+std::to_string(code));
 return data;
}
static void hexdump(const Bytes &b) {
 std::cout<<"RAW ["<<b.size()<<" bytes]:";
 for(uint8_t v:b)std::cout<<' '<<std::hex<<std::setfill('0')<<std::setw(2)<<(unsigned)v;
 std::cout<<std::dec<<"\n";
}
static std::string rfstate(uint8_t r){if(r&0x50)return "OFF";if(r==2)return "ON";return "UNKNOWN";}
static std::string etistate(const Bytes&d) {
 auto m=d[9],a=d[10],s=d[11];
 if((m==2||m==4)&&s==0x17)return "CONNECTED";
 if(m==0||m==1)return a==1?"ADMIN_OFF":"FLASH_OFF";
 if(m==2&&s==0x14)return "LISTENING";
 if(m==4&&(s==0x13||s==0))return "CONNECTING";
 return "UNKNOWN";
}
static std::string jsonescape(const std::string &s){std::string r="\"";for(char c:s){if(c=='"'||c=='\\')r+='\\';r+=c;}return r+'"';}
static Bytes status_data(const Options &o){auto d=fetch(o,"s_000000");if(d.size()!=14)throw std::runtime_error("Invalid status size: "+std::to_string(d.size())+" (expected 14)");return d;}
static void status(const Options&o){auto d=status_data(o);unsigned f=d[13];
 if(o.json){
  std::cout<<"{\"rf\":"<<jsonescape(rfstate(d[4]))<<",\"eti\":"<<jsonescape(etistate(d))
   <<",\"eti_mode\":"<<(unsigned)d[9]<<",\"eti_admin\":"<<(unsigned)d[10]
   <<",\"tcp_status\":"<<(unsigned)d[11]<<",\"buffer_percent\":"<<std::fixed<<std::setprecision(1)<<d[12]*100.0/255
   <<",\"broadcasting\":"<<((f&0x40)?"true":"false")<<",\"spi_sram\":"<<((f&0x80)?"true":"false")
   <<",\"ifft_overflow\":"<<((f&1)?"true":"false")<<",\"fir_clipping\":"<<((f&2)?"true":"false")
   <<",\"ifft_overflow_history\":"<<((f&4)?"true":"false")<<",\"fir_clipping_history\":"<<((f&8)?"true":"false")<<"}\n";
 } else {
 std::cout<<"RF:           "<<rfstate(d[4])<<" [0x"<<std::hex<<(unsigned)d[4]<<std::dec<<"]\n"
  <<"ETI:          "<<etistate(d)<<" [mode="<<(unsigned)d[9]<<", admin="<<(unsigned)d[10]<<", TCP=0x"<<std::hex<<(unsigned)d[11]<<std::dec<<"]\n"
  <<"ETI buffer:   "<<std::fixed<<std::setprecision(1)<<d[12]*100.0/255<<" %\n"
  <<"Broadcasting: "<<((f&0x40)?"ON":"OFF")<<"\nSPI SRAM:     "<<((f&0x80)?"DETECTED":"NO")
  <<"\niFFT overflow: "<<((f&1)?"ACTIVE":((f&4)?"HISTORY":"NO"))
  <<"\nFIR clipping: "<<((f&2)?"ACTIVE":((f&8)?"HISTORY":"NO"))<<"\n";
 if(o.raw)hexdump(d);
 }
}
static void amplitude(const Options&o){auto d=fetch(o,"s_000e00");if(d.size()!=14)throw std::runtime_error("Invalid amplitude reply size: "+std::to_string(d.size()));
 if(o.json)std::cout<<"{\"dac_amplitude\":"<<(unsigned)d[2]<<"}\n";
 else {std::cout<<"DAC amplitude: "<<(unsigned)d[2]<<" / 255 (NOT RF watts)\n";if(o.raw)hexdump(d);}
}
static void config(const Options&o){auto d=fetch(o,"b_070000");if(d.size()<222)throw std::runtime_error("Invalid config reply: too short");
 std::ostringstream ip;ip<<(unsigned)d[22]<<'.'<<(unsigned)d[23]<<'.'<<(unsigned)d[24]<<'.'<<(unsigned)d[25];
 const int port=(d[26]<<8)|d[27];
 std::string mode=d[28]==4?"CLIENT":d[28]==2?"SERVER":d[28]==0?"OFF":d[28]==0x10?"OFF?":"UNKNOWN";
 if(o.json)std::cout<<"{\"remote_ip\":"<<jsonescape(ip.str())<<",\"remote_port\":"<<port<<",\"mode\":"<<jsonescape(mode)
  <<",\"mode_raw\":"<<(unsigned)d[28]<<",\"zmq_enabled\":"<<(d[38]==1?"true":"false")
  <<",\"zmq_raw\":"<<(unsigned)d[38]<<",\"tcp_nodelay\":"<<((d[19]&0x20)?"true":"false")<<"}\n";
 else std::cout<<"Remote endpoint: "<<ip.str()<<":"<<port<<"\nMode: "<<mode<<" (raw 0x"<<std::hex<<(unsigned)d[28]<<std::dec<<")\n"
   <<"ZeroMQ: "<<(d[38]==1?"ENABLED":"DISABLED/UNKNOWN")<<" (raw 0x"<<std::hex<<(unsigned)d[38]<<std::dec<<")\n"
   <<"TCP_NODELAY: "<<((d[19]&0x20)?"ON":"OFF")<<"\n"
   <<"(Authentication and raw flash contents intentionally suppressed.)\n";
}
static void requireyes(const Options&o,const std::string&description){
 if(!o.yes)throw std::runtime_error(description+" changes the modulator. Re-run with --yes to confirm.");
}
static void operate(const Options&o,const std::string &endpoint,const std::string &msg,bool verify=true){
 requireyes(o,msg);std::cerr<<"Sending "<<msg<<" to "<<o.host<<" ...\n";
 auto r=fetch(o,endpoint);
 std::cout<<"HTTP 200 for /"<<endpoint<<" ("<<r.size()<<" bytes). Command execution not proven by HTTP alone.\n";
 if(verify){std::this_thread::sleep_for(std::chrono::milliseconds(200));status(o);}
}
int main(int argc,char**argv){
 try{
  // Parse command and flags before initializing any networking.
  Options o=parse(argc,argv);
  if(curl_global_init(CURL_GLOBAL_DEFAULT)!=0)throw std::runtime_error("libcurl initialization failed");
  struct Cleanup {~Cleanup(){curl_global_cleanup();}} cleanup;
  const auto &c=o.cmd;
  if(c.size()==1&&c[0]=="status")status(o);
  else if(c.size()==1&&c[0]=="amplitude")amplitude(o);
  else if(c.size()==1&&c[0]=="config")config(o);
  else if(c.size()==2&&c[0]=="rf"&&(c[1]=="on"||c[1]=="off"))operate(o,c[1]=="on"?"s_020000":"s_025000","RF "+c[1]);
  else if(c.size()==2&&c[0]=="eti"&&(c[1]=="client"||c[1]=="server"||c[1]=="off"||c[1]=="reconnect")){
   if(c[1]=="reconnect"){
    // In absence of runtime mode query, verify flash mode before issuing client command.
    auto d=fetch(o,"b_070000");if(d.size()<222||d[28]!=4)throw std::runtime_error("Stored mode is not confirmed TCP CLIENT; reconnect refused");
   }
   auto p=(c[1]=="client"||c[1]=="reconnect")?"s_010400":c[1]=="server"?"s_010200":"s_010000";
   operate(o,p,"ETI "+c[1]);
  }
  else if(c.size()==3&&c[0]=="amplitude"&&c[1]=="set"){
   size_t n=0;int a=std::stoi(c[2],&n);if(n!=c[2].size()||a<1||a>255)throw std::runtime_error("Amplitude must be 1..255; 0 means READ");
   std::ostringstream path;path<<"s_000e"<<std::hex<<std::setw(2)<<std::setfill('0')<<a;
   operate(o,path.str(),"DAC amplitude set to "+std::to_string(a),false);amplitude(o);
  }
  else if(c.size()==1&&c[0]=="restart")operate(o,"r_000000","DEVICE RESTART",false);
  else throw std::runtime_error("Invalid command; try --help");
  return 0;
 }catch(const std::exception&e){std::cerr<<"ERROR: "<<e.what()<<"\n";return 1;}
}
