#include <BWAPI.h>

// Independent trial starting point: only assigns idle probes to minerals.
class Miner final : public BWAPI::AIModule {
public:
  void onFrame() override {
    if (BWAPI::Broodwar->getFrameCount() % 12) return;
    for (auto u : BWAPI::Broodwar->self()->getUnits()) {
      if (!u->getType().isWorker() || !u->isIdle()) continue;
      BWAPI::Unit closest = nullptr;
      int distance = 1000000;
      for (auto m : BWAPI::Broodwar->getMinerals()) {
        if (u->getDistance(m) < distance) { closest = m; distance = u->getDistance(m); }
      }
      if (closest) u->gather(closest);
    }
  }
};
extern "C" void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr = game; }
extern "C" BWAPI::AIModule* newAIModule() { return new Miner(); }
