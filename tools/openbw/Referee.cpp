#include <BWAPI.h>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>

// Runs as a separate tournament module, outside the submitted bot source.
class Referee final : public BWAPI::AIModule {
  std::ofstream out;
  int cap = 86400;
  bool capped = false;
  void score(const char* event, bool winner = false) {
    auto p = BWAPI::Broodwar->self();
    out << "{\"event\":\"" << event << "\",\"frame\":" << BWAPI::Broodwar->getFrameCount()
        << ",\"winner\":" << (winner ? "true" : "false")
        << ",\"kills\":" << p->getKillScore() << ",\"razings\":" << p->getRazingScore()
        << ",\"score\":" << p->getKillScore() + p->getRazingScore() << "}\n";
    out.flush();
  }
  void unit(const char* event, BWAPI::Unit u) {
    if (!u || u->getPlayer() != BWAPI::Broodwar->self()) return;
    out << "{\"event\":\"" << event << "\",\"frame\":" << BWAPI::Broodwar->getFrameCount()
        << ",\"id\":" << u->getID() << ",\"type\":" << std::quoted(u->getType().getName())
        << ",\"worker\":" << (u->getType().isWorker() ? "true" : "false")
        << ",\"building\":" << (u->getType().isBuilding() ? "true" : "false") << "}\n";
  }
public:
  void onStart() override {
    out.open("referee.jsonl");
    if (auto value = std::getenv("STARBENCH_FRAME_CAP")) cap = std::stoi(value);
    auto& g = BWAPI::Broodwar;
    g->setGUI(false); g->setLocalSpeed(0); g->setFrameSkip(1000);
    auto start = g->self()->getStartLocation();
    out << "{\"event\":\"start\",\"map_hash\":" << std::quoted(g->mapHash())
        << ",\"start\":[" << start.x << ',' << start.y << "],\"race\":"
        << std::quoted(g->self()->getRace().getName()) << ",\"cap\":" << cap << "}\n";
    out.flush();
  }
  void onFrame() override {
    auto& g = BWAPI::Broodwar;
    int f = g->getFrameCount();
    if (f % 240 == 0) {
      auto p = g->self(); int idle = 0, workers = 0, army = 0;
      std::map<std::string, int> counts;
      for (auto u : p->getUnits()) {
        ++counts[u->getType().getName()];
        if (u->getType().isWorker()) { ++workers; if (u->isIdle()) ++idle; }
        else if (!u->getType().isBuilding()) ++army;
      }
      out << "{\"event\":\"sample\",\"frame\":" << f << ",\"minerals\":" << p->minerals()
          << ",\"gas\":" << p->gas() << ",\"supply_used\":" << p->supplyUsed()
          << ",\"supply_total\":" << p->supplyTotal() << ",\"workers\":" << workers
          << ",\"idle_workers\":" << idle << ",\"army\":" << army << ",\"units\":{";
      bool first = true;
      for (const auto& item : counts) { if (!first) out << ','; first = false; out << std::quoted(item.first) << ':' << item.second; }
      out << "}}\n"; out.flush();
    }
    if (f >= cap && !capped) {
      capped = true; score("cap"); g->leaveGame();
    }
  }
  void onUnitCreate(BWAPI::Unit u) override { unit("create", u); }
  void onUnitComplete(BWAPI::Unit u) override { unit("complete", u); }
  void onUnitDestroy(BWAPI::Unit u) override { unit("destroy", u); }
  void onEnd(bool winner) override { score("end", winner); }
};
class Rules final : public BWAPI::TournamentModule {
public:
  bool onAction(BWAPI::Tournament::ActionID action, void* parameter) override {
    using namespace BWAPI;
    if (action == Tournament::EnableFlag || action == Tournament::PauseGame ||
        action == Tournament::ResumeGame || action == Tournament::SetMap) return false;
    if (action == Tournament::SetLocalSpeed) *static_cast<int*>(parameter) = 0;
    if (action == Tournament::SetFrameSkip) *static_cast<int*>(parameter) = 1000;
    if (action == Tournament::SetGUI) *static_cast<bool*>(parameter) = false;
    return true;
  }
};
extern "C" void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr = game; }
extern "C" BWAPI::AIModule* newTournamentAI() { return new Referee(); }
extern "C" BWAPI::TournamentModule* newTournamentModule() { return new Rules(); }
