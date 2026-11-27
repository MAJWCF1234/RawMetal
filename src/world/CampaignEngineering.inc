// The Service Spine encounter is authored as the same records Builder exports.
void buildEngineeringDirector(Map& m, float floor, float roof) {
 using Type = ScriptAction::Type;
 using Op = ScriptCondition::Op;
 auto identify=[&](const char* tag,int index,const char* label) {
  m.recordFields[recordFieldKey(tag,"",index)]={{"editorlabel",label}};
 };
 auto state=[&](const char* name) {
  const auto id=stateId(name);
  if(std::none_of(m.stateSpellings.begin(),m.stateSpellings.end(),[&](const auto& pair){return pair.first==id;}))
   m.stateSpellings.push_back({id,name});
  return id;
 };
 auto set=[&](const char* name,int value) { state(name);return drSet(name,value); };
 auto wait=[&](const char* name,int value,float seconds) {
  auto action=set(name,value);action.type=Type::Later;action.amount=seconds;return action;
 };
 auto sound=[](Sound clip,int radius) {
  ScriptAction action;action.type=Type::Noise;action.sound=clip;action.amount=.8f;action.value=radius;return action;
 };
 auto door=[&](float left,float right,float axis,bool alongY,const char* name,
               const char* flag,int value,bool open,int leaf) {
  const int index=int(m.doors.size());
  Door d{left,right,axis};d.alongY=alongY;d.open=open?1.f:0.f;d.opening=open;
  d.requireState=state(flag);d.requireValue=value;d.label=name;d.lock=3;d.leaf=leaf;
  m.doors.push_back(std::move(d));identify("DOOR",index,name);return index;
 };
 auto rule=[&](const char* suffix,const char* title,const char* group,
               float x1,float y1,float x2,float y2,std::vector<ScriptAction> steps) -> ScriptEvent& {
  const std::string id=std::string("engineering_rule_")+suffix;
  state(id.c_str());
  identify("SCRIPT_EVENT",int(m.scriptEvents.size()),title);
  m.recordFields[recordFieldKey("SCRIPT_EVENT","",int(m.scriptEvents.size()))].push_back({"editorgroup",group});
  if(std::string_view(title)!=group)
   m.recordFields[recordFieldKey("SCRIPT_EVENT","",int(m.scriptEvents.size()))].push_back({"editorhelper","1"});
  drEvent(m,id.c_str(),x1,y1,x2,y2,"",std::move(steps));
  return m.scriptEvents.back();
 };
 auto gate=[&](ScriptEvent& event,const char* name,int value=1,Op op=Op::Equal) {
  event.conditions.push_back({ScriptCondition::Source::State,state(name),op,value});
 };
 auto annotate=[&](ScriptEvent& event,int action,const char* target) {
  const int index=int(&event-m.scriptEvents.data());
  m.recordFields[recordFieldKey("SCRIPT_ACTION",std::to_string(index),action)]={{"editortarget",target}};
 };

 AiAgent director;director.name="engineering";director.id=state("engineering");
 director.x2=24;director.y2=24;director.bottom=floor-5;director.top=roof;
 director.pressure=.85f;director.calm=34;director.peak=50;
 director.tension=state("engineering_tension");director.inside=state("engineering_inside");
 director.beat=state("engineering_beat");director.quiet=state("engineering_quiet");
 director.pulse=state("engineering_pulse");director.mood=state("engineering_mood");
 identify("AI",int(m.aiAgents.size()),"Engineering Director");
 m.aiAgents.push_back(director);

 // The regular east crossing stays usable until the hall has been entered.
 wall(m,21,1.f,21.2f,13.4f,floor,roof);
 wall(m,21,15.6f,21.2f,16.3f,floor,roof);
 wall(m,21,17.3f,21.2f,23.f,floor,roof);
 wall(m,21,16.3f,21.2f,17.3f,floor+1.06f,roof);
 const auto entry=door(13.4f,15.6f,21.1f,true,"Robotics hall doors","eng_lockdown",0,true,0);
 const auto vent=door(16.3f,17.3f,21.1f,true,"Wall vent","eng_power_restored",1,false,1);
 wall(m,8.0f,17.8f,10.0f,18.0f,floor,roof);
 wall(m,12.0f,17.8f,15.0f,18.0f,floor,roof);
 const auto chapterDoor=door(10,12,17.9f,false,"Ch3 door","eng_power_restored",1,false,0);
 wallSign(m,20.2f,12.9f,floor+1.6f,"ROBOTICS HALL","SERVICE ENTRY",1.5f,drW);
 wallSign(m,20.5f,16.8f,floor+1.4f,"WALL VENT","RETURN TO ACCESS CONTROL",1.4f,drW);
 ScriptAction flicker;flicker.type=Type::Lights;flicker.id=director.id;flicker.value=1;flicker.amount=6;
 auto& lockdown=rule("lockdown","Lockdown","Lockdown",17.5f,12.5f,20.5f,15.8f,
                     {set("eng_lockdown",1),drDoor(entry,false),flicker,sound(Sound::PaSystemLockdown,20)});
 m.recordFields[recordFieldKey("SCRIPT_EVENT","",int(m.scriptEvents.size()-1))].push_back({"editorarea","Robotics hall"});
 annotate(lockdown,0,"Lockdown started");annotate(lockdown,1,"Robotics hall doors");
 annotate(lockdown,2,"Robotics hall lamps");annotate(lockdown,3,"Lockdown alarm");

 const char* rows[]={"Cage row 1","Cage row 2","Cage row 3"};
 const char* wake[]={"eng_cage_1","eng_cage_2","eng_cage_3"};
 for(int index=0;index<3;++index) {
  const float x=6.f+4.f*index;
  wall(m,x-.2f,6.0f,x,9.7f,floor,roof);
  wall(m,x+2.7f,6.0f,x+2.9f,9.7f,floor,roof);
  wall(m,x,6.0f,x+2.7f,6.2f,floor,roof);
  door(x,x+2.7f,9.6f,false,rows[index],wake[index],1,false,1);
  m.doors.back().follow=true;
  CreatureSpawn bot{CreatureKind::Machine,{x+1.3f,7.8f},floor};
  bot.wakeState=state(wake[index]);bot.health=65;
  identify("CREATURE",int(m.creatureSpawns.size()),("Cage robot "+std::to_string(index+1)).c_str());
  m.creatureSpawns.push_back(bot);
 }
 auto& cages=rule("cages","Cages wake","Cages wake",0,0,24,24,
                  {set(wake[0],1),wait(wake[1],1,20),wait(wake[2],1,40)});
 gate(cages,"eng_lockdown");for(int i=0;i<3;++i)annotate(cages,i,rows[i]);

 // The map's base floor cannot be pitted, so the stalker's bay is level with the hall.
 wall(m,15.85f,16,16.05f,23,floor,floor+.85f);
 wall(m,19.95f,16,20.15f,23,floor,floor+.85f);
 wall(m,16,22.8f,20,23,floor,floor+.85f);
 m.lights.push_back(drTube(18.6f,21.5f,roof,1.4f,4.f,2700));
 CreatureSpawn stalker{CreatureKind::Warden,{17.8f,21.5f},floor};
 stalker.wakeState=state("eng_stalker_awake");stalker.hunter=state("eng_skinbot");
 identify("CREATURE",int(m.creatureSpawns.size()),"Skin-suit robot");m.creatureSpawns.push_back(stalker);
 ScriptAction shyness;shyness.type=Type::SetState;shyness.id=hunterLightRetreatState(stalker.hunter);shyness.value=1;
 auto& hunt=rule("stalker","Stalker","Stalker",16.f,17.2f,19.7f,23,
                {shyness,set("eng_stalker_awake",1),sound(Sound::OneMetalDrag,0)});
 
 m.recordFields[recordFieldKey("SCRIPT_EVENT","",int(m.scriptEvents.size()-1))].push_back({"editorarea","Robotics sub-bay"});
 gate(hunt,"eng_lockdown");annotate(hunt,0,"Torch-shy retreat");
 annotate(hunt,1,"Skin-suit robot");annotate(hunt,2,"Wall pipes");

 // The Director sends the stalker looking: on each beat once it is up, the pipes ring where you were last heard.
 {ScriptAction search=sound(Sound::OneMetalDrag,30);search.index=2;
  auto& seek=rule("stalker_searches","Stalker searches","Stalker",0,0,24,24,{search});
  gate(seek,"eng_stalker_awake");gate(seek,"engineering_pulse");seek.once=false;seek.rearm=true;
  annotate(seek,0,"Search where you were heard");}

 const char* breakers[]={"Breaker 1","Breaker 2","Breaker 3"};
 const char* circuits[]={"eng_breaker_1","eng_breaker_2","eng_breaker_3"};
 for(int index=0;index<3;++index) {
  const float x=6.f+5.f*index;
  identify("TERMINAL",int(m.terminals.size()),breakers[index]);
  terminal(m,{x,11.7f},breakers[index],"RESTORE ROOM LIGHTING","CIRCUIT RESET",floor,state(circuits[index]));
  identify("LIGHT",int(m.lights.size()),index==0?"Robotics hall lamps":index==1?"Cage bay lamps":"Service bay lamps");
  auto tube=drTube(x,index==2?19.f:11.f,roof,1.4f,5.5f,4300);
  tube.enabledState=state(circuits[index]);m.lights.push_back(tube);
 }
 identify("AMBIENT",int(m.ambients.size()),"Vent fans");
 m.ambients.push_back({Sound::AmbFan,{20.f,16.8f},floor+1.25f,.65f,12,AmbientEmitter::Loop,0,0,state("eng_power_restored"),0});
 auto& power=rule("breakers","Breakers","Breakers",0,0,24,24,
                  {set("eng_power_restored",1),drDoor(vent,true),drDoor(chapterDoor,true)});
 gate(power,"eng_lockdown");for(const auto* circuit:circuits)gate(power,circuit);
 annotate(power,0,"Vent fans");annotate(power,1,"Wall vent");annotate(power,2,"Ch3 door");

 auto& pacing=rule("pacing","Pacing","Pacing",0,0,24,24,
                   {set("eng_quiet_phase",1),wait("eng_fake_due",1,34)});
 gate(pacing,"eng_lockdown");gate(pacing,"engineering_beat",1,Op::GreaterEqual);
 annotate(pacing,0,"Quiet phase");annotate(pacing,1,"Fake sound due");
 auto& fake=rule("quiet_fakes","Quiet fakes","Pacing",0,0,24,24,
                 {sound(Sound::PaEvacuate,0),sound(Sound::OneMetalDrag,0),wait("eng_fake_played",1,2)});
 gate(fake,"eng_quiet_phase");gate(fake,"eng_fake_due");
 annotate(fake,0,"Fake evacuation voice");annotate(fake,1,"Wall pipes");annotate(fake,2,"Fakes have played");
 auto& resume=rule("fight_returns","Fight returns","Pacing",0,0,24,24,
                   {set("eng_quiet_phase",0),set("eng_fight_resumed",1)});
 gate(resume,"eng_fake_played");gate(resume,"engineering_beat",2,Op::GreaterEqual);
 annotate(resume,0,"Quiet phase");annotate(resume,1,"Backup mining robot");
 CreatureSpawn backup{CreatureKind::Machine,{14.5f,11.f},floor};
 backup.wakeState=state("eng_fight_resumed");backup.health=65;
 identify("CREATURE",int(m.creatureSpawns.size()),"Backup mining robot");
 m.creatureSpawns.push_back(backup);
}
