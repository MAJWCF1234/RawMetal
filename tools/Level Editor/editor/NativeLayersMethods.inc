  HWND upperFloorTiles=nullptr;bool upperFloorVoid=false;
  struct FloorLayer {
    int id=0,group=-1;std::string name;float elevation=0,thickness=0,opacity=1;
    bool visible=true,locked=false;std::array<unsigned,24> mask{};
    FloorLayer(){mask.fill(0xffffffu);}
  };
  mutable std::tuple<std::uint64_t,size_t,const void*,int> floorCacheKey{};
  mutable std::vector<FloorLayer> floorCache;
  std::vector<FloorLayer> floorLayers() const {
    auto key=std::tuple{doc.version,doc.lines.size(),static_cast<const void*>(doc.lines.data()),map};
    if(!NativeDocument::verify&&key==floorCacheKey)return floorCache;
    std::vector<FloorLayer> out;
    for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<6)continue;
      FloorLayer a;a.id=int(number(f[2],0));a.name=decoded(f[3]);a.elevation=number(f[4],0);a.thickness=number(f[5],0);
      for(auto j:doc.records("# EDITOR_LAYER_VIEW",map)){auto v=NativeDocument::fields(doc.lines[j]);
        if(v.size()<8||v[2]!=f[2])continue;a.visible=v[3]!="0";a.locked=v[4]=="1";
        a.opacity=std::clamp(number(v[5],1),0.f,1.f);a.group=int(number(v[6],-1));
        std::istringstream bits(v[7]);for(auto& row:a.mask)if(!(bits>>std::hex>>row))row=0xffffffu;
      }out.push_back(a);
    }
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){return a.elevation<b.elevation;});floorCache=out;floorCacheKey=key;return out;
  }
  FloorLayer floorLayer(int id) const {for(const auto& a:floorLayers())if(a.id==id)return a;return {};}
  void storeLayerView(const FloorLayer& a) {
    std::ostringstream mask;for(unsigned row:a.mask)mask<<std::hex<<row<<' ';
    std::vector<std::string> f{"# EDITOR_LAYER_VIEW",std::to_string(map),std::to_string(a.id),a.visible?"1":"0",a.locked?"1":"0",nativeNumber(a.opacity),std::to_string(a.group),mask.str()};
    for(auto i:doc.records(f[0],map)){auto old=NativeDocument::fields(doc.lines[i]);if(old.size()>2&&old[2]==f[2]){doc.replace(i,f,true);return;}}doc.add(f);
  }
  int floorAtHeight(float z) const {
    auto layers=floorLayers();if(layers.empty())return 0;int id=layers.front().id;
    for(const auto& a:layers)if(z>=a.elevation-.04f)id=a.id;return id;
  }
  float bottomLayerElevation() const {for(const auto& a:floorLayers())if(a.id==0)return a.elevation;return 0;}
  static bool relativeNativeHeight(const std::string& kind){return kind=="FIXTURE"||kind=="DOOR"||kind=="TERMINAL"||kind=="PICKUP";}
  void addAuthored(const std::vector<std::string>& f){
    int schema=schemaFor(f);
    if(schema>=0&&schemas[size_t(schema)].scoped&&f.size()>1&&f[1]==std::to_string(map)&&f[0]!="MAP"&&f[0]!="LAYER"&&f[0]!="ROW"&&f[0]!="TIMED_SEQUENCE"&&f[0]!="SURFACE")doc.addOnLayer(f,layer);else doc.add(f);
  }
  int recordFloor(size_t index) const {
    if(auto assigned=doc.recordLayer(index))return *assigned;
    auto f=NativeDocument::fields(doc.lines.at(index));if(f.size()<2||f[1]!=std::to_string(map))return -1;
    if((f[0]=="ROW"||f[0]=="LAYER")&&f.size()>2)return int(number(f[2],-1));
    if(f[0]=="MAP"||f[0]=="TIMED_SEQUENCE"||f[0]=="SURFACE")return -1;
    int schema=schemaFor(f);if(schema<0)return -1;const auto& names=schemas[size_t(schema)].fields;
    for(const char* key:{"base","bottom","z","bed","az"}){auto at=nativeField(names,f,key);
      if(at!=size_t(-1)&&!f[at].empty()&&number(f[at],-1000)>-900)return floorAtHeight(number(f[at],0)+(relativeNativeHeight(f[0])?bottomLayerElevation():0));}
    return -1;
  }
  bool editableRecord(size_t index) const {
    int floor=recordFloor(index);if(floor<0)return true;auto a=effectiveFloorView(floorLayer(floor));if(!a.visible||a.locked||a.opacity<=0)return false;
    auto h=handleFor(index);if(h){int x=cellOf(h->x),y=cellOf(h->y);if(!(a.mask[size_t(y)]&(1u<<x)))return false;}return true;
  }
  bool visibleFloorRecord(size_t index) const {
    auto fields=NativeDocument::fields(doc.lines[index]);if(fields.size()>12&&fields[0]=="FIXTURE"&&fields[12]=="0")return false;
    int owner=recordFloor(index);if(owner<0)return false;auto a=effectiveFloorView(floorLayer(owner));if(!a.visible||a.opacity<=0)return false;
    auto h=handleFor(index);if(h&&!(a.mask[size_t(cellOf(h->y))]&(1u<<cellOf(h->x))))return false;
    if(owner==layer)return true;
    auto f=NativeDocument::fields(doc.lines[index]);auto box=creationBox(f);
    return box&&box->top>layerElevation()+.03f&&box->bottom<creationCeiling()-.03f;
  }
  bool activeFloorRecord(size_t index) const {return visibleFloorRecord(index)&&editableRecord(index);}
  std::vector<size_t> layerContent(const std::vector<int>& ids) const {
    std::set<size_t> chosen;std::set<int> actors;std::set<std::string> events;
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.empty()||f[0]=="ROW"||f[0]=="LAYER"||f[0][0]=='#')continue;
      int floor=recordFloor(i);if(std::find(ids.begin(),ids.end(),floor)==ids.end())continue;
      chosen.insert(i);if(f[0].starts_with("ACTOR_")&&f.size()>2)actors.insert(int(number(f[2],-1)));
      if(f[0]=="SCRIPT_EVENT"&&f.size()>2)events.insert(f[2]);
    }
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<3)continue;
      if(f[0]=="ACTOR"&&f.size()>7&&actors.count(int(number(f[7],-1))))chosen.insert(i);
      if(f[0].starts_with("ACTOR_")&&actors.count(int(number(f[2],-1))))chosen.insert(i);
      if(f[0].starts_with("SCRIPT_")&&events.count(f[2]))chosen.insert(i);
    }return {chosen.begin(),chosen.end()};
  }
  void ensureLayerHeadroom(float elevation) {
    float desired=elevation+2.4f;
    for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>9&&number(f[9],4.5f)<desired){f[9]=nativeNumber(desired);doc.replace(i,f);}}
  }
  int createFloorLayer(const std::string& name="Upper floor") {
    auto layers=floorLayers();int next=0;float elevation=layers.empty()?3.5f:layers.back().elevation+3.5f;
    for(const auto& a:layers){next=std::max(next,a.id+1);elevation=std::max(elevation,a.elevation+3.5f);}
    if(next>=32)throw std::runtime_error("The engine supports up to 32 floors per map");
    int parent=selectedLayerGroup>=0?selectedLayerGroup:floorLayer(layer).group;
    doc.add({"LAYER",std::to_string(map),std::to_string(next),encoded(name),nativeNumber(elevation),"0.25"});
    for(int y=0;y<24;++y)doc.add({"ROW",std::to_string(map),std::to_string(next),std::to_string(y),std::string(24,'_')});
    ensureLayerHeadroom(elevation);layer=next;if(parent>=0){auto view=floorLayer(next);view.group=parent;storeLayerView(view);}return next;
  }
  void moveLayerHeight(int id,float elevation) {
    auto a=floorLayer(id);if(effectiveFloorView(a).locked)throw std::runtime_error("Unlock this floor before moving it");
    float delta=elevation-a.elevation;if(std::abs(delta)<.00001f)return;
    for(const auto& other:floorLayers())if(other.id!=id&&((other.elevation<a.elevation&&elevation<=other.elevation+.25f)||(other.elevation>a.elevation&&elevation>=other.elevation-.25f)))throw std::runtime_error("Keep this floor between its neighboring floor elevations");
    bool moveSpawn=false;for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7)moveSpawn=floorAtHeight(number(f[7],0))==id;}
    std::set<size_t> owned;auto content=layerContent({id});owned.insert(content.begin(),content.end());
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      float move=owned.count(i)?delta:0;
      if(id==0&&relativeNativeHeight(f[0]))move-=delta;
      if(move!=0&&shiftNativeElevation(f,schemas[size_t(schema)].fields,move))doc.replace(i,f,true);}
    for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==id){f[4]=nativeNumber(elevation);doc.replace(i,f);}}
    for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7&&moveSpawn){f[7]=nativeNumber(number(f[7],0)+delta);doc.replace(i,f);}}
    ensureLayerHeadroom(elevation);
  }
  std::vector<size_t> cloneNativeSet(const std::vector<size_t>& indices,float dx,float dy,float dz,const std::map<int,int>& layerMap={}) {
    std::vector<std::vector<std::string>> copies;std::vector<int> copyFloors;std::map<int,int> fixtures,clutter,actors;std::map<std::string,std::string> events;
    auto allFixtures=doc.records("FIXTURE",map),allClutter=doc.records("CLUTTER",map);
    int nextFixture=int(allFixtures.size()),nextClutter=int(allClutter.size()),nextActor=int(doc.records("ACTOR",map).size());
    std::set<size_t> selected(indices.begin(),indices.end());
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);if(f.empty())continue;
      if(f[0]=="FIXTURE")fixtures[int(std::find(allFixtures.begin(),allFixtures.end(),i)-allFixtures.begin())]=nextFixture++;
      if(f[0]=="CLUTTER")clutter[int(std::find(allClutter.begin(),allClutter.end(),i)-allClutter.begin())]=nextClutter++;
      if(f[0]=="ACTOR"&&f.size()>7)actors[int(number(f[7],-1))]=nextActor++;
      if(f[0]=="SCRIPT_EVENT"&&f.size()>2)events[f[2]]=uniqueName("SCRIPT_EVENT",2,f[2]+"_copy_");
    }
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      if(nativeHandle(schemas[size_t(schema)].fields,f))translateNative(f,schemas[size_t(schema)].fields,dx,dy);
      shiftNativeElevation(f,schemas[size_t(schema)].fields,dz);
      auto rewrite=[&](size_t at,const auto& table){if(at<f.size()&&!f[at].empty()){auto found=table.find(int(number(f[at],-1)));if(found!=table.end())f[at]=std::to_string(found->second);}};
      if(f[0]=="FIXTURE"||f[0]=="CLUTTER"){} // ordinals come from native record order
      if(f[0]=="LEAD"){rewrite(13,clutter);rewrite(18,fixtures);}
      if(f[0]=="BREAKABLE"&&f.size()==15)rewrite(13,fixtures);
      if(f[0]=="ACTOR")rewrite(7,actors);else if(f[0].starts_with("ACTOR_"))rewrite(2,actors);
      if(f[0]=="ACTOR_KEY")rewrite(10,actors);
      if(f[0].starts_with("SCRIPT_")&&f.size()>2){auto found=events.find(f[2]);if(found!=events.end())f[2]=found->second;}
      int owner=recordFloor(i);auto mapped=layerMap.find(owner);
      if(mapped!=layerMap.end())owner=mapped->second;
      else if(!layerMap.empty()){int original=layerMap.begin()->first;float height=0;if(auto box=creationBox(NativeDocument::fields(doc.lines[i])))height=box->bottom;
        for(const auto& [source,target]:layerMap)if(floorLayer(source).elevation<=height+.04f)original=source;owner=layerMap.at(original);}
      copyFloors.push_back(owner<0?layer:owner);copies.push_back(std::move(f));
    }
    std::vector<size_t> result;
    for(size_t i=0;i<copies.size();++i){auto& f=copies[i];doc.addOnLayer(f,copyFloors[i]);result.push_back(doc.records(f[0],map).back());}
    return result;
  }
  void eraseNativeSet(std::vector<size_t> indices) {
    // Delete cable/part dependencies before owners. Each operation uses the
    // engine's native ordinal-remapping helper, inside the caller's transaction.
    std::set<size_t> chosen(indices.begin(),indices.end());
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);
      if(f[0]!="FIXTURE"&&f[0]!="CLUTTER")continue;auto all=doc.records(f[0],map);
      int ordinal=int(std::find(all.begin(),all.end(),i)-all.begin());
      for(auto j:doc.onMap(map)){auto d=NativeDocument::fields(doc.lines[j]);int at=d[0]=="LEAD"?(f[0]=="FIXTURE"?18:13):d[0]=="BREAKABLE"&&f[0]=="FIXTURE"&&d.size()==15?13:-1;
        if(at>=0&&size_t(at)<d.size()&&!d[size_t(at)].empty()&&int(number(d[size_t(at)],-1))==ordinal)chosen.insert(j);}
    }
    std::vector<std::string> dependencies,owners;
    for(auto i:chosen){auto f=NativeDocument::fields(doc.lines[i]);(f[0]=="LEAD"||f[0]=="BREAKABLE"?dependencies:owners).push_back(doc.lines[i]);}
    auto eraseLines=[&](const std::vector<std::string>& lines){for(auto it=lines.rbegin();it!=lines.rend();++it){auto found=std::find(doc.lines.begin(),doc.lines.end(),*it);if(found!=doc.lines.end())eraseNativeObject(doc,size_t(found-doc.lines.begin()));}};
    eraseLines(dependencies);eraseLines(owners);
  }
  std::vector<int> layerPassageMask(const NativeDocument& source,int id) const {
    std::vector<int> cells;
    for(auto i:source.records("# EDITOR_PASSAGE",map)){auto f=NativeDocument::fields(source.lines[i]);
      if(f.size()<4||int(number(f[2],-1))!=id)continue;auto part=dressingCells(f[3]);cells.insert(cells.end(),part.begin(),part.end());}
    std::sort(cells.begin(),cells.end());cells.erase(std::unique(cells.begin(),cells.end()),cells.end());return cells;
  }
  void storeLayerPassageMask(int id,const std::vector<int>& cells) {
    std::vector<size_t> markers;
    for(auto i:doc.records("# EDITOR_PASSAGE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2&&int(number(f[2],-1))==id)markers.push_back(i);}
    std::vector<std::string> marker{"# EDITOR_PASSAGE",std::to_string(map),std::to_string(id),dressingMask(cells)};
    if(markers.empty()){if(!marker[3].empty())doc.add(marker);return;}
    // Keep an empty existing marker during a resize: native drag members refer
    // to indices in the press snapshot, which must not shift between moves.
    doc.replace(markers.front(),marker,true);
    for(size_t n=markers.size();n>1;--n)doc.eraseRecord(markers[n-1]);
  }
  void cloneLayerPassageMasks(const std::map<int,int>& mapping) {
    std::vector<std::pair<int,std::vector<int>>> copies;
    for(const auto& [source,target]:mapping){auto cells=layerPassageMask(doc,source),existing=layerPassageMask(doc,target);
      cells.insert(cells.end(),existing.begin(),existing.end());copies.push_back({target,std::move(cells)});}
    for(const auto& [target,cells]:copies)storeLayerPassageMask(target,cells);
  }
  int duplicateFloor(int source) {
    auto a=floorLayer(source);auto content=layerContent({source});int destination=createFloorLayer(a.name+" copy");auto b=floorLayer(destination);
    for(auto i:doc.records("ROW",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==destination)
      {f[4]=std::string(24,'_');int y=int(number(f[3],0));for(int x=0;x<24;++x)f[4][size_t(x)]=doc.tile(map,source,x,y);doc.replace(i,f);}}
    cloneNativeSet(content,0,0,b.elevation-a.elevation,{{source,destination}});cloneLayerPassageMasks({{source,destination}});
    b.group=a.group;b.opacity=a.opacity;b.mask=a.mask;storeLayerView(b);return destination;
  }
  void deleteFloorLayers(const std::vector<int>& ids) {
    auto layers=floorLayers();if(ids.size()>=layers.size())throw std::runtime_error("A map must retain at least one floor");
    for(int id:ids)if(effectiveFloorView(floorLayer(id)).locked)throw std::runtime_error("Unlock selected floors before deleting them");
    float oldBase=bottomLayerElevation();auto oldOrder=floorStackOrder();
    float spawnZ=oldBase;bool removedSpawn=false;for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7){spawnZ=number(f[7],oldBase);removedSpawn=std::find(ids.begin(),ids.end(),floorAtHeight(spawnZ))!=ids.end();}}
    eraseNativeSet(layerContent(ids));std::vector<size_t> remove;
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<3)continue;
      if((f[0]=="LAYER"||f[0]=="ROW"||f[0]=="# EDITOR_LAYER_VIEW"||f[0]=="# EDITOR_DRESS"||f[0]=="# EDITOR_PASSAGE")&&std::find(ids.begin(),ids.end(),int(number(f[2],-1)))!=ids.end())remove.push_back(i);}
    for(auto it=remove.rbegin();it!=remove.rend();++it)doc.eraseRecord(*it);
    // Compact only native layer indices; all other record fields are retained.
    auto remaining=floorLayers();std::map<int,int> remap;for(size_t i=0;i<remaining.size();++i)remap[remaining[i].id]=int(i);
    for(size_t i=0;i<doc.lines.size();++i){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<3||f[1]!=std::to_string(map))continue;
      if(f[0]=="LAYER"||f[0]=="ROW"||f[0]=="# EDITOR_LAYER_VIEW"||f[0]=="# EDITOR_DRESS"||f[0]=="# EDITOR_PASSAGE"||f[0]=="# EDITOR_AUTO"||f[0]=="# EDITOR_LAYER"){
        auto found=remap.find(int(number(f[2],-1)));if(found!=remap.end()){f[2]=std::to_string(found->second);doc.replace(i,f,true);}}}
    for(const auto& a:floorLayers())storeLayerPassageMask(a.id,layerPassageMask(doc,a.id));
    // Relative native heights are measured from layer zero. Compensate when
    // deleting the old bottom floor so surviving furniture stays in world space.
    float baseDelta=oldBase-bottomLayerElevation();
    if(std::abs(baseDelta)>.00001f)for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema>=0&&relativeNativeHeight(f[0])&&shiftNativeElevation(f,schemas[size_t(schema)].fields,baseDelta))doc.replace(i,f,true);}
    std::string order;for(int id:oldOrder)if(remap.count(id))order+=(order.empty()?"":",")+std::to_string(remap.at(id));
    for(auto i:doc.records("# EDITOR_LAYER_ORDER",map))doc.replace(i,{"# EDITOR_LAYER_ORDER",std::to_string(map),order},true);
    layer=0;
    if(removedSpawn)for(auto i:doc.records("MAP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<8)continue;
      float best=1e9f;int bx=12,by=12;for(int y=0;y<24;++y)for(int x=0;x<24;++x){char ch=doc.tile(map,layer,x,y);if(ch!='.'&&ch!='=')continue;float d=std::hypot(x+.5f-number(f[5],12),y+.5f-number(f[6],12));if(d<best){best=d;bx=x;by=y;}}
      f[5]=nativeNumber(bx+.5f);f[6]=nativeNumber(by+.5f);f[7]=nativeNumber(floorLayer(0).elevation);doc.replace(i,f);}
  }
  void transformFloor(int id,int quarter,int flip) {
    if(effectiveFloorView(floorLayer(id)).locked)throw std::runtime_error("Unlock this floor before transforming it");
    std::array<std::string,24> rows;rows.fill(std::string(24,'_'));for(int y=0;y<24;++y){for(int x=0;x<24;++x){auto [tx,ty]=stampCell(x,y,24,24,quarter,flip);rows[size_t(ty)][size_t(tx)]=doc.tile(map,id,x,y);}}
    auto indices=layerContent({id});
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0||!nativeHandle(schemas[size_t(schema)].fields,f))continue;
      stampInto(f,schemas[size_t(schema)].fields,24,24,quarter,flip,0,0);
      if(f[0]=="DOOR"&&f.size()>3&&number(f[2],0)>number(f[3],0))std::swap(f[2],f[3]);
      if(!nativeLineKind(f[0])){auto h=nativeHandle(schemas[size_t(schema)].fields,f);if(h&&h->points.size()==2){auto [ax,ay]=h->points[0];auto [bx,by]=h->points[1];
        if(number(f[ax],0)>number(f[bx],0))std::swap(f[ax],f[bx]);if(number(f[ay],0)>number(f[by],0))std::swap(f[ay],f[by]);}}
      auto facing=nativeField(schemas[size_t(schema)].fields,f,"facing");if(facing!=size_t(-1)){int turn=int(number(f[facing],0));turn=(turn+quarter)%4;if(flip&1)turn=(-turn+4)%4;if(flip&2)turn=(2-turn+4)%4;f[facing]=std::to_string(turn);}
      doc.replace(i,f,true);
    }
    for(auto i:doc.records("ROW",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==id){f[4]=rows[size_t(int(number(f[3],0)))];doc.replace(i,f);}}
    for(auto i:doc.records("# EDITOR_DRESS",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==id){std::vector<int> cells;for(int c:dressingCells(f[4])){auto [x,y]=stampCell(c%24,c/24,24,24,quarter,flip);cells.push_back(y*24+x);}f[4]=dressingMask(cells);doc.replace(i,f,true);}}
    std::vector<int> passage;for(int c:layerPassageMask(doc,id)){auto [x,y]=stampCell(c%24,c/24,24,24,quarter,flip);passage.push_back(y*24+x);}storeLayerPassageMask(id,passage);
    auto a=floorLayer(id);auto old=a.mask;a.mask.fill(0);
    for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(old[size_t(y)]&(1u<<x)){auto [tx,ty]=stampCell(x,y,24,24,quarter,flip);a.mask[size_t(ty)]|=1u<<tx;}storeLayerView(a);
  }  bool showFloorReferences=true,layersDock=false;
  std::string editorViewControl() const {
    std::ostringstream out;auto floors=floorLayers();out<<floors.size()<<'\n';
    for(size_t i=0;i<floors.size();++i){auto a=effectiveFloorView(floors[i]);
      float top=i+1<floors.size()?floors[i+1].elevation-.001f:std::max(ceilingHeight()+1,a.elevation+4);
      float opacity=a.visible?a.opacity:0.f;
      if(a.id!=layer)opacity*=floorReferenceEnabled()?.22f:0.f;
      out<<(i==0?-1000.f:a.elevation-.001f)<<' '<<top<<' '<<opacity;
      for(unsigned mask:a.mask)out<<' '<<mask;out<<'\n';
    }
    auto guides=selectionGuides();auto hover=hoverGuides();guides.insert(guides.end(),hover.begin(),hover.end());out<<guides.size()<<'\n';
    for(const auto& g:guides)out<<g.ax<<' '<<g.ay<<' '<<g.az<<' '<<g.bx<<' '<<g.by<<' '<<g.bz<<' '<<g.kind<<'\n';
    return out.str();
  }
  bool floorReferenceEnabled() const {for(auto i:doc.records("# EDITOR_LAYER_SETTINGS",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2)return f[2]!="0";}return true;}
  void storeFloorReferences(bool enabled){for(auto i:doc.records("# EDITOR_LAYER_SETTINGS",map)){doc.replace(i,{"# EDITOR_LAYER_SETTINGS",std::to_string(map),enabled?"1":"0"},true);return;}doc.add({"# EDITOR_LAYER_SETTINGS",std::to_string(map),enabled?"1":"0"});}
  std::vector<int> floorStackOrder() const {
    std::vector<int> result;for(auto i:doc.records("# EDITOR_LAYER_ORDER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2)result=dressingCells(f[2]);}
    auto floors=floorLayers();std::set<int> present;for(const auto& a:floors)present.insert(a.id);
    std::erase_if(result,[&](int id){return !present.count(id);});
    for(auto it=floors.rbegin();it!=floors.rend();++it)if(std::find(result.begin(),result.end(),it->id)==result.end())result.push_back(it->id);return result;
  }
  void reorderFloorStack(int id,bool up){auto order=floorStackOrder();auto at=std::find(order.begin(),order.end(),id);if(at==order.end())return;
    if(up&&at!=order.begin())std::iter_swap(at,at-1);else if(!up&&at+1!=order.end())std::iter_swap(at,at+1);
    std::string text;for(int v:order)text+=(text.empty()?"":",")+std::to_string(v);
    for(auto i:doc.records("# EDITOR_LAYER_ORDER",map)){doc.replace(i,{"# EDITOR_LAYER_ORDER",std::to_string(map),text},true);return;}doc.add({"# EDITOR_LAYER_ORDER",std::to_string(map),text});}

  std::vector<int> duplicateLayerSet(std::vector<int> sources,const std::string& groupName="") {
    auto original=floorLayers();std::vector<FloorLayer> copies;
    for(const auto& a:original)if(std::find(sources.begin(),sources.end(),a.id)!=sources.end())copies.push_back(a);
    if(copies.empty())return {};
    int maximum=0;for(const auto& a:original)maximum=std::max(maximum,a.id);
    if(maximum+int(copies.size())>=32)throw std::runtime_error("The copied floors would exceed the engine's 32-layer limit");
    auto content=layerContent(sources);float delta=original.back().elevation+3.5f-copies.front().elevation;
    std::map<int,int> mapping;std::vector<int> result;int group=-1;
    if(!groupName.empty()){group=0;for(auto i:doc.records("# EDITOR_LAYER_GROUP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2)group=std::max(group,int(number(f[2],0))+1);}
      doc.add({"# EDITOR_LAYER_GROUP",std::to_string(map),std::to_string(group),encoded(groupName)});}
    for(const auto& a:copies){int id=createFloorLayer(a.name+" copy");auto b=floorLayer(id);float elevation=a.elevation+delta;
      for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>5&&int(number(f[2],-1))==id){f[4]=nativeNumber(elevation);f[5]=nativeNumber(a.thickness);doc.replace(i,f);}}
      for(auto i:doc.records("ROW",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==id){int y=int(number(f[3],0));for(int x=0;x<24;++x)f[4][size_t(x)]=doc.tile(map,a.id,x,y);doc.replace(i,f);}}
      b.elevation=elevation;b.thickness=a.thickness;b.group=group>=0?group:a.group;b.opacity=a.opacity;b.mask=a.mask;b.visible=a.visible;b.locked=a.locked;
      if(groupName=="From visible"){auto effective=effectiveFloorView(a);b.opacity=effective.opacity;b.mask=effective.mask;b.visible=true;b.locked=false;}
      storeLayerView(b);ensureLayerHeadroom(elevation);mapping[a.id]=id;result.push_back(id);
    }
    cloneNativeSet(content,0,0,delta,mapping);cloneLayerPassageMasks(mapping);layer=result.back();return result;
  }
  struct FloorGroup {
    int id=-1;std::string name;float opacity=1;bool visible=true,locked=false;std::array<unsigned,24> mask{};
    FloorGroup(){mask.fill(0xffffffu);}
  };
  std::vector<FloorGroup> floorGroups() const {
    std::vector<FloorGroup> out;for(auto i:doc.records("# EDITOR_LAYER_GROUP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<4)continue;
      FloorGroup g;g.id=int(number(f[2],-1));g.name=decoded(f[3]);if(f.size()>4)g.opacity=std::clamp(number(f[4],1),0.f,1.f);
      if(f.size()>5)g.visible=f[5]!="0";if(f.size()>6)g.locked=f[6]=="1";
      if(f.size()>7){std::istringstream in(f[7]);for(auto& row:g.mask)if(!(in>>std::hex>>row))row=0xffffffu;}out.push_back(g);}
    return out;
  }
  FloorGroup floorGroup(int id) const {for(auto g:floorGroups())if(g.id==id)return g;return {};}
  void storeFloorGroup(const FloorGroup& g) {
    std::ostringstream mask;for(unsigned row:g.mask)mask<<std::hex<<row<<' ';
    std::vector<std::string> f{"# EDITOR_LAYER_GROUP",std::to_string(map),std::to_string(g.id),encoded(g.name),nativeNumber(g.opacity),g.visible?"1":"0",g.locked?"1":"0",mask.str()};
    for(auto i:doc.records(f[0],map)){auto old=NativeDocument::fields(doc.lines[i]);if(old.size()>2&&int(number(old[2],-1))==g.id){doc.replace(i,f,true);return;}}doc.add(f);
  }
  FloorLayer effectiveFloorView(FloorLayer a) const {
    if(a.group>=0){auto g=floorGroup(a.group);a.opacity*=g.opacity;a.visible=a.visible&&g.visible;a.locked=a.locked||g.locked;
      for(size_t y=0;y<24;++y)a.mask[y]&=g.mask[y];}return a;
  }
