#pragma once
#include <BWAPI.h>
#include <fstream>
#include <cstdlib>
#include <string>

// Observation only: no build, movement, combat or surrender commands.
namespace LocalTrace {
struct Trace {
    std::string bot, directory;
    std::ofstream csv, events;
    int lastFrame = -1;
};
inline Trace& state() { static Trace value; return value; }
inline std::string quote(const std::string& value) {
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') { result += '\\'; result += c; }
        else if (c == '\n') result += "\\n";
        else if (c == '\r') result += "\\r";
        else if (c >= 32) result += c;
    }
    return result + "\"";
}
inline void begin(const char* bot) {
    auto& t = state();
    if (t.csv.is_open()) t.csv.close();
    if (t.events.is_open()) t.events.close();
    const char* run = std::getenv("STARBENCH_RUN_DIR");
    t.directory = run ? run : "E:\\StarCraftAI\\logs";
    t.bot = bot; t.lastFrame = -1;
    t.csv.open(t.directory + "\\" + t.bot + "-telemetry.csv", std::ios::trunc);
    t.events.open(t.directory + "\\" + t.bot + "-events.csv", std::ios::trunc);
    t.csv << "frame,minerals,gas,supply_used,supply_total,workers,combat_units,buildings\n";
    t.events << "frame,event,unit_id,owner_id,type,x,y\n";
    auto self = BWAPI::Broodwar->self();
    auto enemy = BWAPI::Broodwar->enemy();
    std::ofstream meta(t.directory + "\\" + t.bot + "-start.json");
    meta << "{\n  \"bot\": " << quote(t.bot)
         << ",\n  \"player\": " << quote(self ? self->getName() : "")
         << ",\n  \"player_id\": " << (self ? self->getID() : -1)
         << ",\n  \"enemy\": " << quote(enemy ? enemy->getName() : "")
         << ",\n  \"enemy_id\": " << (enemy ? enemy->getID() : -1)
         << ",\n  \"map\": " << quote(BWAPI::Broodwar->mapFileName())
         << ",\n  \"map_hash\": " << quote(BWAPI::Broodwar->mapHash())
         << ",\n  \"multiplayer\": " << (BWAPI::Broodwar->isMultiplayer() ? "true" : "false")
         << ",\n  \"manual_unit_input\": false\n}\n";
}
inline void event(const char* kind, BWAPI::Unit unit) {
    auto& t = state();
    if (!t.events.is_open() || !unit) return;
    auto owner = unit->getPlayer();
    auto p = unit->getPosition();
    t.events << BWAPI::Broodwar->getFrameCount() << ',' << kind << ',' << unit->getID()
             << ',' << (owner ? owner->getID() : -1) << ',' << unit->getType().getName()
             << ',' << p.x << ',' << p.y << '\n';
    t.events.flush();
}
inline void record(const char* bot, bool force = false) {
    if (!BWAPI::BroodwarPtr || !BWAPI::Broodwar->self()) return;
    auto& t = state();
    if (t.bot.empty()) begin(bot);
    int workers = 0, combat = 0, buildings = 0;
    for (auto unit : BWAPI::Broodwar->self()->getUnits()) {
        if (!unit || !unit->exists()) continue;
        auto type = unit->getType();
        if (type.isBuilding()) ++buildings;
        else if (type.isWorker()) ++workers;
        else if (type.canAttack()) ++combat;
    }
    auto self = BWAPI::Broodwar->self();
    int frame = BWAPI::Broodwar->getFrameCount();
    BWAPI::Broodwar->drawBoxScreen(5, 298, 470, 350, BWAPI::Colors::Black, true);
    BWAPI::Broodwar->drawTextScreen(10, 302, "%s | frame %d | automatic control", bot, frame);
    BWAPI::Broodwar->drawTextScreen(10, 317, "Minerals %d Gas %d Supply %.0f/%.0f", self->minerals(), self->gas(), self->supplyUsed()/2.0, self->supplyTotal()/2.0);
    BWAPI::Broodwar->drawTextScreen(10, 332, "Workers %d Combat %d Buildings %d | sample: 24 frames", workers, combat, buildings);
    if (frame == t.lastFrame || (!force && frame % 24 != 0)) return;
    t.csv << frame << ',' << self->minerals() << ',' << self->gas() << ','
          << self->supplyUsed()/2.0 << ',' << self->supplyTotal()/2.0 << ','
          << workers << ',' << combat << ',' << buildings << '\n';
    t.csv.flush(); t.lastFrame = frame;
}
inline void end(const char* bot, bool winner) {
    record(bot, true);
    auto& t = state();
    std::ofstream result(t.directory + "\\" + t.bot + "-result.json");
    result << "{\n  \"bot\": " << quote(bot)
           << ",\n  \"winner\": " << (winner ? "true" : "false")
           << ",\n  \"frame\": " << BWAPI::Broodwar->getFrameCount()
           << ",\n  \"source\": \"BWAPI MatchEnd callback\"\n}\n";
    t.events << BWAPI::Broodwar->getFrameCount() << ",match_end,-1,-1,"
             << (winner ? "win" : "loss") << ",-1,-1\n";
    t.events.flush();
}
}
inline void RecordLocalTelemetry(const char* bot) { LocalTrace::record(bot); }