#include "RouteCache.h"
#include "WalkQueryCache.h"
#include "../../core/FrameProfile.h"
#include <algorithm>
#include <bit>
#include <cstdlib>
namespace retro::ai {
namespace {
// Sixteen keys cover every hull a frame plans with (creature kinds, standing
// and crouched guards) on the player's chunk and its resident neighbours.
constexpr size_t Capacity=16;
struct Slot {
 std::vector<uint32_t> key;uint64_t hash=0,used=0;
 // Authored definitions are named by address in the key; the owner proves the
 // map at that address is still the one the entry was built on.
 std::weak_ptr<const AuthoredMapData> owner;bool owned=false;
 std::unique_ptr<RouteCache::Deck> deck;std::unique_ptr<RouteCache::Grid> grid;
};
struct Named {const World* world;const std::shared_ptr<const AuthoredMapData>* map;};
thread_local std::array<Slot,Capacity> slots;
thread_local uint64_t clock=0;
thread_local std::vector<uint32_t> scratch;
thread_local std::array<Named,8> owners;
thread_local int ownerCount=0;
enum Kind:uint32_t {DeckKey=1,GridKey=2};
Slot* find(const World&world,uint32_t kind,float height,float step,bool sweep,bool&fresh){
 fresh=false;if(!RouteCache::enabled)return nullptr;
 const std::shared_ptr<const AuthoredMapData>* owner=nullptr;
 if(world.authored()){for(int i=ownerCount-1;i>=0;--i)if(owners[size_t(i)].world==&world){owner=owners[size_t(i)].map;break;}if(!owner||!*owner)return nullptr;}
 collisionSnapshot(world,scratch);
 scratch.push_back(kind);scratch.push_back(std::bit_cast<uint32_t>(height));scratch.push_back(std::bit_cast<uint32_t>(step));scratch.push_back(sweep);
 // The doors a live door plan stands open, 32 to a word.
 const auto*plan=world.doorPlanDoors();const size_t doors=world.doors().size();
 for(size_t base=0;base<doors;base+=32){uint32_t bits=0;for(size_t i=base;i<std::min(doors,base+32);++i)if(plan&&i<plan->size()&&(*plan)[i])bits|=1u<<(i-base);scratch.push_back(bits);}
 uint64_t hash=1469598103934665603ull;for(auto value:scratch){hash^=value;hash*=1099511628211ull;}
 Slot* oldest=&slots[0];
 for(auto&slot:slots){
  bool sameOwner=owner?slot.owned&&!slot.owner.expired()&&!slot.owner.owner_before(*owner)&&!owner->owner_before(slot.owner):!slot.owned;
  if(slot.used&&slot.hash==hash&&sameOwner&&slot.key==scratch){slot.used=++clock;perf::count(perf::RouteCacheHits);return &slot;}
  if(slot.used<oldest->used)oldest=&slot;
 }
 perf::count(perf::RouteCacheMisses);fresh=true;
 auto&slot=*oldest;slot.key.swap(scratch);slot.hash=hash;slot.used=++clock;slot.owned=owner!=nullptr;slot.owner=owner?std::weak_ptr<const AuthoredMapData>(*owner):std::weak_ptr<const AuthoredMapData>{};
 slot.deck.reset();slot.grid.reset();
 if(kind==DeckKey)slot.deck=std::make_unique<RouteCache::Deck>();else slot.grid=std::make_unique<RouteCache::Grid>();
 return &slot;
}
}
RouteCache::Owner::Owner(const World&world,const std::shared_ptr<const AuthoredMapData>&map){
 if(ownerCount<int(owners.size())){owners[size_t(ownerCount++)]={&world,&map};m_pushed=true;}
}
RouteCache::Owner::~Owner(){if(m_pushed)--ownerCount;}
RouteCache::Deck* RouteCache::deck(const World&world,float height,bool&fresh){auto*slot=find(world,DeckKey,height,0,false,fresh);return slot?slot->deck.get():nullptr;}
RouteCache::Grid* RouteCache::grid(const World&world,float height,float step,bool sweep,bool&fresh){auto*slot=find(world,GridKey,height,step,sweep,fresh);return slot?slot->grid.get():nullptr;}
bool RouteCache::verify(){
 static const bool on=[]{
#ifdef _MSC_VER
  char*value=nullptr;size_t length=0;bool set=_dupenv_s(&value,&length,"RAWMETAL_ROUTE_CACHE_VERIFY")==0&&value&&*value&&*value!='0';std::free(value);return set;
#else
  const char*value=std::getenv("RAWMETAL_ROUTE_CACHE_VERIFY");return value&&*value&&*value!='0';
#endif
 }();
 return on;
}
}
