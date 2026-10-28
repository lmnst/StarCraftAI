#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <string>
#include <vector>
#include "../RuntimePaths.h"
#include "../../src/STARTcraft/src/bwapi/include/BWAPI/Client/GameTable.h"
using namespace std;
static HANDLE job;
static wstring out;
static string utf8(const wstring& s) { int n=WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),0,0,0,0); string r(n,0); WideCharToMultiByte(CP_UTF8,0,s.data(),(int)s.size(),&r[0],n,0,0); return r; }
static string quote(const wstring& s) { string r="\""; for(char c:utf8(s)){if(c=='\\'||c=='"')r+='\\';r+=c;}return r+'"'; }
static string read(const wstring& p){ifstream f(p);return string(istreambuf_iterator<char>(f),{});}
struct Process { DWORD pid=0; HANDLE handle=nullptr; };
static Process spawn(const wstring& exe,wstring args,const wstring& cwd,const wstring& log){
 SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
 HANDLE f=CreateFileW((out+L"\\"+log).c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,CREATE_NEW,0,nullptr);
 HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdOutput=f;si.hStdError=f;si.hStdInput=in;
 PROCESS_INFORMATION pi{};
 BOOL ok=CreateProcessW(exe.c_str(),&args[0],nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi);
 CloseHandle(f);CloseHandle(in);
 if(!ok)throw runtime_error("CreateProcess failed: "+to_string(GetLastError()));
 if(!AssignProcessToJobObject(job,pi.hProcess)){TerminateProcess(pi.hProcess,2);throw runtime_error("job assignment failed");}
 ResumeThread(pi.hThread);CloseHandle(pi.hThread);return {pi.dwProcessId,pi.hProcess};
}
static DWORD child(DWORD parent){
 HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32W p{};p.dwSize=sizeof(p);DWORD r=0;
 if(Process32FirstW(s,&p))do{if(p.th32ParentProcessID==parent&&!_wcsicmp(p.szExeFile,L"StarCraft.exe")){r=p.th32ProcessID;break;}}while(Process32NextW(s,&p));CloseHandle(s);return r;
}
static bool connected(DWORD pid){
 HANDLE h=OpenFileMappingW(FILE_MAP_READ,FALSE,L"Local\\bwapi_shared_memory_game_list");if(!h)return false;
 auto t=(const BWAPI::GameTable*)MapViewOfFile(h,FILE_MAP_READ,0,0,sizeof(BWAPI::GameTable));bool yes=false;
 if(t){for(auto& e:t->gameInstances)if(e.serverProcessID==pid&&e.isConnected)yes=true;UnmapViewOfFile(t);}CloseHandle(h);return yes;
}
static bool replay(const wchar_t* name){WIN32_FIND_DATAW a{};HANDLE h=FindFirstFileW((out+L"\\"+name+L"*.rep").c_str(),&a);if(h==INVALID_HANDLE_VALUE)return false;FindClose(h);return a.nFileSizeLow>512;}
static bool exited(HANDLE h){return h&&WaitForSingleObject(h,0)==WAIT_OBJECT_0;}
// candidate, guest runtime, guest kind (dll/exe), guest entry, map, seed,
// candidate host (0/1), output directory, wall timeout seconds, guest BWAPI path
int wmain(int argc,wchar_t** v){
 if(argc!=11)return 2;
 out=v[8];if(!CreateDirectoryW(out.c_str(),nullptr))return 3;
 auto began=GetTickCount64();string status="infrastructure_error",error;int exitcode=1;
 vector<Process> processes;
 try{
  job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))throw runtime_error("job setup failed");
  wstring root=StarBench::ProjectRoot(), games[2]={root+L"StarCraft1161",v[2]};
  bool hostCandidate=wcstol(v[7],nullptr,10)!=0, dll=wstring(v[3])==L"dll";
  for(int i=0;i<2;i++){
   bool host=(i==0)==hostCandidate;ofstream f(out+(i?L"\\Opponent.ini":L"\\Candidate.ini"));
   f<<"[ai]\ntournament=Referee.dll\nai="<<(i&&dll?StarBench::Narrow(L"bwapi-data/AI/"+wstring(v[4]).substr(wstring(v[4]).find_last_of(L"\\/")+1)):"NULL")<<"\nai_dbg=NULL\n[auto_menu]\nauto_menu=LAN\ncharacter_name="<<(i?"Opponent":"Candidate")
    <<"\nlan_mode=Local PC\nauto_restart=ON\nmap="<<(host?StarBench::Narrow(L"maps/BroodWar/AIIDE/"+wstring(v[5])):"")
    <<"\ngame="<<(hostCandidate?"Candidate":"Opponent")<<"\nrace=Protoss\ngame_type=MELEE\nwait_for_min_players=2\nwait_for_max_players=2\nwait_for_time=100\nsave_replay="<<StarBench::Narrow(out)<<(i?"\\Opponent.rep":"\\Candidate.rep")
    <<"\n[config]\nshared_memory="<<(i&&dll?"OFF":"ON")<<"\nconsole_attach_auto=FALSE\nconsole_alloc_auto=FALSE\n[window]\nwindowed=ON\nleft="<<(i?685:10)<<"\ntop=40\nwidth=640\nheight=480\n[starcraft]\nsound=OFF\nseed_override="<<wcstoul(v[6],0,10)<<"\n";
  }
  Process injectors[2],bots[2],gameprocs[2];DWORD pids[2]{};
  auto game=[&](int i){StarBench::EnvironmentOverride rundir(L"STARBENCH_RUN_DIR",out);StarBench::EnvironmentOverride side(L"STARBENCH_SIDE",i?L"opponent":L"candidate");StarBench::EnvironmentOverride cfg(L"BWAPI_CONFIG_INI",out+(i?L"\\Opponent.ini":L"\\Candidate.ini"));wstring exe=games[i]+L"\\injectory.x86.exe";wstring bwapi=L"bwapi-data\\BWAPI.dll";injectors[i]=spawn(exe,L"\""+exe+L"\" --launch StarCraft.exe --inject \""+bwapi+L"\" --wait-for-exit",games[i],i?L"opponent-inject.log":L"candidate-inject.log");processes.push_back(injectors[i]);};
  int host=hostCandidate?0:1;game(host);bool guestLaunched=false;ULONGLONG deadAt=0;
  while(GetTickCount64()-began<1000ULL*wcstoul(v[9],0,10)){
   for(int i=0;i<2;i++)if(injectors[i].pid&&!pids[i]){
    pids[i]=child(injectors[i].pid);if(pids[i]){
     gameprocs[i]={pids[i],OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pids[i])};processes.push_back(gameprocs[i]);
     if(i==0||!dll){StarBench::EnvironmentOverride role(L"STARBENCH_SIDE",i?L"opponent":L"candidate");StarBench::EnvironmentOverride pid(L"STARBENCH_SERVER_PID",to_wstring(pids[i]));StarBench::EnvironmentOverride run(L"STARBENCH_RUN_DIR",out);wstring exe=i?v[4]:wstring(v[1])+L"\\Candidate.exe";bots[i]=spawn(exe,L"\""+exe+L"\"",i?games[i]:v[1],i?L"opponent-bot.log":L"candidate-bot.log");processes.push_back(bots[i]);}
     ofstream b(out+(i?L"\\opponent-binding.json":L"\\candidate-binding.json"));b<<"{\"game_pid\":"<<pids[i]<<",\"bot_pid\":"<<bots[i].pid<<"}";
    }
   }
   if(!guestLaunched&&pids[host]&&((host==1&&dll)||connected(pids[host]))){game(1-host);guestLaunched=true;}
   if(!read(out+L"\\result.json").empty()){
    auto end=GetTickCount64();while(GetTickCount64()-end<20000&&!(replay(L"Candidate")&&replay(L"Opponent")))Sleep(100);
    if(replay(L"Candidate")&&replay(L"Opponent")){status="complete";exitcode=0;}else{status="missing_replay";}
    break;
   }
   bool dead=false;for(int i=0;i<2;i++)if(exited(gameprocs[i].handle)||exited(bots[i].handle))dead=true;
   if(dead){if(!deadAt)deadAt=GetTickCount64();if(GetTickCount64()-deadAt>10000){status=read(out+L"\\start.json").empty()?"infrastructure_error":"process_exit";break;}}
   if(GetTickCount64()-began>60000&&read(out+L"\\start.json").empty()){error="game did not start within 60 seconds";break;}
   Sleep(50);
  }
  if(status=="infrastructure_error"&&error.empty())error="wall timeout or startup failure";
 }catch(exception& e){error=e.what();}
 ofstream f(out+L"\\receipt.json");f<<"{\"status\":\""<<status<<"\",\"elapsed_seconds\":"<<(GetTickCount64()-began)/1000.0<<",\"error\":"<<quote(StarBench::Widen(error))<<",\"processes\":[";
 for(size_t i=0;i<processes.size();i++){DWORD code=STILL_ACTIVE;if(processes[i].handle)GetExitCodeProcess(processes[i].handle,&code);if(i)f<<',';f<<"{\"pid\":"<<processes[i].pid<<",\"exit_code\":"<<code<<'}';}f<<"]}";f.close();
 if(job){TerminateJobObject(job,0);CloseHandle(job);}for(auto& p:processes)if(p.handle)CloseHandle(p.handle);return exitcode;
}
