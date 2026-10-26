#pragma once
#include "Game.h"

namespace retro::regression {
inline std::shared_ptr<AuthoredMapData> room(){
 auto m=std::make_shared<AuthoredMapData>();m->name="Engine regression room";
 m->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
 AuthoredLayerData floor;for(auto& row:floor.rows)row=std::string(24,'.');
 floor.rows.front()=floor.rows.back()=std::string(24,'#');
 for(auto& row:floor.rows)row.front()=row.back()='#';m->layers.push_back(floor);
 // Autonomous actors still need a valid authored animation clock.
 TimedSequence clock;clock.timerState=stateId("reg_guard_clock");clock.finishState=stateId("reg_guard_done");clock.durationMs=60000;clock.finishAtMs=60000;clock.soundIntervalMs=0;clock.x1=clock.y1=-2;clock.x2=clock.y2=-1;
 m->timedSequences.push_back(clock);return m;
}
inline std::shared_ptr<CustomCampaign> campaign(std::shared_ptr<const AuthoredMapData> map){
 auto c=std::make_shared<CustomCampaign>();c->name="Engine regression";c->key=0x72656716;c->maps={map};return c;
}
inline ActorTrack guard(Vec2 start,Vec2 goal,AiFaction faction=AiFaction::Facility){
 ActorTrack a;a.timerState=stateId("reg_guard_clock");a.scale=1.6f;a.health=100;
 a.damageState=stateId("reg_guard_damage");a.deadState=stateId("reg_guard_dead");
 a.keys={{0,start,0,0},{60000,start,0,0}};a.ai.mode=ActorAiMode::Guard;a.ai.faction=faction;
 a.ai.patrol={{{goal.x,goal.y},0,60000}};return a;
}
inline std::shared_ptr<AuthoredMapData> doorRoom(){
 auto m=room();m->definition.playerStart={8.5f,11};
 m->structures.push_back({0,7.9f,7,8.1f,0,8});
 m->structures.push_back({10,7.9f,24,8.1f,0,8});
 m->doors.push_back({7,10,8});return m;
}
}

