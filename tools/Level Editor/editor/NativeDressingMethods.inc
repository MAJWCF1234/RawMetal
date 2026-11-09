  struct DressingTheme { int floor; std::vector<int> pieces; };
  static const std::vector<DressingTheme>& dressingThemes() {
    namespace m=retro::model;
    static const std::vector<DressingTheme> themes{
      {24,{m::lab_exec_desk,m::lab_boardroom_chair,m::office_bookcase,m::lab_potted_plant,m::office_file_cabinet_open}},
      {51,{m::office_couch_green,m::office_armchair_green,m::office_display_cabinet,m::lab_potted_plant,m::office_pc_tower}},
      {50,{m::psx_locker,m::ind_crate,m::prop_cardboard_box,m::ind_barrel,m::lab_specimen_crate}},
      {27,{m::lab_clean_bench,m::lab_sample_fridge,m::lab_photocopier,m::med_supply_cabinet,m::lab_potted_plant}},
      {46,{m::pwr_locker_bank,m::ind_crate,m::ind_barrel,m::prop_cardboard_box,m::med_supply_cabinet}},
      {15,{m::pwr_diesel_genset,m::pwr_control_cabinet,m::pwr_air_handler,m::ind_barrel,m::ind_crate}}};
    return themes;
  }
  static bool dressingSeat(int model) {
    return model==retro::model::lab_boardroom_chair||model==retro::model::office_armchair_green;
  }
  static bool dressingAnchor(int model) {
    return model==retro::model::lab_exec_desk||model==retro::model::office_couch_green;
  }
  int dressingTheme(const std::vector<int>& cells,int chosen=-1) const {
    if(chosen>=0)return chosen%int(dressingThemes().size());
    float votes[6]{};std::set<int> userModels,autoModels;
    for(auto i:doc.records("FIXTURE",map)){
      auto f=NativeDocument::fields(doc.lines[i]);
      if(f.size()>8&&onCreationFloor(number(f[5],-99)+bottomLayerElevation())&&cellIn(cells,number(f[3],-99),number(f[4],-99)))
        (doc.automatic(i,layer)?autoModels:userModels).insert(int(number(f[2],-1)));
    }
    // User anchors outweigh generated pieces; repeated barrels do not pile up votes.
    for(int t=0;t<6;++t)for(int model:dressingThemes()[size_t(t)].pieces){
      int shared=0;for(const auto& theme:dressingThemes())shared+=std::find(theme.pieces.begin(),theme.pieces.end(),model)!=theme.pieces.end();
      votes[t]+=(userModels.count(model)?32.f:0.f)/std::max(1,shared);
      votes[t]+=(autoModels.count(model)?1.f:0.f)/std::max(1,shared);
    }
    std::string title;
    for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2)title=decoded(f[2]);}
    std::transform(title.begin(),title.end(),title.begin(),[](unsigned char c){return char(std::tolower(c));});
    const std::vector<std::vector<std::string>> words{
      {"office","admin","study"},{"lounge","waiting","reception","break room"},{"storage","warehouse","stock"},
      {"lab","clinic","medical","hospital"},{"locker","changing","workshop"},{"power","generator","utility","plant room"}};
    for(int t=0;t<6;++t)for(const auto& word:words[size_t(t)])if(title.find(word)!=std::string::npos)votes[t]+=8;
    int best=int(std::max_element(votes,votes+6)-votes);
    return votes[best]>0?best:cells.size()<=24?0:cells.size()<=64?3:2;
  }
  std::optional<creation::Box> dressingWallBox(int model,float x,float y,int side) const {
    if(model<0||model>=int(std::size(retro::model::size)))return {};
    const auto& size=retro::model::size[model];if(size[0]<=0||size[1]<=0||size[2]<=0)return {};
    creation::Box box{x,y,size[0],size[1],layerElevation(),layerElevation()+size[2],creation::wallYaw(model,side)};
    if(side==0)box.y+=box.halfY()+.08f;if(side==1)box.y-=box.halfY()+.08f;
    if(side==2)box.x+=box.halfX()+.08f;if(side==3)box.x-=box.halfX()+.08f;
    return box;
  }
  void addDressingPiece(int model,const creation::Box& box) {
    doc.addAutomatic({"FIXTURE",std::to_string(map),std::to_string(model),nativeNumber(box.x),nativeNumber(box.y),
      nativeNumber(box.bottom-bottomLayerElevation()),nativeNumber(box.w),nativeNumber(box.d),nativeNumber(box.top-box.bottom),nativeNumber(box.yaw),"1"},layer);
  }
  bool wallPiece(int model,float x,float y,int side,const std::vector<int>* cells=nullptr) {
    auto box=dressingWallBox(model,x,y,side);if(!box||!creationFits(*box,cells))return false;
    addDressingPiece(model,*box);return true;
  }
  static std::string dressingMask(std::vector<int> cells) {
    std::sort(cells.begin(),cells.end());cells.erase(std::unique(cells.begin(),cells.end()),cells.end());
    std::string out;for(int c:cells)if(c>=0&&c<576)out+=(out.empty()?"":",")+std::to_string(c);return out;
  }
  static std::vector<int> dressingCells(const std::string& text) {
    std::vector<int> cells;std::istringstream in(text);std::string part;
    while(std::getline(in,part,',')){char* end=nullptr;long c=strtol(part.c_str(),&end,10);
      if(end!=part.c_str()&&*end==0&&c>=0&&c<576)cells.push_back(int(c));}return cells;
  }
  void rememberDressing(const std::vector<int>& cells,int chosen) {
    std::vector<std::string> marker{"# EDITOR_DRESS",std::to_string(map),std::to_string(layer),std::to_string(chosen),dressingMask(cells)};
    for(auto i:doc.records("# EDITOR_DRESS",map)){
      auto f=NativeDocument::fields(doc.lines[i]);if(f.size()!=5||f[2]!=marker[2])continue;
      auto previous=dressingCells(f[4]);bool overlaps=false;
      for(int c:cells)if(std::find(previous.begin(),previous.end(),c)!=previous.end()){overlaps=true;break;}
      if(overlaps){if(f!=marker)doc.replace(i,marker,true);return;}
    }doc.add(marker);
  }
  bool dressingSeatHasAnchor(int model,const creation::Box& seat,const std::vector<int>& cells) const {
    for(auto i:doc.records("FIXTURE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<11)continue;
      int other=int(number(f[2],-1));if(!dressingAnchor(other))continue;
      if((model==retro::model::lab_boardroom_chair)!=(other==retro::model::lab_exec_desk))continue;
      auto a=creationBox(f);if(!a||!onCreationFloor(a->bottom)||!cellIn(cells,a->x,a->y))continue;
      float sign=other<63?-1.f:1.f,fx=std::sin(a->yaw)*sign,fy=std::cos(a->yaw)*sign;
      float dx=seat.x-a->x,dy=seat.y-a->y,along=dx*fx+dy*fy,across=std::abs(dx*fy-dy*fx);
      if(along>0&&along<a->d*.5f+seat.d*.5f+1.f&&across<.35f&&std::cos(seat.yaw-a->yaw)<-.9f)return true;
    }return false;
  }
  bool dressingHasBacking(int model,const creation::Box& box,const std::vector<char>& walls) const {
    if(dressingSeat(model))return true;
    float sign=model<63?-1.f:1.f,fx=std::sin(box.yaw)*sign,fy=std::cos(box.yaw)*sign;
    int x=int(std::floor(box.x-fx*(box.d*.5f+.18f))),y=int(std::floor(box.y-fy*(box.d*.5f+.18f)));
    return x>=0&&x<24&&y>=0&&y<24&&walls[size_t(y*24+x)]==1;
  }
  int dressRoomCells(const std::vector<int>& cells,int chosen=-1,bool track=true,int budget=-1) {
    if(cells.empty())return 0;
    const auto& theme=dressingThemes()[size_t(dressingTheme(cells,chosen))];
    if(track)rememberDressing(cells,chosen);
    bool changeFloor=false;
    for(auto i:doc.records("STRUCT",map))if(track&&doc.automatic(i,layer)){
      auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>9&&onCreationFloor(number(f[6],-99))&&
        cellIn(cells,(number(f[2],0)+number(f[4],0))*.5f,(number(f[3],0)+number(f[5],0))*.5f)&&int(number(f[9],-1))!=theme.floor)changeFloor=true;
    }
    if(changeFloor)clearAutomaticRoom(cells,"STRUCT");
    int floorMaterial=theme.floor;
    if(!track)for(auto i:doc.records("STRUCT",map))if(doc.automatic(i,layer)){
      auto f=NativeDocument::fields(doc.lines[i]);
      if(f.size()>9&&std::abs(number(f[6],-99)-layerElevation())<.03f&&number(f[7],99)<=layerElevation()+.05f&&
         cellIn(cells,(number(f[2],0)+number(f[4],0))*.5f,(number(f[3],0)+number(f[5],0))*.5f)){floorMaterial=int(number(f[9],floorMaterial));break;}
    }
    addCreationFloor(cells,floorMaterial);
    // Keep valid items verbatim, only repairing conflicts or incompatible pieces.
    auto walls=barriers();
    // Repair anchors before their dependent seats, so one commit converges.
    for(int pass=0;pass<2;++pass){auto fixtures=doc.records("FIXTURE",map);
    for(auto it=fixtures.rbegin();it!=fixtures.rend();++it)if(doc.automatic(*it,layer)){
      auto f=NativeDocument::fields(doc.lines[*it]);auto box=creationBox(f);
      if(!box||!onCreationFloor(box->bottom)||!cellIn(cells,box->x,box->y))continue;
      int model=int(number(f[2],-1));if(dressingSeat(model)!=(pass==1))continue;
      bool compatible=std::find(theme.pieces.begin(),theme.pieces.end(),model)!=theme.pieces.end();
      if(!compatible||!creationFits(*box,&cells,int(*it))||!dressingHasBacking(model,*box,walls)||
         (dressingSeat(model)&&!dressingSeatHasAnchor(model,*box,cells))){
        try{eraseNativeObject(doc,*it);}catch(const std::exception&){doc.claimAutomatic(*it);}
      }
    }
    }
    struct Seat{float x,y;int side;};std::vector<Seat> seats;
    for(int c:cells){if(c<0||c>=576)continue;int x=c%24,y=c/24;
      for(float slide:{.25f,.5f,.75f}){
        if(y>0&&walls[size_t(c-24)]==1)seats.push_back({x+slide,float(y),0});
        if(y<23&&walls[size_t(c+24)]==1)seats.push_back({x+slide,float(y+1),1});
        if(x>0&&walls[size_t(c-1)]==1)seats.push_back({float(x),y+slide,2});
        if(x<23&&walls[size_t(c+1)]==1)seats.push_back({float(x+1),y+slide,3});
      }
    }
    int target=std::clamp(int(cells.size())/10+1,1,int(theme.pieces.size()));
    if(budget>=0)target=std::min(target,budget);
    int existing=0;std::map<int,int> counts;
    for(auto i:doc.records("FIXTURE",map)){auto f=NativeDocument::fields(doc.lines[i]);
      if(f.size()>8&&onCreationFloor(number(f[5],-99)+bottomLayerElevation())&&cellIn(cells,number(f[3],-99),number(f[4],-99))){++existing;++counts[int(number(f[2],-1))];}}
    int made=0;
    for(;existing<target;++existing){
      struct Candidate{int model=-1;creation::Box box{};float score=-1e9f;} best;
      for(size_t p=0;p<theme.pieces.size();++p){int model=theme.pieces[p];if(counts[model])continue;
        auto consider=[&](const creation::Box& box,float relation){
          float score=30.f-float(p)*2.f+relation;
          score+=float(roomHash(map,layer,model,int(std::round(box.x*4)),int(std::round(box.y*4)))%997)/9970.f;
          if(score<=best.score||!creationFits(box,&cells))return;
          if(!dressingSeat(model)&&model!=retro::model::lab_potted_plant){
            float sign=model<63?-1.f:1.f,fx=std::sin(box.yaw)*sign,fy=std::cos(box.yaw)*sign;
            creation::Box access{box.x+fx*(box.d*.5f+.38f),box.y+fy*(box.d*.5f+.38f),.65f,.6f,
              box.bottom+.05f,std::min(creationCeiling(),box.bottom+1.8f),box.yaw};
            if(model!=retro::model::lab_exec_desk&&!creationFits(access,&cells))return;
          }best={model,box,score};
        };
        if(dressingSeat(model)){
          for(auto i:doc.records("FIXTURE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<11)continue;
            int anchorModel=int(number(f[2],-1));if(!dressingAnchor(anchorModel))continue;
            if((model==retro::model::lab_boardroom_chair)!=(anchorModel==retro::model::lab_exec_desk))continue;
            auto anchor=creationBox(f);if(!anchor||!onCreationFloor(anchor->bottom)||!cellIn(cells,anchor->x,anchor->y))continue;
            float sign=anchorModel<63?-1.f:1.f,fx=std::sin(anchor->yaw)*sign,fy=std::cos(anchor->yaw)*sign;
            const auto& size=retro::model::size[model];
            for(float extra:{.22f,.5f,.8f}){float gap=anchor->d*.5f+size[1]*.5f+extra;
              consider({anchor->x+fx*gap,anchor->y+fy*gap,size[0],size[1],layerElevation(),layerElevation()+size[2],
                std::remainder(anchor->yaw+3.14159265359f,6.28318530718f)},12.f-extra);}
          }
        }else for(const auto& seat:seats)if(auto box=dressingWallBox(model,seat.x,seat.y,seat.side)){
          float relation=0;
          if(model==retro::model::lab_potted_plant){
            int x=cellOf(box->x),y=cellOf(box->y);
            for(auto [dx,dy]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){int xx=x+dx,yy=y+dy;
              if(xx>=0&&xx<24&&yy>=0&&yy<24&&walls[size_t(yy*24+xx)]==1)relation+=1.5f;}
          }consider(*box,relation);
        }
      }
      if(best.model<0)break; // Leave breathing room rather than force clutter.
      addDressingPiece(best.model,best.box);++counts[best.model];++made;
    }return made;
  }
  void reconcileDressing() {
    if(!autoDressingEnabled)return;
    struct Managed{std::string line;std::vector<std::string> fields;};std::vector<Managed> managed;
    for(auto i:doc.records("# EDITOR_DRESS",map)){
      auto f=NativeDocument::fields(doc.lines[i]);if(f.size()==5&&f[2]==std::to_string(layer))managed.push_back({doc.lines[i],f});
    }
    if(managed.empty())return;
    auto walls=barriers();std::vector<char> seen(576,0);
    for(auto& marker:managed){
      auto old=dressingCells(marker.fields[4]);std::vector<std::vector<int>> rooms;std::vector<int> all;
      for(int c:old)if(!seen[size_t(c)]){
        auto cells=roomAt(c%24,c/24,walls);if(cells.empty())continue;
        for(int r:cells)seen[size_t(r)]=1;
        all.insert(all.end(),cells.begin(),cells.end());rooms.push_back(std::move(cells));
      }
      if(rooms.empty())continue;
      bool changed=dressingMask(all)!=marker.fields[4];
      for(const auto& cells:rooms){int budget=0;
        for(auto i:doc.records("FIXTURE",map)){auto f=NativeDocument::fields(doc.lines[i]);
          if(f.size()>8&&onCreationFloor(number(f[5],-99)+bottomLayerElevation())&&cellIn(cells,number(f[3],-99),number(f[4],-99)))++budget;}
        dressRoomCells(cells,int(number(marker.fields[3],-1)),false,changed?-1:budget);
        if(changed)addCreationLights(cells);
      }
      if(changed){auto found=std::find(doc.lines.begin(),doc.lines.end(),marker.line);
        if(found!=doc.lines.end()){marker.fields[4]=dressingMask(all);doc.replace(size_t(found-doc.lines.begin()),marker.fields,true);}}
    }
  }
  int furnishRoom(int x0,int y0,int x1,int y1) {
    std::vector<int> cells;
    for(int y=std::max(0,y0+1);y<std::min(24,y1);++y)for(int x=std::max(0,x0+1);x<std::min(24,x1);++x)
      if(doc.tile(map,layer,x,y)=='.'||doc.tile(map,layer,x,y)=='=')cells.push_back(y*24+x);
    return dressRoomCells(cells,forcedTheme);
  }