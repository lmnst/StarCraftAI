#include <BWAPI.h>
#include <BWAPI/Client.h>
#include <windows.h>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "Bot.h"
using namespace BWAPI;
static std::string directory;
static bool candidate=true;
static bool capped=false;
static int ownScore=0,enemyScore=0,lastFrame=0;
static void scores(){ownScore=Broodwar->self()->getKillScore()+Broodwar->self()->getRazingScore();enemyScore=Broodwar->enemy()->getKillScore()+Broodwar->enemy()->getRazingScore();lastFrame=Broodwar->getFrameCount();}
static void result(bool winner){std::ofstream f(directory+(candidate?"\\result.json":"\\demo-result.json"));f<<"{\"outcome\":\""<<(capped?(ownScore>enemyScore?"win":ownScore<enemyScore?"loss":"draw"):(winner?"win":"loss"))<<"\",\"reason\":\""<<(capped?"frame_cap_kills_plus_razings":"BWAPI_MatchEnd")<<"\",\"frame\":"<<lastFrame<<",\"candidate_score\":"<<ownScore<<",\"opponent_score\":"<<enemyScore<<"}";}
int main(){
 directory=std::getenv("STARBENCH_RUN_DIR");
 const char* side=std::getenv("STARBENCH_SIDE");candidate=!side||std::string(side)!="opponent";
 while(!BWAPIClient.connect())std::this_thread::sleep_for(std::chrono::milliseconds(100));
 Bot bot;std::ofstream csv(directory+(candidate?"\\telemetry.csv":"\\demo-telemetry.csv"));csv<<"frame,minerals,gas,supply,workers,combat,buildings,own_score,enemy_score\n";
 const char* testCap=std::getenv("STARBENCH_PREFLIGHT_FRAME_CAP");
 const int frameCap=testCap?std::atoi(testCap):86400;
 bool began=false,ended=false;
 while(BWAPIClient.isConnected()){
  if(ended){BWAPIClient.update();continue;}
  for(auto& e:Broodwar->getEvents()){
   if(e.getType()==EventType::MatchStart){
    began=true;std::ofstream f(directory+(candidate?"\\start.json":"\\demo-start.json"));auto a=Broodwar->self()->getStartLocation(),b=Broodwar->enemy()->getStartLocation();
    for(auto location:Broodwar->getStartLocations())if(location!=a)b=location;
    f<<"{\"map_hash\":\""<<Broodwar->mapHash()<<"\",\"map\":\""<<Broodwar->mapFileName()<<"\",\"candidate_player_id\":"<<Broodwar->self()->getID()<<",\"candidate_start\":["<<a.x<<','<<a.y<<"],\"opponent_start\":["<<b.x<<','<<b.y<<"],\"race\":\""<<Broodwar->self()->getRace().getName()<<"\",\"opponent_race\":\""<<Broodwar->enemy()->getRace().getName()<<"\"}";
    bot.onStart();
   }
   if(e.getType()==EventType::MatchFrame&&began){
    scores();if(candidate&&lastFrame>=frameCap&&!capped){capped=true;Broodwar->leaveGame();}
    if(!capped)bot.onFrame();
    Broodwar->setLocalSpeed(0);Broodwar->setFrameSkip(0);
    if(lastFrame%240==0){int w=0,c=0,b=0;for(auto u:Broodwar->self()->getUnits()){if(u->getType().isWorker())w++;else if(u->getType().isBuilding())b++;else if(u->getType().canAttack())c++;}csv<<lastFrame<<','<<Broodwar->self()->minerals()<<','<<Broodwar->self()->gas()<<','<<Broodwar->self()->supplyUsed()/2<<','<<w<<','<<c<<','<<b<<','<<ownScore<<','<<enemyScore<<'\n';csv.flush();}
   }
   if(e.getType()==EventType::MatchEnd){if(!capped)scores();result(e.isWinner());ended=true;}
  }
  BWAPIClient.update();
 }
 return ended?0:1;
}
