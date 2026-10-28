// Locally authored demonstration opponents. These are NOT the official demos.
#define DEMO_MODE 1
#include <BWAPI.h>
#include "Bot.h"
using namespace BWAPI;
static int count(UnitType t){return Broodwar->self()->allUnitCount(t);}
static bool completed(UnitType t){return Broodwar->self()->completedUnitCount(t)>0;}
static bool build(UnitType type){
 if(Broodwar->self()->minerals()<type.mineralPrice()||Broodwar->self()->gas()<type.gasPrice())return false;
 for(auto u:Broodwar->self()->getUnits())if(u->getType().isWorker()&&!u->isConstructing()&&!u->isMoving()&&!u->isCarryingGas()){
  auto at=Broodwar->getBuildLocation(type,Broodwar->self()->getStartLocation(),32,false);if(at.isValid()&&u->build(type,at))return true;
 }return false;
}
void Bot::onStart(){}
void Bot::onFrame(){
 if(Broodwar->getFrameCount()%12)return;
 auto self=Broodwar->self();using namespace UnitTypes;
 int gasWorkers=0;for(auto u:self->getUnits())if(u->isGatheringGas())gasWorkers++;
 for(auto u:self->getUnits()){
  if(u->getType().isWorker()&&!u->isConstructing()){
   Unit target=nullptr;
   if(DEMO_MODE!=1&&completed(Protoss_Assimilator)&&gasWorkers<3&&!u->isCarryingMinerals()){
    for(auto r:self->getUnits())if(r->getType()==Protoss_Assimilator&&r->isCompleted()){target=r;gasWorkers++;break;}
   }
   if(!target&&u->isIdle()){int d=999999;for(auto m:Broodwar->getMinerals())if(u->getDistance(m)<d){d=u->getDistance(m);target=m;}}
   if(target)u->gather(target);
  }
  if(u->getType()==Protoss_Nexus&&u->isCompleted()&&!u->isTraining()&&count(Protoss_Probe)<(DEMO_MODE==1?14:22))u->train(Protoss_Probe);
 }
 if(self->supplyTotal()-self->supplyUsed()<6&&self->supplyTotal()<400&&count(Protoss_Pylon)==self->completedUnitCount(Protoss_Pylon)){if(build(Protoss_Pylon))return;}
 if(!completed(Protoss_Pylon))return;
 const int gates=DEMO_MODE==2?4:2;
 if(count(Protoss_Gateway)<gates){if(build(Protoss_Gateway))return;}
 if(DEMO_MODE!=1&&count(Protoss_Gateway)&&!count(Protoss_Assimilator)){if(build(Protoss_Assimilator))return;}
 if(DEMO_MODE!=1&&completed(Protoss_Gateway)&&!count(Protoss_Cybernetics_Core)){if(build(Protoss_Cybernetics_Core))return;}
 if(DEMO_MODE==3&&completed(Protoss_Cybernetics_Core)&&!count(Protoss_Citadel_of_Adun)){if(build(Protoss_Citadel_of_Adun))return;}
 if(DEMO_MODE==3&&completed(Protoss_Citadel_of_Adun)&&!count(Protoss_Templar_Archives)){if(build(Protoss_Templar_Archives))return;}
 for(auto u:self->getUnits()){
  if(u->getType()==Protoss_Cybernetics_Core&&DEMO_MODE==2&&!u->isUpgrading())u->upgrade(UpgradeTypes::Singularity_Charge);
  if(u->getType()==Protoss_Gateway&&u->isCompleted()&&!u->isTraining()){
   UnitType t=DEMO_MODE==1?Protoss_Zealot:DEMO_MODE==3&&completed(Protoss_Templar_Archives)?Protoss_Dark_Templar:completed(Protoss_Cybernetics_Core)?Protoss_Dragoon:Protoss_Zealot;
   u->train(t);
  }
  if(u->getType().canAttack()&&!u->getType().isWorker()&&!u->getType().isBuilding()&&u->isCompleted()&&(u->isIdle()||u->isMoving())){
   Position target=Position(self->getStartLocation());for(auto p:Broodwar->getStartLocations())if(p!=self->getStartLocation())target=Position(p)+Position(64,48);
   for(auto e:Broodwar->enemy()->getUnits())if(e->isVisible()&&e->getType().isBuilding()){target=e->getPosition();break;}
   u->attack(target);
  }
 }
}
