  float creationCeiling() const {
    float floor=layerElevation(), top=ceilingHeight();
    for(auto i:doc.records("LAYER",map)){
      auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<5)continue;
      float z=number(f[4],floor), thickness=f.size()>5?number(f[5],0):0;
      if(z>floor+.1f)top=std::min(top,z-std::max(0.f,thickness));
    }
    return top;
  }
  bool onCreationFloor(float z) const {
    return z>=layerElevation()-.03f&&z<creationCeiling()-.001f;
  }
  std::optional<creation::Box> creationBox(const std::vector<std::string>& f) const {
    if(f.size()<3)return {};
    auto n=[&](size_t i,float fallback=0.f){return i<f.size()?number(f[i],fallback):fallback;};
    if(f[0]=="FIXTURE"&&f.size()>=11)return creation::Box{n(3),n(4),n(6),n(7),n(5)+bottomLayerElevation(),n(5)+bottomLayerElevation()+n(8),n(9)};
    if(f[0]=="CLUTTER"&&f.size()>=14&&f[2]=="7")return creation::Box{n(3),n(4),n(8),n(9),n(5),n(5)+n(10),n(6)};
    if(f[0]=="STRUCT"&&f.size()>=10){if(n(7)-n(6)<=.05f)return {};
      return creation::Box{(n(2)+n(4))*.5f,(n(3)+n(5))*.5f,n(4)-n(2),n(5)-n(3),n(6),n(7)};}
    if(f[0]=="DOOR"&&f.size()>=6)return creation::Box{(n(2)+n(3))*.5f,n(4),n(3)-n(2),.3f,n(5)+bottomLayerElevation(),n(5)+bottomLayerElevation()+2.2f};
    static const std::set<std::string> physical={"PRIM","STAIR","WATER","COMPACTOR","CREATURE","CLUTTER","ACTOR_KEY","ACTOR_IDLE_KEY","TERMINAL","PICKUP","SIGN","LIGHT","PIPE","LEAD","BREAKABLE"};
    if(!physical.count(f[0]))return {};
    int schema=schemaFor(f);if(schema<0)return {};
    const auto& names=schemas[size_t(schema)].fields;auto handle=nativeHandle(names,f);if(!handle)return {};
    auto named=[&](const char* name,float fallback){auto at=std::find(names.begin(),names.end(),name);
      return at==names.end()?fallback:n(size_t(at-names.begin()),fallback);};
    float floor=layerElevation(), bottom=named("bottom",named("base",named("z",named("bed",floor))));
    if(bottom<-900)bottom=floor;
    float height=named("height",named("h",f[0]=="CREATURE"||f[0].starts_with("ACTOR_")?1.8f:.35f));
    float top=named("top",named("surface",bottom+height));
    if(f[0]=="PIPE"){float end=named("endZ",bottom);top=std::max(bottom,end)+.02f;bottom=std::min(bottom,end);}
    if(f[0]=="LEAD"){float a=named("az",floor),b=named("bz",floor);bottom=std::min(a,b);top=std::max(a,b)+.02f;}
    float yaw=named("yaw",named("facing",0)*1.57079632679f);
    return creation::Box{handle->x,handle->y,handle->width,handle->depth,bottom,std::max(bottom+.02f,top),yaw};
  }
  std::vector<creation::Box> creationObstacles(int ignored=-1) const {
    std::vector<creation::Box> out;
    for(auto i:doc.onMap(map))if(int(i)!=ignored){auto f=NativeDocument::fields(doc.lines[i]);
      if(auto box=creationBox(f))out.push_back(*box);}
    return out;
  }
  bool creationFits(const creation::Box& box,const std::vector<int>* room=nullptr,int ignored=-1) const {
    const float floor=layerElevation(), top=creationCeiling();
    if(box.w<=0||box.d<=0||box.bottom<floor-.03f||box.top>top-.03f)return false;
    float left=box.x-box.halfX(), right=box.x+box.halfX(), north=box.y-box.halfY(), south=box.y+box.halfY();
    if(left<0||right>24||north<0||south>24)return false;
    for(int y=int(std::floor(north));y<int(std::ceil(south));++y)
      for(int x=int(std::floor(left));x<int(std::ceil(right));++x){
        creation::Box tile{float(x)+.5f,float(y)+.5f,1,1,floor,top};if(!creation::overlaps(box,tile))continue;
        char ch=doc.tile(map,layer,x,y);
        if((ch!='.'&&ch!='=')||(room&&std::find(room->begin(),room->end(),y*24+x)==room->end()))return false;
      }
    auto occupied=creationObstacles(ignored);
    for(const auto& other:occupied)if(creation::overlaps(box,other,.08f))return false;
    // Doors need standing room on both sides, including their full opening.
    for(auto i:doc.records("DOOR",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<6||!onCreationFloor(number(f[5],0)+bottomLayerElevation()))continue;
      creation::Box approach{(number(f[2],0)+number(f[3],0))*.5f,number(f[4],0),number(f[3],0)-number(f[2],0)+.5f,2.4f,floor,top};
      if(creation::overlaps(box,approach))return false;
    }
    for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7&&onCreationFloor(number(f[7],0))){
      creation::Box spawn{number(f[5],0),number(f[6],0),1.2f,1.2f,floor,top};if(creation::overlaps(box,spawn))return false;}}
    if(room){
      std::vector<char> before(576,0),after(576,0);
      for(int c:*room){int x=c%24,y=c/24;if(c<0||c>=576)continue;
        creation::Box person{float(x)+.5f,float(y)+.5f,.56f,.56f,floor+.05f,std::min(top,floor+1.8f)};
        bool clear=true;for(const auto& other:occupied)if(creation::overlaps(person,other)){clear=false;break;}
        before[size_t(c)]=clear;after[size_t(c)]=clear&&!creation::overlaps(person,box);
      }
      auto components=[](std::vector<char> cells){int count=0;for(int c=0;c<576;++c)if(cells[size_t(c)]){
        ++count;std::vector<int> stack{c};cells[size_t(c)]=0;
        while(!stack.empty()){int at=stack.back();stack.pop_back();for(auto [dx,dy]:{std::pair{1,0},{-1,0},{0,1},{0,-1}}){
          int x=at%24+dx,y=at/24+dy;if(x<0||x>=24||y<0||y>=24)continue;int next=y*24+x;
          if(cells[size_t(next)]){cells[size_t(next)]=0;stack.push_back(next);}}}}
        return count;};
      if(components(after)==0||components(after)>components(before))return false;
    }
    return true;
  }
  void addCreationFloor(const std::vector<int>& cells,int material) {
    const float floor=layerElevation();std::vector<char> wanted(576,0);
    auto supported=nativeWalkableCells();for(int c:cells)if(c>=0&&c<576&&supported[size_t(c)])wanted[size_t(c)]=1;
    std::vector<creation::Box> existing;
    for(auto i:doc.records("STRUCT",map)){auto f=NativeDocument::fields(doc.lines[i]);if(!nativeFloorSlab(f))continue;
      // Subtract the exact patch footprint. A narrow painted stripe does not
      // cover the rest of its metre-wide cell, and must remain unchanged.
      existing.push_back({(number(f[2],0)+number(f[4],0))*.5f,(number(f[3],0)+number(f[5],0))*.5f,
        number(f[4],0)-number(f[2],0),number(f[5],0)-number(f[3],0),floor-.6f,floor+.08f});}
    for(int y=0;y<24;++y)for(int x=0;x<24;){if(!wanted[size_t(y*24+x)]){++x;continue;}
      int start=x;while(x<24&&wanted[size_t(y*24+x)])++x;
      std::vector<creation::Box> pieces{{(start+x)*.5f,y+.5f,float(x-start),1,floor,floor+.02f}};
      for(const auto& patch:existing){std::vector<creation::Box> next;for(const auto& piece:pieces){auto remains=creation::subtract(piece,patch);next.insert(next.end(),remains.begin(),remains.end());}pieces=std::move(next);if(pieces.empty())break;}
      for(const auto& piece:pieces)doc.addAutomatic({"STRUCT",std::to_string(map),nativeNumber(piece.x-piece.w*.5f),nativeNumber(piece.y-piece.d*.5f),
        nativeNumber(piece.x+piece.w*.5f),nativeNumber(piece.y+piece.d*.5f),nativeNumber(piece.bottom),nativeNumber(piece.top),"0",std::to_string(material)},layer);
    }
  }
  int addCreationLights(const std::vector<int>& cells,const char* kelvin="3200"){
    if(cells.empty())return 0;int made=0;auto lamps=lightsIn(cells);
    auto distance=[&](float x,float y){float nearest=1e9f;
      for(auto i:lamps){auto f=NativeDocument::fields(doc.lines[i]);nearest=std::min(nearest,std::hypot(number(f[2],0)-x,number(f[3],0)-y));}return nearest;};
    auto add=[&](int c){doc.addAutomatic({"LIGHT",std::to_string(map),nativeNumber(c%24+.5f),nativeNumber(c/24+.5f),
      nativeNumber(lampHeight()),"0","0","1","8",kelvin},layer);lamps=lightsIn(cells);++made;};
    for(int c:cells)if(c%24%4==2&&c/24%4==2&&distance(c%24+.5f,c/24+.5f)>=2.6f)add(c);
    // Fill a real coverage gap, never add a fallback on an already-lit point.
    for(;;){float gap=2.6f;int best=-1;
      for(int c:cells){float d=distance(c%24+.5f,c/24+.5f);if(d>gap){gap=d;best=c;}}
      if(best<0)break;add(best);
    }
    return made;
  }
  void cutCreationStructure(size_t index,const std::vector<creation::Box>& cuts){
    auto f=NativeDocument::fields(doc.lines[index]);if(f.size()<10)return;
    bool owned=doc.automatic(index);int ownerLayer=doc.recordLayer(index).value_or(layer);
    creation::Box original{(number(f[2],0)+number(f[4],0))*.5f,(number(f[3],0)+number(f[5],0))*.5f,
      number(f[4],0)-number(f[2],0),number(f[5],0)-number(f[3],0),number(f[6],0),number(f[7],0)};
    std::vector<creation::Box> pieces{original};bool changed=false;
    for(const auto& cut:cuts){std::vector<creation::Box> next;for(const auto& piece:pieces){
      if(creation::overlaps(piece,cut))changed=true;
      auto remains=creation::subtract(piece,cut);next.insert(next.end(),remains.begin(),remains.end());}pieces=std::move(next);}
    if(!changed)return;
    doc.eraseRecord(index);
    for(const auto& piece:pieces){auto fields=f;
      fields[2]=nativeNumber(piece.x-piece.w*.5f);fields[3]=nativeNumber(piece.y-piece.d*.5f);
      fields[4]=nativeNumber(piece.x+piece.w*.5f);fields[5]=nativeNumber(piece.y+piece.d*.5f);
      fields[6]=nativeNumber(piece.bottom);fields[7]=nativeNumber(piece.top);
      if(owned)doc.addAutomatic(fields,ownerLayer);else doc.addOnLayer(fields,ownerLayer);
    }
  }
  void clearAutomaticRoom(const std::vector<int>& cells,const std::string& kind){
    if(kind=="STRUCT"){
      std::vector<creation::Box> cuts;for(int c:cells)cuts.push_back({float(c%24)+.5f,float(c/24)+.5f,1,1,layerElevation()-.03f,layerElevation()+.05f});
      auto all=doc.records(kind,map);for(auto it=all.rbegin();it!=all.rend();++it)if(doc.automatic(*it,layer))cutCreationStructure(*it,cuts);
      return;
    }
    auto all=doc.records(kind,map);for(auto it=all.rbegin();it!=all.rend();++it)if(doc.automatic(*it,layer)){
      auto f=NativeDocument::fields(doc.lines[*it]);auto box=creationBox(f);
      if(box&&onCreationFloor(box->bottom)&&cellIn(cells,box->x,box->y)){
        try{eraseNativeObject(doc,*it);}catch(const std::exception&){doc.claimAutomatic(*it);}
      }
    }
  }
  void clearCreationWalls(const std::vector<int>& cells){
    auto previous=doc;try{
    std::vector<creation::Box> cuts;for(int c:cells)cuts.push_back({float(c%24)+.5f,float(c/24)+.5f,1,1,layerElevation(),creationCeiling()});
    auto intersects=[&](const creation::Box& box){for(const auto& cut:cuts)if(creation::overlaps(box,cut))return true;return false;};
    auto structures=doc.records("STRUCT",map);
    for(auto it=structures.rbegin();it!=structures.rend();++it){auto f=NativeDocument::fields(doc.lines[*it]);
      if(auto box=creationBox(f);box&&intersects(*box))cutCreationStructure(*it,cuts);}
    for(;;){int victim=-1;
      for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.empty()||f[0]=="STRUCT")continue;
        if(auto box=creationBox(f);box&&intersects(*box)){victim=int(i);break;}}
      if(victim<0)break;
      auto object=NativeDocument::fields(doc.lines[size_t(victim)]);
      // Remove dependent cable/part records before the attached object; then
      // the native deletion helper can renumber every surviving reference.
      if(object[0]=="FIXTURE"||object[0]=="CLUTTER"){
        auto list=doc.records(object[0],map);int ordinal=int(std::find(list.begin(),list.end(),size_t(victim))-list.begin());
        int dependent=-1;
        for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);int field=-1;
          if(f[0]=="LEAD")field=object[0]=="FIXTURE"?18:13;
          if(object[0]=="FIXTURE"&&f[0]=="BREAKABLE"&&f.size()==15)field=13;
          if(field>=0&&size_t(field)<f.size()&&!f[size_t(field)].empty()&&int(number(f[size_t(field)],-1))==ordinal){dependent=int(i);break;}}
        if(dependent>=0){eraseNativeObject(doc,size_t(dependent));continue;}
      }
      eraseNativeObject(doc,size_t(victim));
    }
    }catch(...){doc=std::move(previous);throw;}
  }
