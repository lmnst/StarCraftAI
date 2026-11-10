// v6-fix-r1-20261110 with the human commander: F5-F9 hotkeys (STARCRAFT_COMMAND_MODE=demo)
// and a file-based command entry (STARCRAFT_COMMAND_MODE=external). Strategy itself is the frozen fix-r1.
#include <windows.h>
#include <BWAPI.h>
#include <bwem.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <fstream>
#include <map>
#include <set>
#include <vector>
using namespace BWAPI;
using namespace UnitTypes;

// Astra round 6: v3 baseline with targetless retreat from undetected ground attackers.
// Information about enemy units comes only from BWAPI visibility and remembered sightings.
class AstraBot final : public AIModule {
  Player me;
  Position home, enemyStart, rally;
  std::vector<TilePosition> bases;
  std::vector<Unit> own, enemies, army, workers, nexuses;
  std::map<int, Position> enemyBuildings;
  std::set<int> defenders;
  Unit scout = nullptr, builder = nullptr;
  UnitType planned = None;
  TilePosition buildTile = TilePositions::Invalid;
  int planFrame = 0, frame = 0, cash = 0, gas = 0;
  int rangeStart = -1;
  bool attacking = false, cloakSeen = false, cloakTechSeen = false;
  Unit homeThreat = nullptr;
  std::ofstream trace, commander;
  enum class Stance { Attack, Rally, Retreat, Defend };
  enum class Action { Attack, Defend, Rally, Retreat, Expand, Invalid };
  static constexpr const char* actions[] = {"attack","defend","rally","retreat","expand"};
  struct Expansion {
    const char* source = nullptr;
    int requestFrame = -1;
    TilePosition tile = TilePositions::Invalid;
  };
  Expansion expansionPlan;
  std::map<int,Expansion> expansionConstruction;
  int expansionRequestFrame = -1;
  const char* expansionStatus = "idle";
  static constexpr int decisionFrames = 120;
  bool external = false;
  Stance stance = Stance::Rally;
  int lastSnapshot = -1, lastSequence = 0, expandUntil = -1;
  std::streamoff commandOffset = 0;
  std::map<int, int> buildingLastSeen;

  bool demo = false;
  HHOOK demoKeyHook = nullptr;
  static inline std::vector<WPARAM> demoKeys;
  struct KeyCommand { Action action; int frame; };
  std::vector<KeyCommand> keyCommands;

  static LRESULT CALLBACK receiveDemoKey(int code, WPARAM removal, LPARAM data) {
    if(code==HC_ACTION && removal==PM_REMOVE) {
      const auto* message=reinterpret_cast<const MSG*>(data);
      if(message->message==WM_KEYDOWN && !(message->lParam & (1L<<30)) &&
         message->wParam>=VK_F5 && message->wParam<=VK_F9)
        demoKeys.push_back(message->wParam);
    }
    return CallNextHookEx(nullptr,code,removal,data);
  }
  int appliedKeyFrame = -1, lastDemoTick = -1;

  static const char* actionName(Action action) {
    return action==Action::Invalid ? "invalid" : actions[static_cast<int>(action)];
  }

  static Action parseAction(const std::string& name) {
    for(int i=0;i<5;++i) if(name==actions[i]) return static_cast<Action>(i);
    return Action::Invalid;
  }

  const char* stanceName() const {
    switch(stance) {
      case Stance::Attack: return "attack";
      case Stance::Retreat: return "retreat";
      case Stance::Defend: return "defend";
      default: return "rally";
    }
  }

  int cannonBlockedUntil = -1;
  UnitType lastDetectStep = None;
  int defenseTarget = -1, lastContact = -1;
  UnitType defenseTargetType = None;
  int defenseRetryTarget = -1, defenseRetryUntil = -1;
  static constexpr int defenseRadius = 420;
  static constexpr int chaseMargin = 160, contactTimeout = 72, contactRange = 96;
  static constexpr int cannonRetryDelay = 720;
  // Observational only: these values never enter a gameplay decision.
  long long defenderFrames = 0;
  std::set<int> defendersEver;
  struct UndetectedSnapshot { int id; UnitType type; Position position; };
  std::vector<UndetectedSnapshot> undetectedSnapshots;
  std::map<int, Position> ownPositions;
  int enemySnapshotFrame = -1;

  void recordDefenderUsage() {
    // Nominal assigned-worker frames, sampled every 8 frames; not all attack orders.
    defenderFrames += 8LL * static_cast<long long>(defenders.size());
    for(int id:defenders) defendersEver.insert(id);
  }
  void recordOwnLoss(Unit u) {
    if(!u || u->getPlayer()!=Broodwar->self()) return;
    Position position=u->getPosition();
    auto remembered=ownPositions.find(u->getID());
    if(!position.isValid() && remembered!=ownPositions.end()) position=remembered->second;
    if(!position.isValid()) return;
    // Read only the last legal visible snapshot, never a now-hidden enemy handle.
    // Proximity is a proxy, not attribution of the killing blow to a DT.
    for(const auto &e:undetectedSnapshots) if(position.getDistance(e.position)<224) {
      trace<<"{\"event\":\"own_loss_undetected\",\"frame\":"<<Broodwar->getFrameCount()
           <<",\"type\":\""<<u->getType().getName()<<"\",\"unit_id\":"<<u->getID()
           <<",\"snapshot_frame\":"<<enemySnapshotFrame<<",\"threat_id\":"<<e.id
           <<",\"threat_type\":\""<<e.type.getName()<<"\",\"x\":"<<position.x<<",\"y\":"<<position.y<<"}\n";
      break;
    }
  }

  int count(UnitType t, bool complete=false) const {
    int n=0; for (auto u:own) if(u->getType()==t && (!complete || u->isCompleted())) ++n;
    return n;
  }
  int queued(UnitType t) const {
    int n=count(t); for(auto u:own) if(u->getType().isBuilding())
      for(auto q:u->getTrainingQueue()) if(q==t) ++n;
    return n;
  }
  Position clamp(Position p) const {
    return Position(std::max(16,std::min(Broodwar->mapWidth()*32-17,p.x)),
                    std::max(16,std::min(Broodwar->mapHeight()*32-17,p.y)));
  }
  Position toward(Position a, Position b, double distance) const {
    double dx=b.x-a.x,dy=b.y-a.y,len=std::sqrt(dx*dx+dy*dy);
    if(len<1) return a;
    return clamp(Position(int(a.x+dx/len*distance),int(a.y+dy/len*distance)));
  }
  double baseDistance(Position p) const {
    double d=p.getDistance(home); for(auto n:nexuses) d=std::min(d,p.getDistance(n->getPosition())); return d;
  }
  bool commandReady(Unit u,int delay=16) const {
    return frame-u->getLastCommandFrame()>=delay && !u->isAttackFrame() && !u->isStartingAttack();
  }
  void move(Unit u,Position p) {
    if(commandReady(u) && (u->isIdle() || u->getTargetPosition().getDistance(p)>48 || u->isStuck())) u->move(p);
  }
  void hit(Unit u,Unit target) {
    if(target && commandReady(u,12) && (u->getOrderTarget()!=target || u->isIdle() || u->isStuck())) u->attack(target);
  }
  void advance(Unit u,Position p) {
    if(commandReady(u,24) && (u->isIdle() || u->getTargetPosition().getDistance(p)>96 || u->isStuck())) u->attack(p);
  }
  void refresh() {
    own.clear(); enemies.clear(); army.clear(); workers.clear(); nexuses.clear();
    undetectedSnapshots.clear(); ownPositions.clear(); enemySnapshotFrame=frame;
    for(auto u:me->getUnits()) if(u->exists()) {
      own.push_back(u);
      ownPositions[u->getID()]=u->getPosition();
      if(u->getType().isWorker() && u->isCompleted()) workers.push_back(u);
      else if(!u->getType().isBuilding() && u->isCompleted() && (u->getType().canAttack() || u->getType()==Protoss_Reaver)) army.push_back(u);
      if(u->getType()==Protoss_Nexus && u->isCompleted()) nexuses.push_back(u);
    }
    for(auto u:Broodwar->enemy()->getUnits()) if(u->exists() && u->isVisible()) {
      enemies.push_back(u);
      if(u->getType().isBuilding()) {
        enemyBuildings[u->getID()]=u->getPosition();
        if(external) buildingLastSeen[u->getID()]=frame;
      }
      UnitType t=u->getType();
      if(t==Protoss_Dark_Templar || u->isCloaked() || u->isBurrowed()) {
        if(!cloakSeen) trace<<"{\"event\":\"cloak_seen\",\"frame\":"<<frame<<",\"flag\":\"cloak\",\"type\":\""<<t.getName()<<"\"}\n";
        cloakSeen=true;
      }
      if(t==Protoss_Citadel_of_Adun || t==Protoss_Templar_Archives) {
        if(!cloakTechSeen) trace<<"{\"event\":\"cloak_seen\",\"frame\":"<<frame<<",\"flag\":\"tech\",\"type\":\""<<t.getName()<<"\"}\n";
        cloakTechSeen=true;
      }
      if(!u->isDetected() && !u->isFlying() && t.canAttack())
        undetectedSnapshots.push_back({u->getID(),t,u->getPosition()});
    }
    for(auto it=enemyBuildings.begin();it!=enemyBuildings.end();) {
      bool seen=false; for(auto e:enemies) if(e->getID()==it->first) seen=true;
      if(Broodwar->isVisible(TilePosition(it->second)) && !seen) {
        buildingLastSeen.erase(it->first);
        it=enemyBuildings.erase(it);
      } else ++it;
    }
    homeThreat=nullptr; double best=1e9;
    for(auto e:enemies) if(baseDistance(e->getPosition())<700 && (!e->getType().isWorker() || e->isAttacking())) {
      if(!e->getType().canAttack() && !e->getType().isBuilding()) continue;
      double score=baseDistance(e->getPosition());
      if(e->getType()==Protoss_Photon_Cannon) score-=150;
      if(score<best) {best=score;homeThreat=e;}
    }
  }
  bool freeSite(TilePosition t,UnitType type) const {
    if(!t.isValid() || !Broodwar->canBuildHere(t,type,nullptr,false)) return false;
    if(type.requiresPsi() && !Broodwar->hasPower(t,type)) return false;
    int x=t.x,y=t.y,w=type.tileWidth(),h=type.tileHeight();
    for(auto u:own) if(u->getType().isBuilding()) {
      auto p=u->getTilePosition();
      if(x<p.x+u->getType().tileWidth()+1 && x+w+1>p.x && y<p.y+u->getType().tileHeight()+1 && y+h+1>p.y) return false;
    }
    Position center=Position(t)+Position(w*16,h*16);
    for(auto m:Broodwar->getMinerals()) if(m->getDistance(center)<100) return false;
    for(auto g:Broodwar->getGeysers()) if(g->getDistance(center)<100) return false;
    for(auto b:bases) {
      if(x<b.x+5 && x+w+1>b.x && y<b.y+4 && y+h+1>b.y) return false;
    }
    return true;
  }
  TilePosition findSite(UnitType type,Position anchor) const {
    TilePosition origin(anchor),chosen=TilePositions::Invalid; double best=1e18;
    for(int dx=-16;dx<=16;++dx) for(int dy=-16;dy<=16;++dy) {
      TilePosition t(origin.x+dx,origin.y+dy);
      if(!freeSite(t,type)) continue;
      Position p=Position(t)+Position(type.tileWidth()*16,type.tileHeight()*16);
      double s=p.getDistance(anchor);
      if(type==Protoss_Pylon) {
        for(auto u:own) if(u->getType()==Protoss_Pylon && u->getDistance(p)<128) s+=180;
      }
      if(s<best) {best=s;chosen=t;}
    }
    return chosen;
  }
  TilePosition expansion() const {
    TilePosition best=TilePositions::Invalid; int distance=10000000;
    for(auto t:bases) {
      Position p=Position(t)+Position(64,48);
      if(p.getDistance(home)<200 || p.getDistance(enemyStart)<700) continue;
      bool occupied=false;
      for(auto n:own) if(n->getType()==Protoss_Nexus && n->getDistance(p)<240) occupied=true;
      for(auto e:enemies) if(e->getDistance(p)<400 && e->getType().canAttack()) occupied=true;
      if(occupied) continue;
      int length=0; BWEM::Map::Instance().GetPath(home,p,&length);
      if(length>=0 && length<distance) {distance=length;best=t;}
    }
    return best;
  }
  bool expansionInProgress() const {
    return planned==Protoss_Nexus || count(Protoss_Nexus)>count(Protoss_Nexus,true);
  }

  bool wantsExpansion(int baseNumber) const {
    if(demo && expansionInProgress()) return false;
    if(external && expandUntil>=frame) return true;
    if(external && !demo) return false;
    if(homeThreat) return false;
    if(baseNumber==2) return workers.size()>=20 && army.size()>=8 && frame>8000;
    return army.size()>=18 && me->minerals()>700;
  }

  UnitType nextBuilding() const {
    int pylons=count(Protoss_Pylon), gates=count(Protoss_Gateway), core=count(Protoss_Cybernetics_Core);
    int futureSupply=me->supplyTotal()+16*(pylons-count(Protoss_Pylon,true));
    int margin=std::max(4,3*gates+int(nexuses.size())*2);
    if(me->supplyTotal()<400 && futureSupply-me->supplyUsed()<=margin) return Protoss_Pylon;
    if(pylons==0 && workers.size()>=7) return Protoss_Pylon;
    for(auto u:own) if(u->getType().requiresPsi() && u->isCompleted() && !u->isPowered()) return Protoss_Pylon;
    if(count(Protoss_Pylon,true)==0) return None;
    if(gates<1) return Protoss_Gateway;
    if(gates<2 && workers.size()>=12) return Protoss_Gateway;
    bool detection=cloakSeen || cloakTechSeen;
    if(detection && count(Protoss_Forge)==0) return Protoss_Forge;
    if(detection && count(Protoss_Forge,true) && count(Protoss_Photon_Cannon)<int(nexuses.size()) && frame>=cannonBlockedUntil) return Protoss_Photon_Cannon;
    if(count(Protoss_Assimilator)==0 && (count(Protoss_Zealot)>=3 || frame>4400 || detection)) return Protoss_Assimilator;
    if(count(Protoss_Assimilator)>0 && core==0) return Protoss_Cybernetics_Core;
    if(core && detection && count(Protoss_Robotics_Facility)==0) return Protoss_Robotics_Facility;
    if(count(Protoss_Robotics_Facility,true) && count(Protoss_Observatory)==0) return Protoss_Observatory;
    if(count(Protoss_Nexus)<2 && wantsExpansion(2) && expansion().isValid()) return Protoss_Nexus;
    int targetGates=std::min(8,2+(workers.size()>=18?1:0)+2*std::max(0,int(nexuses.size())-1));
    if(gates<targetGates && core && (me->minerals()>180 || army.size()>=6)) return Protoss_Gateway;
    if(core && count(Protoss_Robotics_Facility)==0 && (frame>9000 && army.size()>=10)) return Protoss_Robotics_Facility;
    if(count(Protoss_Robotics_Facility,true) && count(Protoss_Observatory,true) && count(Protoss_Robotics_Support_Bay)==0 && army.size()>=10 && frame>11500) return Protoss_Robotics_Support_Bay;
    if(count(Protoss_Forge)==0 && army.size()>=12 && me->minerals()>350) return Protoss_Forge;
    if(count(Protoss_Citadel_of_Adun)==0 && count(Protoss_Zealot)>=8 && frame>9500 && me->minerals()>250) return Protoss_Citadel_of_Adun;
    if(count(Protoss_Assimilator)<int(nexuses.size()) && me->gas()<150 && core) return Protoss_Assimilator;
    if(count(Protoss_Nexus)<3 && wantsExpansion(3) && expansion().isValid()) return Protoss_Nexus;
    if(me->minerals()>700 && gates<10) return Protoss_Gateway;
    return None;
  }
  void logExpansion(const char* phase, const Expansion& plan, int gameFrame, int unitId=-1) {
    commander << "{\"event\":\"expansion_" << phase << "\",\"frame\":" << gameFrame
              << ",\"source\":\"" << plan.source << "\",\"request_frame\":" << plan.requestFrame;
    if(plan.tile.isValid()) commander << ",\"tile\":[" << plan.tile.x << ',' << plan.tile.y << ']';
    if(unitId>=0) commander << ",\"unit_id\":" << unitId;
    commander << "}" << std::endl;
  }

  void observeExpansionStart(Unit u, int gameFrame) {
    if(!expansionPlan.source || u->getType()!=Protoss_Nexus || u->getTilePosition()!=expansionPlan.tile) return;
    logExpansion("started",expansionPlan,gameFrame,u->getID());
    if(expansionPlan.requestFrame>=0) expansionStatus="started";
    expansionConstruction.emplace(u->getID(),expansionPlan);
    expansionPlan=Expansion{};
  }

  void finishExpansion(Unit u, const char* phase) {
    const auto it=expansionConstruction.find(u->getID());
    if(it==expansionConstruction.end()) return;
    logExpansion(phase,it->second,Broodwar->getFrameCount(),u->getID());
    if(it->second.requestFrame>=0) expansionStatus=phase;
    expansionConstruction.erase(it);
  }

  void build() {
    if(planned!=None) {
      bool started=false;
      for(auto u:own) if(u->getType()==planned && u->getTilePosition()==buildTile) {
        started=true;
        if(planned==Protoss_Nexus) observeExpansionStart(u,frame);
        break;
      }
      if(started || !builder || !builder->exists() || frame-planFrame>600) {
        if(!started && planned==Protoss_Photon_Cannon) {
          cannonBlockedUntil=frame+cannonRetryDelay;
          trace<<"{\"event\":\"cannon_skip\",\"frame\":"<<frame<<",\"reason\":\"plan_failed\",\"until\":"<<cannonBlockedUntil<<"}\n";
        }
        if(trace) trace<<"{\"event\":\"plan_end\",\"frame\":"<<frame<<",\"started\":"<<(started?"true":"false")<<"}\n";
        if(expansionPlan.source) {
          logExpansion("plan_failed",expansionPlan,frame);
          if(expansionPlan.requestFrame>=0) expansionStatus="plan_failed";
          expansionPlan=Expansion{};
        }
        planned=None;builder=nullptr;
      }
    }
    if(planned==None) {
      UnitType type=nextBuilding();
      if(type==None || me->minerals()<type.mineralPrice()-40 || me->gas()<type.gasPrice()) return;
      Position anchor=toward(home,enemyStart,160);
      TilePosition tile=TilePositions::Invalid;
      if(type==Protoss_Nexus) tile=expansion();
      else if(type==Protoss_Assimilator) {
        double best=1e9;
        for(auto g:Broodwar->getGeysers()) if(baseDistance(g->getPosition())<400 && Broodwar->canBuildHere(g->getTilePosition(),type)) {
          double d=baseDistance(g->getPosition()); if(d<best){best=d;tile=g->getTilePosition();}
        }
      } else {
        if(type==Protoss_Pylon) for(auto u:own) if(u->getType().requiresPsi() && u->isCompleted() && !u->isPowered()) {anchor=u->getPosition();break;}
        if(type==Protoss_Photon_Cannon) {
          anchor=home;
          for(auto n:nexuses) {bool covered=false;for(auto u:own) if(u->getType()==Protoss_Photon_Cannon && u->getDistance(n)<300) covered=true;
            if(!covered){anchor=n->getPosition();break;}}
        }
        tile=findSite(type,anchor);
      }
      if(!tile.isValid()) {
        if(type==Protoss_Photon_Cannon) {
          cannonBlockedUntil=frame+cannonRetryDelay;
          trace<<"{\"event\":\"cannon_skip\",\"frame\":"<<frame<<",\"reason\":\"no_site\",\"until\":"<<cannonBlockedUntil<<"}\n";
        }
        return;
      }
      double best=1e9;
      for(auto w:workers) if(w!=scout && !defenders.count(w->getID()) && !w->isGatheringGas()) {
        double d=w->getDistance(Position(tile)); if(w->isCarryingMinerals()) d+=100;
        if(d<best && w->hasPath(Position(tile))) {best=d;builder=w;}
      }
      if(!builder) return;
      planned=type;buildTile=tile;planFrame=frame;
      if(external && type==Protoss_Nexus) {
        const bool manual=expandUntil>=frame;
        expansionPlan={manual ? "manual" : "auto",manual ? expansionRequestFrame : -1,tile};
        if(manual) {
          expandUntil=-1;
          expansionStatus="planned";
        }
        logExpansion("planned",expansionPlan,frame);
      }
      trace<<"{\"event\":\"build_plan\",\"frame\":"<<frame<<",\"type\":\""<<type.getName()<<"\",\"x\":"<<tile.x<<",\"y\":"<<tile.y<<"}\n";
    }
    if(planned!=None && builder && commandReady(builder,24)) {
      if(!builder->build(planned,buildTile)) {
        std::string buildError=Broodwar->getLastError().toString();
        if(builder->getDistance(Position(buildTile)+Position(32,32))>80) builder->move(Position(buildTile)+Position(32,32));
        if(frame%120==0) trace<<"{\"event\":\"build_wait\",\"frame\":"<<frame<<",\"error\":\""<<buildError<<"\"}\n";
      }
    }
  }
  Unit visibleEnemy(int id) const {
    for(auto e:enemies) if(e->getID()==id && e->exists() && e->isVisible()) return e;
    return nullptr;
  }
  Unit pickDefenseTarget() const {
    Unit target=nullptr; double best=1e9;
    for(auto e:enemies) if(e->exists() && e->isVisible() && !e->isFlying() && e->isDetected() && baseDistance(e->getPosition())<defenseRadius) {
      UnitType t=e->getType();
      if(e->getID()==defenseRetryTarget && frame<defenseRetryUntil) continue;
      if(t==Protoss_Photon_Cannon && e->isCompleted() && e->getHitPoints()+e->getShields()>80) continue;
      if(t.isWorker() && !e->isAttacking()) continue;
      if(army.size()>=5 && !t.isBuilding()) continue;
      if(t==Protoss_Pylon || t==Protoss_Photon_Cannon || t.canAttack()) {
        double s=baseDistance(e->getPosition());
        if(t==Protoss_Photon_Cannon && !e->isCompleted()) s-=300;
        if(t==Protoss_Pylon) s-=100;
        if(t.isWorker()) s+=150;
        if(s<best) {best=s;target=e;}
      }
    }
    return target;
  }
  void restoreWorker(Unit w) {
    if(!w || !w->exists() || w==builder || w==scout || w->isConstructing()) return;
    Unit mineral=nullptr; double best=1e9;
    for(auto m:Broodwar->getMinerals()) if(m->getResources()>0 && baseDistance(m->getPosition())<380) {
      double d=w->getDistance(m);
      if(d<best) {best=d;mineral=m;}
    }
    // If gathering fails or no local mineral survives, cancel the stale attack.
    // The existing economy manager retries eligible work on its next 24-frame tick.
    if(!mineral || !w->gather(mineral)) w->stop();
  }
  void releaseDefenders(const char* reason,bool restore=true) {
    if(restore) for(auto w:workers) if(defenders.count(w->getID())) restoreWorker(w);
    trace<<"{\"event\":\"defense_end\",\"frame\":"<<frame<<",\"reason\":\""<<reason
         <<"\",\"target_id\":"<<defenseTarget<<",\"target\":\""<<defenseTargetType.getName()
         <<"\",\"workers\":"<<defenders.size()<<"}\n";
    defenders.clear(); defenseTarget=-1; defenseTargetType=None;
  }
  void workerDefense() {
    Unit current=defenseTarget>=0?visibleEnemy(defenseTarget):nullptr;
    if(defenseTarget>=0 && !current) releaseDefenders("lost");
    if(current) {
      bool engaged=current->isAttacking();
      for(auto w:workers) if(defenders.count(w->getID()) && w->getDistance(current)<contactRange) engaged=true;
      if(engaged && baseDistance(current->getPosition())<defenseRadius) lastContact=frame;
      const char* reason=nullptr;
      if(!current->isDetected()) reason="undetected";
      else if(current->isFlying()) reason="flying";
      else if(current->getType()==Protoss_Photon_Cannon && current->isCompleted() && current->getHitPoints()+current->getShields()>80) reason="unsafe_cannon";
      else if(baseDistance(current->getPosition())>defenseRadius+chaseMargin) reason="boundary";
      else if(frame-lastContact>contactTimeout) {
        reason="no_contact";
        // Do not immediately reacquire the same unreachable idle target.
        defenseRetryTarget=current->getID(); defenseRetryUntil=frame+contactTimeout;
      }
      else if(army.size()>=5 && !current->getType().isBuilding()) reason="army";
      if(reason) {releaseDefenders(reason);current=nullptr;}
    }
    Unit fresh=pickDefenseTarget();
    if(current && fresh && fresh!=current && current->getType().isWorker() && !fresh->getType().isWorker()) {
      releaseDefenders("switch");current=nullptr;
    }
    if(!current && fresh) {
      current=fresh; defenseTarget=fresh->getID(); defenseTargetType=fresh->getType(); lastContact=frame;
      trace<<"{\"event\":\"defense_start\",\"frame\":"<<frame<<",\"target\":\""<<defenseTargetType.getName()
           <<"\",\"target_id\":"<<defenseTarget<<"}\n";
    }
    if(!current) return;
    int wanted=current->getType().isWorker()?1:(current->getType().isBuilding()?6:4);
    wanted=std::min(wanted,std::max(0,int(workers.size())-4));
    for(auto it=defenders.begin();it!=defenders.end();) {
      Unit w=Broodwar->getUnit(*it);
      if(!w || !w->exists() || w==builder || w==scout || w->isConstructing() || (w->getShields()<5 && w->getHitPoints()<15)) {
        restoreWorker(w); it=defenders.erase(it);
      } else ++it;
    }
    // Casualties can shrink the reserve after assignment. Release excess workers,
    // preserving the nearest existing defenders; never replace a healthy member.
    if(int(defenders.size())>wanted) {
      std::vector<Unit> active;
      for(auto w:workers) if(defenders.count(w->getID())) active.push_back(w);
      std::sort(active.begin(),active.end(),[&](Unit a,Unit b){return a->getDistance(current)>b->getDistance(current);});
      for(auto w:active) {
        if(int(defenders.size())<=wanted) break;
        restoreWorker(w); defenders.erase(w->getID());
      }
    }
    std::vector<Unit> candidates=workers;
    std::sort(candidates.begin(),candidates.end(),[&](Unit a,Unit b){return a->getDistance(current)<b->getDistance(current);});
    for(auto w:candidates) {
      if(int(defenders.size())>=wanted) break;
      if(w==builder || w==scout || w->isConstructing() || defenders.count(w->getID())) continue;
      if(w->getShields()<5 && w->getHitPoints()<15) continue;
      defenders.insert(w->getID());
    }
    for(auto w:workers) if(defenders.count(w->getID())) hit(w,current);
  }
  void economy() {
    std::vector<Unit> minerals,refineries;
    for(auto m:Broodwar->getMinerals()) if(m->getResources()>0 && baseDistance(m->getPosition())<380) minerals.push_back(m);
    for(auto u:own) if(u->getType()==Protoss_Assimilator && u->isCompleted() && u->getResources()>0) refineries.push_back(u);
    std::map<int,int> assigned;
    for(auto w:workers) if(w->getOrderTarget()) assigned[w->getOrderTarget()->getID()]++;
    int gasTarget=me->gas()<100?3:(me->gas()<240?2:0);
    for(auto w:workers) {
      if(w==builder || w==scout || defenders.count(w->getID()) || w->isConstructing()) continue;
      if(!commandReady(w,24)) continue;
      if(w->isCarryingMinerals() || w->isCarryingGas()) { if(w->isIdle()) w->returnCargo(); continue; }
      Unit current=w->getOrderTarget(),target=nullptr;
      if(current && w->isGatheringGas() && assigned[current->getID()]<=gasTarget) continue;
      for(auto g:refineries) if(assigned[g->getID()]<gasTarget && w->getDistance(g)<450) {target=g;break;}
      if(!target && current && w->isGatheringMinerals() && current->getResources()>0 && (assigned[current->getID()]<=2 || w->getDistance(current)<72)) continue;
      if(!target) {
        double best=1e9;
        for(auto m:minerals) {
          double score=w->getDistance(m)+assigned[m->getID()]*100;
          if(score<best){best=score;target=m;}
        }
      }
      if(target && target!=current) { if(current) --assigned[current->getID()]; ++assigned[target->getID()]; w->gather(target); }
      else if(target && w->isIdle()) w->gather(target);
    }
  }
  bool train(Unit u,UnitType type) {
    if(cash<type.mineralPrice() || gas<type.gasPrice() || !u->canTrain(type)) return false;
    if(u->train(type)){cash-=type.mineralPrice();gas-=type.gasPrice();return true;} return false;
  }
  bool upgrade(Unit u,UpgradeType type) {
    if(me->getUpgradeLevel(type)>0 || me->isUpgrading(type) || cash<type.mineralPrice() || gas<type.gasPrice()) return false;
    if(u->canUpgrade(type) && u->upgrade(type)){cash-=type.mineralPrice();gas-=type.gasPrice();return true;} return false;
  }
  void production() {
    UnitType desired=nextBuilding();
    UnitType detectStep=None;
    if(cloakSeen || cloakTechSeen) {
      bool roboIdle=false;
      for(auto u:own) if(u->getType()==Protoss_Robotics_Facility && u->isCompleted() && u->isPowered() && u->isIdle()) roboIdle=true;
      if(roboIdle && count(Protoss_Observatory,true) && queued(Protoss_Observer)==0) detectStep=Protoss_Observer;
      else if((desired==Protoss_Robotics_Facility || desired==Protoss_Observatory) && planned!=desired) detectStep=desired;
    }
    if(detectStep!=lastDetectStep) {
      trace<<"{\"event\":\"detect_step\",\"frame\":"<<frame<<",\"step\":\""<<detectStep.getName()<<"\",\"gas\":"<<me->gas()<<"}\n";
      lastDetectStep=detectStep;
    }
    // Reserve only an actual building task, an urgent pylon, or an intentional expansion.
    // Reserving every future 150-200 mineral structure was repeatedly idling the Nexus.
    UnitType reserve=planned!=None?planned:((desired==Protoss_Nexus || (desired==Protoss_Pylon && me->supplyTotal()-me->supplyUsed()<=4))?desired:None);
    cash=me->minerals()-(reserve!=None?reserve.mineralPrice():0);
    gas=me->gas()-(reserve!=None?reserve.gasPrice():0);
    if(detectStep!=None && detectStep!=Protoss_Observer) {
      cash-=detectStep.mineralPrice(); gas-=detectStep.gasPrice();
    }
    int workerGoal=std::min(64,23+18*std::max(0,int(nexuses.size())-1));
    // Maintain economic growth; the first two Zealots still get an emergency priority.
    if(homeThreat && army.size()<2) for(auto u:own) if(u->getType()==Protoss_Gateway && u->isCompleted() && u->isPowered() && u->isIdle()) train(u,Protoss_Zealot);
    for(auto n:nexuses) if(n->isIdle() && queued(Protoss_Probe)<workerGoal) train(n,Protoss_Probe);
    bool rangeNeeded=detectStep==None && count(Protoss_Dragoon)>=1 && !me->getUpgradeLevel(UpgradeTypes::Singularity_Charge) && !me->isUpgrading(UpgradeTypes::Singularity_Charge);
    if(rangeNeeded) {
      bool began=false;
      for(auto u:own) if(u->getType()==Protoss_Cybernetics_Core && u->isCompleted() && u->isPowered()) {
        if(upgrade(u,UpgradeTypes::Singularity_Charge)) {began=true;rangeStart=frame;trace<<"{\"event\":\"range_upgrade_started\",\"frame\":"<<frame<<"}\n";break;}
      }
      if(!began) {cash-=UpgradeTypes::Singularity_Charge.mineralPrice();gas-=UpgradeTypes::Singularity_Charge.gasPrice();}
    }
    for(auto u:own) if(u->isCompleted() && u->isPowered()) {
      if(u->getType()==Protoss_Robotics_Facility && u->isIdle()) {
        int observers=(cloakSeen || nexuses.size()>1)?2:1;
        if(queued(Protoss_Observer)<observers && count(Protoss_Observatory,true)) train(u,Protoss_Observer);
        else if(count(Protoss_Robotics_Support_Bay,true) && queued(Protoss_Reaver)<2) train(u,Protoss_Reaver);
      }
      if(u->getType()==Protoss_Reaver && u->getScarabCount()<5 && u->getTrainingQueue().empty()) train(u,Protoss_Scarab);
      if(detectStep==None && u->getType()==Protoss_Forge && army.size()>=10) upgrade(u,UpgradeTypes::Protoss_Ground_Weapons);
      if(detectStep==None && u->getType()==Protoss_Citadel_of_Adun) upgrade(u,UpgradeTypes::Leg_Enhancements);
    }
    for(auto u:own) if(u->getType()==Protoss_Gateway && u->isCompleted() && u->isPowered() && u->isIdle()) {
      bool dragoon=detectStep==None && count(Protoss_Cybernetics_Core,true) && gas>=50 && queued(Protoss_Dragoon)<2*std::max(3,count(Protoss_Zealot));
      if(!train(u,dragoon?Protoss_Dragoon:Protoss_Zealot) && cash>200) train(u,Protoss_Zealot);
    }
  }
  void scouting() {
    if(scout && (!scout->exists() || scout==builder)) scout=nullptr;
    if(!scout && frame>1600 && frame<3600 && workers.size()>=10) for(auto w:workers) if(w!=builder && !defenders.count(w->getID())) {scout=w;break;}
    if(!scout) return;
    if(frame>8000 || scout->getHitPoints()<15 || homeThreat) {move(scout,home);scout=nullptr;return;}
    Unit danger=nullptr;
    for(auto e:enemies) if(e->getType().canAttack() && (!e->getType().isWorker() || e->isAttacking()) && scout->getDistance(e)<200) {danger=e;break;}
    if(danger) move(scout,toward(scout->getPosition(),danger->getPosition(),-128));
    else move(scout,clamp(enemyStart+Position(((frame/240)%2)*192-96,((frame/480)%2)*192-96)));
  }
  Position objective() const {
    if(!enemyBuildings.empty()) {
      Position best=enemyBuildings.begin()->second; double d=1e9;
      for(auto entry:enemyBuildings) {double v=entry.second.getDistance(home);if(v<d){d=v;best=entry.second;}}
      return best;
    }
    if(!Broodwar->isExplored(TilePosition(enemyStart))) return enemyStart;
    if(!bases.empty()) return Position(bases[(frame/720)%bases.size()])+Position(64,48);
    return enemyStart;
  }
  double power(Unit u) const {
    UnitType t=u->getType();
    if(!u->isCompleted() || (!t.canAttack() && t!=Protoss_Reaver)) return 0;
    double base=t.isWorker()?.4:(t==Protoss_Zealot?2.0:(t==Protoss_Dragoon?3.0:(t==Protoss_Reaver?7.0:(t==Protoss_Photon_Cannon?4.0:3.0))));
    return base*std::max(.2,double(u->getHitPoints()+u->getShields())/std::max(1,t.maxHitPoints()+t.maxShields()));
  }
  Position detectionPoint(Unit u) const {
    Position best=Positions::None; double distance=1e18;
    for(auto o:own) {
      bool cannon=o->getType()==Protoss_Photon_Cannon && o->isCompleted() && o->isPowered();
      bool observer=o->getType()==Protoss_Observer && o->isCompleted();
      if(!cannon && !observer) continue;
      Position p=o->getPosition();
      if(!p.isValid() || !u->hasPath(p)) continue;
      double d=u->getDistance(o);
      if(d<distance) {distance=d;best=p;}
    }
    return best;
  }
  void moveWithoutAttacking(Unit u,Position p) {
    // An attack order may already target a nearby point. It must not survive just
    // because the generic move() distance threshold would suppress this command.
    if(commandReady(u) && (u->getOrder()!=Orders::Move || u->isIdle() || u->getTargetPosition().getDistance(p)>48 || u->isStuck())) u->move(p);
  }
  Position armyCenter() const {
    if(army.empty()) return home;
    long x=0, y=0;
    for(auto u:army) { x+=u->getPosition().x; y+=u->getPosition().y; }
    return Position(int(x/army.size()), int(y/army.size()));
  }

  Position armyGoal() const {
    if(!external) return homeThreat ? homeThreat->getPosition() : (attacking ? objective() : rally);
    if(stance==Stance::Defend) return homeThreat ? homeThreat->getPosition() : home;
    if(stance==Stance::Attack) return objective();
    return stance==Stance::Retreat ? home : rally;
  }

  void combat() {
    if(external) {
      if(stance==Stance::Retreat && !army.empty() &&
         std::all_of(army.begin(),army.end(),[&](Unit u) { return u->getDistance(home)<220; })) {
        stance=Stance::Rally;
        commander << "{\"event\":\"retreat_complete\",\"frame\":" << frame << "}" << std::endl;
      }
      attacking=stance==Stance::Attack;
    } else {
      if(army.size()>=6) attacking=true;
      if(army.size()<3) attacking=false;
    }
    const Position goal=armyGoal(), center=armyCenter();
    const bool defendingThreat=homeThreat && (!external || stance==Stance::Defend);
    for(auto u:army) {
      if(external && stance==Stance::Retreat) { moveWithoutAttacking(u,goal); continue; }
      if(demo && stance==Stance::Defend && u->getDistance(goal)>320) {
        moveWithoutAttacking(u,goal);
        continue;
      }
      if(u->getType()==Protoss_Reaver && u->getScarabCount()==0) {move(u,toward(center,home,120));continue;}
      Unit target=nullptr, undetectedThreat=nullptr; double best=1e9,foe=0,friendPower=0;
      for(auto a:army) if(a->getDistance(u)<320) friendPower+=power(a);
      for(auto e:enemies) {
        if(u->getDistance(e)<400) foe+=power(e);
        if(!e->isDetected() && !e->isFlying() && e->getType().canAttack() && u->getDistance(e)<224 &&
           (!undetectedThreat || u->getDistance(e)<u->getDistance(undetectedThreat))) undetectedThreat=e;
        if(!e->isDetected() || !u->canAttack(e) || u->getDistance(e)>=400) continue;
        double score=u->getDistance(e);
        if(e->getType().canAttack() && !e->getType().isWorker()) score-=100;
        if(e->getType()==Protoss_Reaver || e->getType()==Protoss_High_Templar || e->getType()==Protoss_Dark_Templar) score-=180;
        if(e->getType().isWorker()) score-=20;
        if(e->getType()==Protoss_Pylon) score+=50;
        score+=(e->getHitPoints()+e->getShields())*.12;
        if(score<best){best=score;target=e;}
      }
      if(undetectedThreat) {
        Position safe=detectionPoint(u);
        if(safe!=Positions::None) {moveWithoutAttacking(u,safe);continue;}
      }
      // Preserve reinforcements instead of feeding isolated units into a larger force.
      if(target && foe>friendPower*1.25 && baseDistance(u->getPosition())>430) {
        Position retreat=toward(center,home,180);
        if(u->getDistance(retreat)<80) retreat=toward(u->getPosition(),home,180);
        move(u,retreat);continue;
      }
      if(u->getShields()<4 && u->getHitPoints()<u->getType().maxHitPoints()/3 && target && u->getDistance(home)>250) {
        move(u,toward(u->getPosition(),home,144));continue;
      }
      if(target && u->getType()==Protoss_Dragoon && u->getGroundWeaponCooldown()>6 && !target->isFlying() && u->getDistance(target)<(me->getUpgradeLevel(UpgradeTypes::Singularity_Charge)?170:105)) {
        move(u,toward(u->getPosition(),target->getPosition(),-80));continue;
      }
      if(target) hit(u,target);
      else if(undetectedThreat) {
        // Do not chase an untargetable attacker while waiting for detection.
        move(u,toward(u->getPosition(),undetectedThreat->getPosition(),-128));
      } else if(attacking && !defendingThreat && u->getDistance(center)>320 && army.size()>=6) advance(u,center);
      else advance(u,goal);
    }
    int index=0;
    for(auto u:own) if(u->getType()==Protoss_Observer && u->isCompleted()) {
      Position destination=(index++==0 || !defendingThreat)?toward(center,goal,64):homeThreat->getPosition();
      Unit danger=nullptr;
      for(auto e:enemies) if(e->getType().airWeapon()!=WeaponTypes::None && e->getDistance(u)<200 && e->isDetected()) {danger=e;break;}
      if(danger) destination=toward(u->getPosition(),danger->getPosition(),-96);
      move(u,destination);
    }
  }

  const char* requestExpansion() {
    if(expandUntil>=frame || expansionInProgress()) return "expansion_pending";
    if(count(Protoss_Nexus)>=3) return "base_limit";
    if(me->minerals()<Protoss_Nexus.mineralPrice()) return "not_enough_minerals";
    if(!expansion().isValid()) return "no_valid_site";
    expandUntil=frame+600;
    expansionRequestFrame=frame;
    return "queued";
  }

  const char* applyCommand(Action action) {
    switch(action) {
      case Action::Attack: stance=Stance::Attack; break;
      case Action::Defend: stance=Stance::Defend; break;
      case Action::Rally: stance=Stance::Rally; break;
      case Action::Retreat: stance=Stance::Retreat; break;
      case Action::Expand:
        expansionStatus=requestExpansion();
        return expansionStatus;
      case Action::Invalid: return "invalid_action";
    }
    return "applied";
  }

  void pollDemoKeys() {
    for(WPARAM key:demoKeys) {
      const int index=int(key-VK_F5);
      const Action action=static_cast<Action>(index);
      keyCommands.push_back({action,frame});
      commander << "{\"event\":\"key_received\",\"frame\":" << frame
                << ",\"key\":\"F" << index+5 << "\",\"action\":\"" << actionName(action) << "\"}" << std::endl;
    }
    demoKeys.clear();
    Broodwar->drawTextScreen(8,32,"F5 Attack | F6 Defend | F7 Rally | F8 Retreat | F9 Expand");
    Broodwar->drawTextScreen(8,46,"Army: %s | Last F9: %s | Nexus: %s",
                            stanceName(),expansionStatus,expansionInProgress()?"busy":"idle");
  }

  void applyDemoKeys() {
    for(const KeyCommand& key:keyCommands) {
      const char* status=applyCommand(key.action);
      appliedKeyFrame=frame;
      commander << "{\"event\":\"command\",\"source\":\"keyboard\",\"source_frame\":" << key.frame
                << ",\"frame\":" << frame << ",\"action\":\"" << actionName(key.action)
                << "\",\"status\":\"" << status << "\",\"stance\":\"" << stanceName() << "\"}" << std::endl;
    }
    keyCommands.clear();
  }

  void readCommands() {
    std::ifstream commands("bwapi-data/write/commander.commands",std::ios::binary);
    if(!commands) return;
    commands.seekg(commandOffset);
    std::string line, action;
    // A final line without a newline may still be in flight.
    while(std::getline(commands,line) && !commands.eof()) {
      commandOffset=commands.tellg();
      std::istringstream fields(line);
      int seq, sourceFrame;
      if(!(fields>>seq>>sourceFrame>>action) || seq<=lastSequence) continue;
      lastSequence=seq;
      const Action requested=parseAction(action);
      const char* status=sourceFrame<0 || sourceFrame>lastSnapshot || frame-sourceFrame>2*decisionFrames
                       ? "stale" : applyCommand(requested);
      commander << "{\"event\":\"command\",\"source\":\"cli\",\"seq\":" << seq
                << ",\"source_frame\":" << sourceFrame << ",\"frame\":" << frame
                << ",\"action\":\"" << actionName(requested) << "\",\"status\":\"" << status
                << "\",\"stance\":\"" << stanceName() << "\"}" << std::endl;
    }
  }

  void publishState() {
    lastSnapshot=frame;
    const Position center=armyCenter(), goal=armyGoal();
    commander << "{\"event\":\"state\",\"frame\":" << frame << ",\"stance\":\"" << stanceName()
              << "\",\"minerals\":" << me->minerals() << ",\"gas\":" << me->gas() << ",\"workers\":" << workers.size()
              << ",\"bases\":" << nexuses.size() << ",\"army_count\":" << army.size() << ",\"home\":[" << home.x << ',' << home.y
              << "],\"center\":[" << center.x << ',' << center.y << "],\"goal\":[" << goal.x << ',' << goal.y << ']'
              << ",\"home_threat\":" << (homeThreat ? "true" : "false")
              << ",\"expansion_pending\":" << ((expandUntil>=frame || expansionInProgress()) ? "true" : "false")
              << ",\"expansion_status\":\"" << expansionStatus << '"';
    std::map<std::string,int> forces;
    for(auto u:army) ++forces[u->getType().getName()];
    commander << ",\"army\":{";
    const char* comma="";
    for(const auto& unit:forces) {
      commander << comma << '"' << unit.first << "\":" << unit.second;
      comma=",";
    }
    commander << "},\"visible_enemies\":[";
    comma="";
    for(auto enemy:enemies) {
      const Position pos=enemy->getPosition();
      commander << comma << "{\"type\":\"" << enemy->getType().getName() << "\",\"x\":" << pos.x << ",\"y\":" << pos.y << '}';
      comma=",";
    }
    commander << "],\"known_enemy_buildings\":[";
    comma="";
    for(const auto& building:enemyBuildings) {
      commander << comma << "{\"x\":" << building.second.x << ",\"y\":" << building.second.y
                << ",\"last_seen\":" << buildingLastSeen.at(building.first) << '}';
      comma=",";
    }
    commander << "]}" << std::endl;
  }

public:
  ~AstraBot() override {
    if(demoKeyHook) UnhookWindowsHookEx(demoKeyHook);
  }

  void onStart() override {
    me=Broodwar->self();home=Position(me->getStartLocation())+Position(64,48);enemyStart=home;
    for(auto t:Broodwar->getStartLocations()) if(t!=me->getStartLocation()) enemyStart=Position(t)+Position(64,48);
    rally=toward(home,enemyStart,220);
    BWEM::Map::Instance().Initialize(BroodwarPtr);
    BWEM::Map::Instance().FindBasesForStartingLocations();
    for(const auto &area:BWEM::Map::Instance().Areas()) for(const auto &base:area.Bases()) bases.push_back(base.Location());
    const char* mode=std::getenv("STARCRAFT_COMMAND_MODE");
    demo=mode && std::string(mode)=="demo";
    external=demo || (mode && std::string(mode)=="external");
    const std::string logFolder="bwapi-data/write/";
    std::string suffix;
    if(demo) {
      for(int round=2; std::ifstream(logFolder+"astra-trace"+suffix+".jsonl").good(); ++round)
        suffix="-"+std::to_string(round);
    }
    trace.open(logFolder+"astra-trace"+suffix+".jsonl");
    if(demo) {
      Broodwar->enableFlag(Flag::UserInput);
      demoKeyHook=SetWindowsHookExW(WH_GETMESSAGE,receiveDemoKey,nullptr,GetCurrentThreadId());
      if(!demoKeyHook) trace << "{\"event\":\"keyboard_hook_error\",\"win32_error\":" << GetLastError() << "}" << std::endl;
    }
    if(external) commander.open(logFolder+"commander-state"+suffix+".jsonl");
    trace<<"{\"event\":\"start\",\"variant\":\"v6-fix-r1-20261110\",\"command_mode\":\""
         <<(demo?"demo":external?"external":"internal")<<"\",\"bases\":"<<bases.size()<<"}"<<std::endl;
  }
  void onFrame() override {
    frame=Broodwar->getFrameCount();
    if(demo) pollDemoKeys();
    if(frame%8 || (demo && lastDemoTick==frame)) return;
    if(demo) lastDemoTick=frame;
    refresh();
    if(external) {
      if(demo) applyDemoKeys();
      else readCommands();
      if(expandUntil>=0 && frame>expandUntil) {
        expandUntil=-1;
        expansionStatus="expired";
        logExpansion("expired",Expansion{"manual",expansionRequestFrame},frame);
      }
    }
    workerDefense();recordDefenderUsage();
    if(frame%24==0){build();economy();production();scouting();}
    combat();
    if(demo && frame%24==0 && !army.empty()) {
      const Position center=armyCenter();
      Broodwar->setScreenPosition(std::max(0,center.x-320),std::max(0,center.y-200));
    }
    if(external && (frame%decisionFrames==0 || frame==appliedKeyFrame)) publishState();
    if(frame%240==0) {
      trace<<"{\"event\":\"state\",\"frame\":"<<frame<<",\"workers\":"<<workers.size()<<",\"army\":"<<army.size()<<",\"minerals\":"<<me->minerals()<<",\"gas\":"<<me->gas()<<",\"attacking\":"<<(attacking?"true":"false")<<",\"range_level\":"<<me->getUpgradeLevel(UpgradeTypes::Singularity_Charge)<<",\"range_start\":"<<rangeStart<<",\"nexuses\":"<<nexuses.size()<<",\"defenders\":"<<defenders.size()<<",\"defender_frames\":"<<defenderFrames<<",\"distinct_defenders\":"<<defendersEver.size()<<",\"planned\":\""<<planned.getName()<<"\"}\n";trace.flush();
    }
  }
  void onUnitCreate(Unit u) override {
    if(expansionPlan.source && u && u->getPlayer()==me)
      observeExpansionStart(u,Broodwar->getFrameCount());
  }
  void onUnitComplete(Unit u) override {
    if(!u || u->getPlayer()!=Broodwar->self()) return;
    UnitType t=u->getType();
    if(t==Protoss_Nexus) finishExpansion(u,"completed");
    if(t==Protoss_Robotics_Facility || t==Protoss_Observatory || t==Protoss_Observer || t==Protoss_Photon_Cannon)
      trace<<"{\"event\":\"unit_complete\",\"frame\":"<<Broodwar->getFrameCount()<<",\"type\":\""<<t.getName()
           <<"\",\"unit_id\":"<<u->getID()<<",\"x\":"<<u->getPosition().x<<",\"y\":"<<u->getPosition().y<<"}\n";
  }
  void onUnitDestroy(Unit u) override {
    if(u && !expansionConstruction.empty()) finishExpansion(u,"destroyed");
    recordOwnLoss(u);
    undetectedSnapshots.erase(std::remove_if(undetectedSnapshots.begin(),undetectedSnapshots.end(),
      [u](const UndetectedSnapshot &e){return e.id==u->getID();}),undetectedSnapshots.end());
    ownPositions.erase(u->getID());
    enemyBuildings.erase(u->getID());
    buildingLastSeen.erase(u->getID());
    if(u->getType().isMineralField()) BWEM::Map::Instance().OnMineralDestroyed(u);
    else if(u->getPlayer()==Broodwar->neutral() && u->getType().isBuilding()) BWEM::Map::Instance().OnStaticBuildingDestroyed(u);
  }
  void onEnd(bool winner) override {
    if(external) commander << "{\"event\":\"end\",\"frame\":" << Broodwar->getFrameCount()
                           << ",\"winner\":" << (winner ? "true" : "false") << "}" << std::endl;
    frame=Broodwar->getFrameCount();
    // Close the trace interval, but issue no worker orders after game end.
    if(defenseTarget>=0) releaseDefenders("game_end",false);
    trace<<"{\"event\":\"end\",\"winner\":"<<(winner?"true":"false")
         <<",\"defender_frames\":"<<defenderFrames<<",\"distinct_defenders\":"<<defendersEver.size()<<"}\n";
    trace.close();
  }
};
extern "C" void gameInit(BWAPI::Game* game) { BWAPI::BroodwarPtr=game; }
extern "C" BWAPI::AIModule* newAIModule() { return new AstraBot(); }
