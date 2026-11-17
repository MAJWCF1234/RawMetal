  // ---- Prefabs (map format 2, docs/Systems/map-format.md) ----
  // Make prefab: the picked things become a PREFAB block (in metres from their middle, map
  // field "@") put above the first MAP, and one PREFAB_PLACE where they stood. Place prefab
  // sets another copy down: of the picked PREFAB_PLACE a metre along, or of the newest prefab
  // at the cursor. Both are one Ctrl+Z. The engine reads the block (CustomCampaign.cpp).
  static bool prefabRecord(const std::string& kind){
    static const std::set<std::string> kinds{"FIXTURE","CLUTTER","PROP","LIGHT","CREATURE","PICKUP","CRATE","CRATE_PLATE","NAV_HINT","TERMINAL","SIGN",
      "AMBIENT","EMITTER","PIPE","PRIM","STRUCT","HAZARD","WATER","DARK","ZONE","SHELTER","COMPACTOR","WET"};
    return kinds.count(kind)!=0;
  }
  std::vector<std::string> prefabNames()const{
    std::vector<std::string> out;for(const auto& line:doc.lines)if(line.starts_with("PREFAB|")){auto f=NativeDocument::fields(line);if(f.size()>1)out.push_back(decoded(f[1]));}return out;
  }
  // Whether a prefab names its records (then each placement needs its own prefix: names are unique per map).
  bool prefabNamed(const std::string& name)const{bool in=false;for(const auto& line:doc.lines){if(line.starts_with("PREFAB|")){in=decoded(NativeDocument::fields(line)[1])==name;continue;}
    if(line.starts_with("PREFAB_END")){if(in)return false;continue;}if(in&&line.find("|name=")!=std::string::npos)return true;}return false;}
  // A prefix no placement on this map uses yet: "<prefab>_<n>_".
  std::string placePrefix(const std::string& name)const{std::set<std::string> used;for(auto i:doc.records("PREFAB_PLACE",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>7)used.insert(decoded(f[7]));}
    for(int n=1;;++n){auto p=name+"_"+std::to_string(n)+"_";if(!used.count(p))return p;}}
  std::string uniquePrefabName()const{auto names=prefabNames();for(int n=1;;++n){auto name="prefab_"+std::to_string(n);if(std::find(names.begin(),names.end(),name)==names.end())return name;}}
  bool makePrefab(){
    auto indices=selectedNativeSet();if(indices.empty()&&record>=0)indices.push_back(size_t(record));
    if(indices.empty()){message(L"Pick the things to make a prefab of first (drag a box round them).");return false;}
    float cx=0,cy=0;int count=0;
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);
      if(!prefabRecord(f[0])){message(wide("A prefab cannot hold "+f[0]+": it names other records by number, or has no place."));return false;}
      if(f[0]=="LIGHT"&&f.size()>26&&!f[26].empty()){message(L"A lamp riding a prop cannot go in a prefab.");return false;}
      if(!editableRecord(i)){message(L"Unlock and show this floor before editing its content.");return false;}
      if(auto h=handleFor(i)){cx+=h->x;cy+=h->y;++count;}}
    if(!count){message(L"These have no place on the plan to make a prefab from.");return false;}
    // The middle, on the quarter metre, is the prefab's origin.
    const float ox=std::round(cx/float(count)*4)/4,oy=std::round(cy/float(count)*4)/4;
    std::vector<std::string> body;
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      translateNative(f,schemas[size_t(schema)].fields,-ox,-oy);f[1]="@";
      body.push_back(NativeDocument::join(f)+NativeDocument::namedText(NativeDocument::named(doc.lines[i])));}
    const auto name=uniquePrefabName();
    auto saved=doc;remember();
    std::vector<size_t> order(indices.begin(),indices.end());std::sort(order.rbegin(),order.rend());
    try{for(auto i:order)eraseNativeObject(doc,i);}catch(const std::exception& e){doc=std::move(saved);history.pop_back();message(wide(e.what()));return false;}
    doc.ensureFormat2();
    // The block goes above the first MAP: a PREFAB must come before its first PREFAB_PLACE.
    size_t at=doc.lines.size();for(size_t i=0;i<doc.lines.size();++i)if(doc.lines[i].starts_with("MAP|")){at=i;break;}
    const std::string cr=doc.crlf?"\r":"";std::vector<std::string> block{"PREFAB|"+encoded(name)+cr};
    for(auto& b:body)block.push_back(b+cr);block.push_back("PREFAB_END"+cr);
    doc.lines.insert(doc.lines.begin()+std::ptrdiff_t(at),block.begin(),block.end());doc.touch();
    std::vector<std::string> place{"PREFAB_PLACE",std::to_string(map),encoded(name),nativeNumber(ox),nativeNumber(oy),"0","0"};
    if(prefabNamed(name))place.push_back(placePrefix(name));
    addAuthored(place);
    selectedRecords.clear();record=int(doc.records("PREFAB_PLACE",map).back());
    publish();selectRecord(record);
    message(wide("Made "+name+" of "+std::to_string(body.size())+" things. Place prefab sets down another; Ctrl+Z takes it back."));
    return true;
  }
  bool placePrefab(){
    std::vector<std::string> place;
    if(record>=0&&NativeDocument::fields(doc.lines[size_t(record)])[0]=="PREFAB_PLACE"){
      place=NativeDocument::fields(doc.lines[size_t(record)]);
      const float step=snap>0?snap:1.f;place[3]=nativeNumber(number(place[3],0)+step);place[4]=nativeNumber(number(place[4],0)+step);
      if(prefabNamed(decoded(place[2]))){place.resize(8);place[7]=placePrefix(decoded(place[2]));}
    }else{
      auto names=prefabNames();if(names.empty()){message(L"There is no prefab yet: pick some things and press Make prefab.");return false;}
      const float x=hoverX>0&&hoverX<24?std::round(hoverX*4)/4:12,y=hoverY>0&&hoverY<24?std::round(hoverY*4)/4:12;
      place={"PREFAB_PLACE",std::to_string(map),encoded(names.back()),nativeNumber(x),nativeNumber(y),"0","0"};
      if(prefabNamed(names.back()))place.push_back(placePrefix(names.back()));
    }
    remember();addAuthored(place);
    record=int(doc.records("PREFAB_PLACE",map).back());selectedRecords.clear();
    publish();selectRecord(record);
    message(L"Placed. R turns it a quarter; Ctrl+Z takes it back.");
    return true;
  }
