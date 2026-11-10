#include <BWAPI.h>
#include <fstream>

class ReplayAudit final : public BWAPI::AIModule {
  std::ofstream log;
public:
  void onStart() override {
    log.open("bwapi-data/write/replay-audit.jsonl");
    log << "{\"event\":\"start\",\"replay\":" << (BWAPI::Broodwar->isReplay() ? "true" : "false")
        << ",\"expected_frame\":" << BWAPI::Broodwar->getReplayFrameCount() << "}" << std::endl;
  }
  void onEnd(bool) override {
    log << "{\"event\":\"end\",\"frame\":" << BWAPI::Broodwar->getFrameCount() << "}" << std::endl;
  }
};
extern "C" void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr=game; }
extern "C" BWAPI::AIModule* newAIModule() { return new ReplayAudit(); }
