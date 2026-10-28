#include <BWAPI.h>
#include "Bot.h"
void Bot::onStart() {}
void Bot::onFrame() {
 for(auto worker:BWAPI::Broodwar->self()->getUnits())if(worker->getType().isWorker()&&worker->isIdle()){
  BWAPI::Unit best=nullptr;int d=100000;for(auto m:BWAPI::Broodwar->getMinerals())if(worker->getDistance(m)<d){best=m;d=worker->getDistance(m);}if(best)worker->gather(best);
 }
}
