#include <BWAPI.h>
#ifdef LOCAL_Steamhammer
#include "UAlbertaBotModule.h"
extern "C" BWAPI::AIModule* newAIModule() { return new UAlbertaBot::UAlbertaBotModule(); }
#endif
#ifdef LOCAL_Locutus
#include "LocutusBotModule.h"
extern "C" BWAPI::AIModule* newAIModule() { return new Locutus::LocutusBotModule(); }
#endif
extern "C" void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr = game; }
