#pragma once
// Map format 2 (docs/Systems/map-format.md): named fields on any record, entity
// names, per-entity overrides and the custom properties scripts read and write.
//
// A record's positional fields come first, as they always did. After them any
// number of `key=value` fields may follow (a file that starts with FORMAT|2):
//   FIXTURE|0|27|12.5|6|0.75|0.42|0.4|0.38|3.1416|1|name=desk_monitor|tint=1,0.4,0.4
// The parser keeps every named field, known or not, in the order written, so a
// newer key survives an older build's export and the Campaign Builder's save.
// The keys the engine knows are parsed here into EntityTraits; the systems that
// own a thing (Clutter, World, the renderer, creatures, actors) read them.
//
// The words for sides, orders, weapons and roles are the console summon's
// (game/Summon.cpp uses these same parsers), so an authored body can be given
// anything a summoned one can.
#include "../core/Math.h"
#include "ScriptDefinition.h"
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace retro {
struct NamedField {std::string key,value;bool operator==(const NamedField&)const=default;};
using NamedFields=std::vector<NamedField>;

// The map format version a file declares (FORMAT|n, the first record); a file
// without one is version 1, which has no named fields.
inline constexpr int MapFormatVersion=2;

// A key: a lower-case letter, then letters, digits, _ . or - (at most 48). Rows of
// tiles, titles and labels never start that way, so they are never read as keys.
bool namedFieldKey(std::string_view key);
// "key=value" with a valid key; the value is the text after the first '='.
bool splitNamedField(std::string_view field,NamedField& out);
// Takes the trailing named fields off `f` (the positional fields stay) and
// returns them in file order. A named field before a positional one is not
// named: the run is read from the end.
NamedFields peelNamedFields(std::vector<std::string>& f);
const NamedField* findNamedField(const NamedFields& fields,std::string_view key);
// An entity name: a letter or '_', then letters, digits, '_' or '-' (at most 48).
bool validEntityName(std::string_view name);
// A custom property key after "p.": letters, digits and '_' (at most 32).
bool validPropertyKey(std::string_view key);

// ---- The summon vocabulary (console summon and map overrides) ----
bool parseFactionWord(std::string_view word,bool creature,AiFaction& out);
const char* factionWord(AiFaction faction);
// A creature's standing order (Game::SummonOrder): idle patrol wander guard hunt follow.
bool parseOrderWord(std::string_view word,int& out);
const char* orderWord(int order);
// 0 none, 1 rifle (the shotgun class), 2 handgun.
bool parseWeaponWord(std::string_view word,int& out);
bool parseRoleWord(std::string_view word,GuardSquadRole& out);
// An authored man's AI: scripted civilian escort guard, or a summon order
// (idle -> civilian, follow -> escort, patrol/guard/hunt/wander -> guard).
bool parseActorAiWord(std::string_view word,ActorAiMode& out);

// The typed overrides of one record. -1 (or NaN, for floats) is "not given".
struct EntityTraits {
 std::string tag,name;int map=-1;
 // Physics (props: CLUTTER, and the chairs FIXTURE turns loose).
 float mass=-1,health=-1,density=-1,friction=-1,restitution=-1,volume=-1;
 signed char breakable=-1,floats=-1,frozen=-1,soft=-1,anchored=-1,solid=-1;
 int material=-1; // PropMaterial
 // Look: a colour the texture is multiplied by, a size factor, another model's
 // sheet, and a glow (0-4) in the texture's own colour (times the tint).
 Point3 tint{1,1,1};bool tinted=false;float scale=1;int texture=-1;float emissive=0;
 int impact=-1; // Sound: what it sounds like when it strikes something
 // Bodies (CREATURE, ACTOR): the summon traits.
 int faction=-1,order=-1,aiMode=-1,weapon=-1,role=-1;float hp=0;std::string display;
 // Custom properties (p.<key>=value), without the "p.".
 std::vector<std::pair<std::string,std::string>> properties;
 // A named prop's `frozen` property: scripts write it (SCRIPT_ACTION STATE @name.frozen 0) to let it go.
 StateId frozenState=0;
 bool looks()const{return tinted||emissive>0||texture>=0;}
 bool physical()const{return mass>0||health>=0||density>0||friction>=0||restitution>=0||volume>=0||breakable>=0||floats>=0||frozen>=0||soft>=0||anchored>=0||material>=0||impact>=0;}
 bool body()const{return faction>=0||order>=0||aiMode>=0||weapon>=0||role>=0||hp>0||!display.empty();}
 bool any()const{return looks()||physical()||body()||scale!=1||solid>=0||!properties.empty()||!name.empty();}
};
using EntityTraitsRef=std::shared_ptr<const EntityTraits>;
// The known keys of `fields` for a record of `tag`. Throws std::runtime_error
// naming the key for a bad value; unknown keys are left alone.
EntityTraits parseEntityTraits(std::string_view tag,const NamedFields& fields);
// The keys parseEntityTraits reads (the guide's named-field table documents each;
// tools/campaign/make_custom_map_guide.py NAMED_FIELDS must list the same).
const std::vector<std::string_view>& knownNamedKeys();
// Where a record's named fields are kept (AuthoredMapData::recordFields): its tag,
// the record it hangs off (an actor's index for ACTOR_*, an event's for SCRIPT_*,
// a node's for DIALOGUE_REPLY, a layer's for ROW; empty for the rest) and its
// ordinal among records of that tag and parent in the map (a ROW: its row).
std::string recordFieldKey(std::string_view tag,std::string_view parent,int ordinal);

// The script state that holds property `key` of entity `entity` on map `map`
// (its file index and name). SCRIPT_REQUIRE and SCRIPT_ACTION STATE/ADD name it
// as @entity.key (@N:entity.key for another map).
StateId entityPropertyState(int map,std::string_view mapName,std::string_view entity,std::string_view key);
// The state that marks a property seeded with its authored value (once per game).
StateId entityPropertySeedState(StateId property);
// A property's value as a script reads it: the integer it spells, else 0 (1 for "true"/"yes").
int entityPropertyValue(std::string_view text);
}
