  std::vector<int> lastCorridorAnchors;
  // Floor support, rather than a non-wall character, bounds an upstairs room.
  std::array<bool,576> nativeWalkableCells() const {
    std::array<bool,576> result{};
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){char tile=doc.tile(map,layer,x,y);result[size_t(y*24+x)]=tile=='='||(layer==0&&tile!='#'&&tile!='_');}
    for(auto i:doc.records("STRUCT",map)){auto f=NativeDocument::fields(doc.lines[i]);if(!nativeFloorSlab(f))continue;
      for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(number(f[2],99)<=x+.001f&&number(f[3],99)<=y+.001f&&number(f[4],-1)>=x+.999f&&number(f[5],-1)>=y+.999f)result[size_t(y*24+x)]=true;}
    return result;
  }
  bool passageIntersects(const creation::Box& box) const {
    for(auto i:doc.records("# EDITOR_PASSAGE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<4||int(number(f[2],-1))!=layer)continue;
      for(int c:dressingCells(f[3]))if(creation::overlaps(box,{c%24+.5f,c/24+.5f,1,1,layerElevation(),std::min(creationCeiling(),layerElevation()+1.8f)},.04f))return true;}
    return false;
  }
  bool corridorManaged(size_t index) const {
    if(!doc.automatic(index,layer))return false;auto owner=NativeDocument::fields(doc.lines[index]);if(owner[0]!="FIXTURE"&&owner[0]!="CLUTTER")return true;
    auto all=doc.records(owner[0],map);int ordinal=int(std::find(all.begin(),all.end(),index)-all.begin());
    for(auto i:doc.onMap(map)){if(doc.automatic(i,layer))continue;auto f=NativeDocument::fields(doc.lines[i]);int field=f[0]=="LEAD"?(owner[0]=="FIXTURE"?18:13):f[0]=="BREAKABLE"&&owner[0]=="FIXTURE"&&f.size()==15?13:-1;
      if(field>=0&&size_t(field)<f.size()&&!f[size_t(field)].empty()&&int(number(f[size_t(field)],-1))==ordinal)return false;}return true;
  }
  // A corridor opens wall panels, not arbitrary user architecture or furniture.
  bool corridorWall(const std::vector<std::string>& f) const {
    if(f.size()<10||f[0]!="STRUCT"||nativeFloorSlab(f))return false;
    float w=number(f[4],0)-number(f[2],0),d=number(f[5],0)-number(f[3],0),b=number(f[6],0),t=number(f[7],0);
    return t>layerElevation()+.2f&&b<std::min(creationCeiling(),layerElevation()+1.75f)&&t-std::max(b,layerElevation())>=1.6f&&((w>=1.8f&&d<=1.05f)||(d>=1.8f&&w<=1.05f));
  }
  void corridor(float ax,float ay,float bx,float by) {
    const float floor=layerElevation(),top=creationCeiling();
    if(effectiveFloorView(floorLayer(layer)).locked){message(L"Unlock this floor before building a corridor.");return;}
    if(top-floor<1.75f){message(L"Raise the floor above or the roof first: a corridor needs standing headroom.");return;}
    auto walkable=nativeWalkableCells();auto walls=barriers();
    int start=cellOf(ay)*24+cellOf(ax),goal=cellOf(by)*24+cellOf(bx);
    if(!walkable[size_t(start)]||!walkable[size_t(goal)]||walls[size_t(start)]||walls[size_t(goal)]){message(L"Start and finish on clear floor inside the rooms you want to connect.");return;}
    std::array<int,576> rooms{};rooms.fill(-1);int nextRoom=0;std::set<int> background;std::map<int,int> innerContacts;
    for(int c=0;c<576;++c)if(walkable[size_t(c)]&&!walls[size_t(c)]&&rooms[size_t(c)]<0){std::vector<int> pending{c};rooms[size_t(c)]=nextRoom;
      while(!pending.empty()){int at=pending.back();pending.pop_back();int x=at%24,y=at/24;if(x==0||x==23||y==0||y==23)background.insert(nextRoom);if(x==1||x==22||y==1||y==22)++innerContacts[nextRoom];
        for(auto [dx,dy]:{std::pair{1,0},{0,1},{-1,0},{0,-1}}){int X=x+dx,Y=y+dy;if(X<0||Y<0||X>=24||Y>=24)continue;int n=Y*24+X;if(walkable[size_t(n)]&&!walls[size_t(n)]&&rooms[size_t(n)]<0){rooms[size_t(n)]=nextRoom;pending.push_back(n);}}}
      ++nextRoom;}
    int roomA=rooms[size_t(start)],roomB=rooms[size_t(goal)];
    if(background.empty()){int candidate=-1,contacts=0;for(auto [id,count]:innerContacts)if(id!=roomA&&id!=roomB&&count>contacts){candidate=id;contacts=count;}if(candidate>=0)background.insert(candidate);}
    if(roomA==roomB){message(L"Those rooms already share an open passage. No changes made.");return;}
    ::corridor::Grid grid{};
    // An enclosed void is an authored opening (for example a ramp gap). The
    // open background between room islands is available for a new passage.
    // Older maps have a solid outer ROW border: contact with its inner edge
    // still identifies exterior background, rather than an authored ramp hole.
    std::array<bool,576> gaps{},seen{};
    for(int c=0;c<576;++c)if(!walkable[size_t(c)]&&!walls[size_t(c)]&&!seen[size_t(c)]){std::vector<int> pending{c},component;seen[size_t(c)]=true;bool edge=false;
        while(!pending.empty()){int at=pending.back();pending.pop_back();component.push_back(at);int x=at%24,y=at/24;edge|=x<=1||y<=1||x>=22||y>=22;
        for(auto [dx,dy]:{std::pair{1,0},{0,1},{-1,0},{0,-1}}){int X=x+dx,Y=y+dy;if(X<0||Y<0||X>=24||Y>=24)continue;int n=Y*24+X;if(!walkable[size_t(n)]&&!walls[size_t(n)]&&!seen[size_t(n)]){seen[size_t(n)]=true;pending.push_back(n);}}}
      if(!edge)for(int at:component)gaps[size_t(at)]=true;}
    for(int c=0;c<576;++c){auto& cell=grid[size_t(c)];int r=rooms[size_t(c)];cell.room=r;
      bool other=r>=0&&r!=roomA&&r!=roomB&&!background.count(r);
      cell.blocked=gaps[size_t(c)]||other;cell.wallProtected=cell.blocked;
      cell.buildWalls=r!=roomA&&r!=roomB&&!walls[size_t(c)];cell.cost=walls[size_t(c)]?90:walkable[size_t(c)]?10:18;
      char tile=doc.tile(map,layer,c%24,c/24);if(tile=='C'||tile=='B'||tile=='T'){cell.blocked=cell.wallProtected=true;}}
    // Do not tunnel through an uninvolved room's wall just because its interior
    // is protected. Existing source/destination room walls remain carveable.
    for(int c=0;c<576;++c)if(walls[size_t(c)])for(auto [dx,dy]:{std::pair{1,0},{0,1},{-1,0},{0,-1}}){int x=c%24+dx,y=c/24+dy;if(x<0||y<0||x>=24||y>=24)continue;int r=rooms[size_t(y*24+x)];if(r>=0&&r!=roomA&&r!=roomB&&!background.count(r))grid[size_t(c)].blocked=true;}
    std::vector<size_t> wallRecords;
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(corridorWall(f)){wallRecords.push_back(i);continue;}if(f.empty()||nativeFloorSlab(f))continue;
      auto box=creationBox(f);if(!box)continue;
      bool managed=corridorManaged(i);
      for(int c=0;c<576;++c){if(creation::overlaps(*box,{c%24+.5f,c/24+.5f,1,1,floor+.04f,std::min(top,floor+1.75f)},.03f)){
        if(managed)grid[size_t(c)].cost+=180;else grid[size_t(c)].blocked=true;}
        if(!managed&&creation::overlaps(*box,{c%24+.5f,c/24+.5f,1,1,floor,top},.03f))grid[size_t(c)].wallProtected=true;}}
    auto route=::corridor::plan(grid,start,goal);
    if(route.cells.empty()){message(L"No clear two-metre route fits here. Move the blocking item or choose a clearer part of each room.");return;}
    std::set<int> path(route.cells.begin(),route.cells.end()),sideWalls;
    for(int c:route.cells)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int x=c%24+dx,y=c/24+dy;if(x<0||y<0||x>=24||y>=24)continue;int n=y*24+x;if(!path.count(n)&&grid[size_t(n)].buildWalls&&!grid[size_t(n)].wallProtected)sideWalls.insert(n);}
    const auto before=doc.text();const auto previousFuture=future;auto oldHistory=history.size();auto oldDirty=dirty;auto oldChanges=changeCount;int oldRecord=record;auto oldSelection=selectedRecords;remember();
    try{
      std::vector<creation::Box> cuts;for(int c:route.cells)cuts.push_back({c%24+.5f,c/24+.5f,1,1,floor,top});
      // A native ROW wall may begin on a lower floor and span several storeys.
      // Replace only its active-height opening, retaining the rest as native
      // wall structures with the same map wall material and UV density.
      auto floors=floorLayers();
      for(const auto& a:floors)if(a.elevation<=floor+.01f)for(int c:route.cells)if(doc.tile(map,a.id,c%24,c/24)=='#'){
        doc.paint(map,a.id,c%24,c/24,a.id==0?'.':'_');
        auto fragment=[&](float b,float t,int owner){if(t-b>.001f)doc.addOnLayer({"STRUCT",std::to_string(map),std::to_string(c%24),std::to_string(c/24),std::to_string(c%24+1),std::to_string(c/24+1),nativeNumber(b),nativeNumber(t),"0","3"},owner);};
        fragment(a.elevation,layer>0?floor-std::max(.02f,floorLayer(layer).thickness):floor,a.id);fragment(top,ceilingHeight(),floorAtHeight(top+.05f));}
      for(auto it=wallRecords.rbegin();it!=wallRecords.rend();++it)cutCreationStructure(*it,cuts);
      std::vector<size_t> generated;
      for(auto i:doc.onMap(map))if(corridorManaged(i)){auto f=NativeDocument::fields(doc.lines[i]);if(f[0]=="STRUCT"||f[0]=="DOOR")continue;auto box=creationBox(f);if(!box)continue;
        for(int c:route.cells)if(creation::overlaps(*box,{c%24+.5f,c/24+.5f,1,1,floor,std::min(top,floor+1.8f)})){generated.push_back(i);break;}
        if(std::find(generated.begin(),generated.end(),i)==generated.end())for(int c:sideWalls)if(creation::overlaps(*box,{c%24+.5f,c/24+.5f,1,1,floor,top})){generated.push_back(i);break;}}
      eraseNativeSet(generated);
      // Existing concrete, stripes and all other floor patches remain intact.
      // Only newly opened cells need an authored native floor.
      for(int c:route.cells)if(!walkable[size_t(c)]){
        int material=floorPaintMaterial;float best=1e9f;
        for(auto i:doc.records("STRUCT",map)){auto f=NativeDocument::fields(doc.lines[i]);if(!nativeFloorSlab(f))continue;float d=std::hypot((number(f[2],0)+number(f[4],0))*.5f-(c%24+.5f),(number(f[3],0)+number(f[5],0))*.5f-(c/24+.5f));if(d<best){best=d;material=int(number(f[9],material));}}
        doc.paint(map,layer,c%24,c/24,layer>0?'=':'.');paintNativeFloor(float(c%24),float(c/24),float(c%24+1),float(c/24+1),material);}
      // Side walls are bounded native solids, so they cannot grow through
      // another floor the way a full-height ROW wall would.
      auto remaining=sideWalls;while(!remaining.empty()){int first=*remaining.begin(),x0=first%24,y0=first/24,x1=x0+1,y1=y0+1;while(x1<24&&remaining.count(y0*24+x1))++x1;
        for(;y1<24;++y1){bool full=true;for(int x=x0;x<x1;++x)if(!remaining.count(y1*24+x)){full=false;break;}if(!full)break;}
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)remaining.erase(y*24+x);
        doc.addOnLayer({"STRUCT",std::to_string(map),std::to_string(x0),std::to_string(y0),std::to_string(x1),std::to_string(y1),nativeNumber(floor),nativeNumber(top),"0","3"},layer);}

      std::set<int> reserved(path);for(auto i:doc.records("# EDITOR_PASSAGE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>3&&int(number(f[2],-1))==layer)for(int c:dressingCells(f[3]))reserved.insert(c);}
      storeLayerPassageMask(layer,std::vector<int>(reserved.begin(),reserved.end()));
      std::vector<int> lampCells;float lampZ=lampHeight();for(int c:route.cells){bool clear=true;for(auto i:doc.records("STRUCT",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7&&c%24+.5f>=number(f[2],0)&&c%24+.5f<number(f[4],0)&&c/24+.5f>=number(f[3],0)&&c/24+.5f<number(f[5],0)&&lampZ>=number(f[6],0)-.1f&&lampZ<=number(f[7],0)+.1f){clear=false;break;}}if(clear)lampCells.push_back(c);}
      addCreationLights(lampCells,"3000");lastCorridorAnchors=route.anchors;
      if(doc.text()==before){history.resize(oldHistory);future=previousFuture;message(L"The passage is already complete. No changes made.");return;}
      selectedRecords.clear();record=-1;publish(true,false);refreshRecords();
      message(L"Corridor placed with clear width, native floors and automatic lighting. Ctrl+Z restores the whole edit.");
    }catch(...){doc.loadText(before);history.resize(oldHistory);future=previousFuture;dirty=oldDirty;changeCount=oldChanges;record=oldRecord;selectedRecords=std::move(oldSelection);publish(false,false);throw;}
  }
