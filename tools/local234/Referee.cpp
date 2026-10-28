#include <BWAPI.h>
#include <cstdlib>
#include <fstream>
#include <string>
class RefereeAI: public BWAPI::AIModule {
 std::string prefix;int frame=0,score=0;bool reachedCap=false;std::ofstream csv;
 void sample(){frame=BWAPI::Broodwar->getFrameCount();auto p=BWAPI::Broodwar->self();score=p->getKillScore()+p->getRazingScore();}
 public:
 void onStart() override {
  const char* dir=std::getenv("STARBENCH_RUN_DIR"),*side=std::getenv("STARBENCH_SIDE");
  if(!dir||!side)return;prefix=std::string(dir)+"\\"+side;
  csv.open(prefix+"-referee.csv");csv<<"frame,kills,razings,score\n";
  BWAPI::Broodwar->setGUI(false);BWAPI::Broodwar->setFrameSkip(1000);BWAPI::Broodwar->setLocalSpeed(0);
 }
 void onFrame() override {
  BWAPI::Broodwar->setGUI(false);BWAPI::Broodwar->setFrameSkip(1000);BWAPI::Broodwar->setLocalSpeed(0);
  if(prefix.empty())return;sample();
  const char* testCap=std::getenv("STARBENCH_PREFLIGHT_FRAME_CAP");int cap=testCap?std::atoi(testCap):86400;
  if(frame>=cap&&!reachedCap){reachedCap=true;std::ofstream f(prefix+"-cap.json");f<<"{\"frame\":"<<frame<<",\"score\":"<<score<<'}';}
  if(frame%240==0){auto p=BWAPI::Broodwar->self();csv<<frame<<','<<p->getKillScore()<<','<<p->getRazingScore()<<','<<score<<'\n';csv.flush();}
 }
 void onEnd(bool winner) override {
  if(prefix.empty())return;sample();std::ofstream f(prefix+"-referee.json");auto p=BWAPI::Broodwar->self();f<<"{\"winner\":"<<(winner?"true":"false")<<",\"frame\":"<<frame<<",\"kills\":"<<p->getKillScore()<<",\"razings\":"<<p->getRazingScore()<<",\"score\":"<<score<<'}';
 }
};
class Rules: public BWAPI::TournamentModule {
 public:bool onAction(BWAPI::Tournament::ActionID a,void* p) override {
  using namespace BWAPI;
  if(a==Tournament::EnableFlag)return false;
  if(a==Tournament::SetLocalSpeed){*static_cast<int*>(p)=0;return true;}
  if(a==Tournament::SetFrameSkip){*static_cast<int*>(p)=1000;return true;}
  if(a==Tournament::SetGUI){*static_cast<bool*>(p)=false;return true;}
  if(a==Tournament::SetMap||a==Tournament::PauseGame||a==Tournament::ResumeGame)return false;
  return true;
 }
};
extern "C" __declspec(dllexport) void gameInit(BWAPI::Game* game){BWAPI::BroodwarPtr=game;}
extern "C" __declspec(dllexport) BWAPI::AIModule* newTournamentAI(){return new RefereeAI();}
extern "C" __declspec(dllexport) BWAPI::TournamentModule* newTournamentModule(){return new Rules();}
