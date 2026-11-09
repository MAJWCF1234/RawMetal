// Behavioral checks against the editor's real document and commit path.
static void verifyAdaptiveDressing(Editor& e){
  auto makeRoom=[&](const char* title){
    e.doc.newCampaign("Adaptive dressing regression");e.doc.addRoom(title);
    e.map=e.layer=0;e.record=-1;e.autoDressingEnabled=true;e.objectDrag=e.brushing=false;
    e.history.clear();e.future.clear();std::vector<int> cells;
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){
      bool inside=x>=4&&x<=14&&y>=4&&y<=12;
      e.doc.paint(0,0,x,y,inside?'.':'#');if(inside)cells.push_back(y*24+x);
    }
    auto mi=e.doc.records("MAP",0).front();auto f=NativeDocument::fields(e.doc.lines[mi]);
    f[5]="4.5";f[6]="4.5";e.doc.replace(mi,f);return cells;
  };
  auto exists=[&](const std::string& line){return std::find(e.doc.lines.begin(),e.doc.lines.end(),line)!=e.doc.lines.end();};
  auto cells=makeRoom("Medical laboratory");
  check(e.dressingTheme(cells)==3,"A named laboratory is not recognized");
  auto mi=e.doc.records("MAP",0).front();auto fields=NativeDocument::fields(e.doc.lines[mi]);fields[2]="Room";e.doc.replace(mi,fields);
  int desk=retro::model::lab_exec_desk;const auto& dim=retro::model::size[desk];
  e.doc.add({"FIXTURE","0",std::to_string(desk),"8","6","0",nativeNumber(dim[0]),nativeNumber(dim[1]),nativeNumber(dim[2]),"0","1"});
  auto user=e.doc.lines[e.doc.records("FIXTURE",0).back()];
  check(e.dressingTheme(cells)==0,"User desk does not guide the inferred room function");
  auto start=GetTickCount64();e.dressRoomCells(cells);auto elapsed=GetTickCount64()-start;
  check(exists(user),"Dressing altered its authored anchor");
  bool chair=false;
  for(auto i:e.doc.records("FIXTURE",0)){auto f=NativeDocument::fields(e.doc.lines[i]);
    if(int(e.number(f[2],-1))!=retro::model::lab_boardroom_chair)continue;
    float dx=e.number(f[3],0)-8,dy=e.number(f[4],0)-6;
    check(std::abs(dx)<.35f&&dy>0&&dy<3,"Chair does not sit in front of its desk");
    check(std::cos(e.number(f[9],0))<-.9f,"Chair does not face its desk");chair=true;
  }
  check(chair,"Dressing supplied no usable chair for the authored desk");
  // Moving the authored desk should move its generated chair, not leave an orphan.
  auto deskIndex=std::find(e.doc.lines.begin(),e.doc.lines.end(),user)-e.doc.lines.begin();
  e.selectRecord(int(deskIndex));e.moveSelected(1.5f,1.f);
  user=e.doc.lines[size_t(e.record)];chair=false;
  for(auto i:e.doc.records("FIXTURE",0)){auto f=NativeDocument::fields(e.doc.lines[i]);
    if(int(e.number(f[2],-1))==retro::model::lab_boardroom_chair){
      check(std::abs(e.number(f[3],0)-9.5f)<.35f&&e.number(f[4],0)>7,"Chair did not follow its moved user desk");chair=true;}}
  check(chair,"Moving its desk removed the generated chair without replacing it");
  auto stable=e.doc.text();e.dressRoomCells(cells);
  check(e.doc.text()==stable,"Identical redressing reshuffles valid placements");
  e.publish();check(e.doc.text()==stable,"An unchanged committed room is not byte-stable");
  // A wider room must keep the existing composition while adding usable space.
  std::vector<std::string> kept;
  for(int y=4;y<=12;++y)e.doc.paint(0,0,15,y,'.');
  auto widerWalls=e.barriers();
  for(auto i:e.doc.records("FIXTURE",0)){
    auto f=NativeDocument::fields(e.doc.lines[i]);auto box=e.creationBox(f);
    if(!e.doc.automatic(i,0)||(box&&e.dressingHasBacking(int(e.number(f[2],-1)),*box,widerWalls)))kept.push_back(e.doc.lines[i]);
  }
  e.publish();
  for(const auto& line:kept)check(exists(line),"Expanding a room unnecessarily moved existing furniture");
  auto expanded=e.doc.text();e.publish();check(e.doc.text()==expanded,"Expanded room keeps changing after its first update");
  // Collide with a non-relational generated piece. Only that piece should move.
  size_t victim=0;std::string old;kept.clear();
  for(auto i:e.doc.records("FIXTURE",0))if(e.doc.automatic(i,0)){
    auto f=NativeDocument::fields(e.doc.lines[i]);int model=int(e.number(f[2],-1));
    if(!victim&&!Editor::dressingSeat(model)&&!Editor::dressingAnchor(model)){victim=i;old=e.doc.lines[i];}
    else kept.push_back(e.doc.lines[i]);
  }
  check(victim!=0,"No independent generated piece available for local-repair test");
  fields=NativeDocument::fields(old);fields[2]="6"; // a user obstacle with this exact footprint
  e.remember();auto undo=e.doc.text();e.doc.add(fields);
  auto placed=e.doc.lines[e.doc.records("FIXTURE",0).back()];
  e.record=int(e.doc.records("FIXTURE",0).back());e.publish();
  check(exists(placed)&&!exists(old),"User placement did not displace conflicting auto furniture");
  check(e.record>=0&&e.doc.lines[size_t(e.record)]==placed,"Local repair lost the selected user object");
  for(const auto& line:kept)check(exists(line),"A local conflict moved unrelated furniture");
  auto repaired=e.doc.text();e.publish();check(e.doc.text()==repaired,"Local repair does not converge");
  e.doc.loadText(undo);e.publish(true,false);check(e.doc.text()==undo,"Undo reran dressing instead of restoring exact content");
  e.doc.loadText(repaired);e.record=-1;
  // Deliberately deleting a generated item must not immediately respawn it.
  victim=0;for(auto i:e.doc.records("FIXTURE",0))if(e.doc.automatic(i,0)){victim=i;break;}
  check(victim!=0,"Missing auto item for deletion check");eraseNativeObject(e.doc,victim);
  auto count=e.doc.records("FIXTURE",0).size();e.publish();
  check(e.doc.records("FIXTURE",0).size()==count,"Dressing respawned a deliberately deleted item");
  auto saved=e.doc.text();NativeDocument reopened;reopened.loadText(saved);e.doc=std::move(reopened);e.publish();
  check(e.doc.text()==saved,"Reload changed the managed arrangement");
  e.autoDressingEnabled=false;
  for(auto i:e.doc.records("FIXTURE",0))if(e.doc.automatic(i,0)){
    fields=NativeDocument::fields(e.doc.lines[i]);fields[2]="6";e.doc.add(fields);break;
  }
  auto disabled=e.doc.text();e.publish();check(e.doc.text()==disabled,"Disabled dressing still changes an existing room");
  // Six authored scenes exercise every furniture family through real engine validation.
  auto evidence=e.root/L".build"/L"creation-regression";fs::create_directories(evidence);
  std::ofstream shots(evidence/L"adaptive-shots.txt");
  for(int theme=0;theme<6;++theme){
    cells=makeRoom("Room");e.dressRoomCells(cells,theme);e.addCreationLights(cells);
    auto records=e.doc.records("FIXTURE",0);check(!records.empty(),"A spacious themed room contains no furniture");
    for(size_t a=0;a<records.size();++a){auto box=e.creationBox(NativeDocument::fields(e.doc.lines[records[a]]));
      check(bool(box),"Dressed model has no native footprint");
      check(e.creationFits(*box,&cells,int(records[a])),"Themed dressing blocks a room, door or another object");
      for(size_t b=a+1;b<records.size();++b)check(!creation::overlaps(*box,*e.creationBox(NativeDocument::fields(e.doc.lines[records[b]]))),"Themed arrangement overlaps");
    }
    auto stableTheme=e.doc.text();e.publish();
    if(e.doc.text()!=stableTheme){std::ofstream(evidence/L"unstable-before.rmc")<<stableTheme;e.doc.save(evidence/L"unstable-after.rmc");
      throw std::runtime_error("Themed layout changes after commit: theme "+std::to_string(theme));}
    check(e.validateDocument(),"Engine rejected a generated themed room");
    auto file=evidence/(L"adaptive-room-"+std::to_wstring(theme)+L".rmc");e.doc.save(file);
    auto image=evidence/(L"adaptive-room-"+std::to_wstring(theme)+L".ppm");
    shots<<file.string()<<'|'<<image.string()<<"|4.7|4.7|3.4|42|-23|576\n";
  }
  cells=makeRoom("Old user map");e.doc.add({"FIXTURE","0","6","8","8","0","1.87","0.6","0.99","0","1"});
  auto legacy=e.doc.text();e.publish();check(e.doc.text()==legacy,"An unmarked older map was automatically taken over");
  std::cout<<"PASS: context from room name and authored anchors; facing desk/chair pair; byte-stable repeats; stable room expansion; local conflict repair; selection preservation; exact undo; intentional deletion; save/reopen; disabled/legacy isolation ("<<elapsed<<" ms initial solve)\n";
}