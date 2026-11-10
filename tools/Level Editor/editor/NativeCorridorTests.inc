  void verifyCorridors() {
    auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(std::string("Corridor: ")+message);};
    auto folder=root/L".build"/L"corridors";fs::create_directories(folder);std::vector<fs::path> exports;
    std::ofstream report(root/L"native-editor-corridor-test.txt");
    auto reset=[&](bool upper=false){doc.newCampaign("Native corridor regression");doc.addBlankMap("Corridor rooms");map=layer=0;record=-1;history.clear();future.clear();selectedRecords.clear();lastCorridorAnchors.clear();autoDressingEnabled=false;autoDetail=false;richDetail=false;smartSnap=false;brush=1;tool=1;upperFloorVoid=false;floorMaterialExplicit=false;floorPaintMaterial=15;if(upper)createFloorLayer();refresh();};
    auto room=[&](int x0,int y0,int x1,int y1){tool=1;gestureNow(float(x0),float(y0),float(x1),float(y1));};
    auto join=[&](float ax,float ay,float bx,float by){lastCorridorAnchors.clear();tool=CorridorTool;gestureNow(ax,ay,bx,by);};
    auto save=[&](const wchar_t* name,bool below=false){require(!lastCorridorAnchors.empty(),"A successful route has no waypoints");
      std::string ordered;for(int c:lastCorridorAnchors)ordered+=(ordered.empty()?"":",")+std::to_string(c);
      doc.add({"# EDITOR_CORRIDOR_ROUTE","0",std::to_string(layer),nativeNumber(layerElevation()),ordered});
      if(below){auto mask=layerPassageMask(doc,layer);auto at=std::find_if(mask.begin(),mask.end(),[](int c){return c%24==12;});require(at!=mask.end(),"Upper route did not cross the lower wall");doc.add({"# EDITOR_CORRIDOR_WALL_BELOW",nativeNumber(*at%24+.5f),nativeNumber(*at/24+.5f),"0"});}
      path=folder/name;require(validateDocument(),"Engine rejected the authored corridor");saveTo(path);NativeDocument reopened;reopened.open(path);require(reopened.text()==doc.text(),"Saved/reopened native corridor differs");exports.push_back(path);
    };
    for(int quadrant=0;quadrant<4;++quadrant){reset();int ay=quadrant&1?15:3,by=quadrant&1?3:15;room(2,ay-1,8,ay+5);room(15,by-1,21,by+5);
      float ax=5.5f,Ay=ay+2.5f,bx=18.5f,By=by+2.5f;if(quadrant&2){std::swap(ax,bx);std::swap(Ay,By);}auto baseline=doc.text();history.clear();future.clear();join(ax,Ay,bx,By);require(!lastCorridorAnchors.empty(),"A drag quadrant failed");auto built=doc.text();require(history.size()==1,"A corridor is not one Undo");
      auto count=doc.records("LIGHT",0).size();join(ax,Ay,bx,By);require(doc.text()==built&&doc.records("LIGHT",0).size()==count&&history.size()==1,"Repeating a corridor adds geometry, lights or Undo");
      SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==baseline,"Undo did not restore the full corridor transaction");SendMessageW(window,WM_COMMAND,Redo,0);require(doc.text()==built,"Redo changed the corridor");
      doc.loadText(baseline);history.clear();future.clear();join(bx,By,ax,Ay);require(doc.text()==built,"Reversing endpoints changed the route or generated content");if(quadrant==0)save(L"diagonal.rmc");
    }
    report<<"PASS: all drag quadrants and reversals; identical output; stable repeat; one-step Undo/Redo\n"<<std::flush;
    reset();room(2,2,8,9);room(15,2,21,9);
    doc.addOnLayer({"FIXTURE","0","6","11.5","5.5","0","2","2","1.8","0","1"},0);auto item=doc.lines[doc.records("FIXTURE",0).back()];
    join(5.5f,5.5f,18.5f,5.5f);require(!lastCorridorAnchors.empty(),"Route did not detour around authored furniture");require(std::find(doc.lines.begin(),doc.lines.end(),item)!=doc.lines.end(),"Corridor altered user furniture");
    auto box=creationBox(NativeDocument::fields(item));for(int c:layerPassageMask(doc,0))require(!creation::overlaps(*box,{c%24+.5f,c/24+.5f,1,1,0,1.75f}),"Detour still intersects user furniture");save(L"detour.rmc");
    report<<"PASS: complete passage and wall buffer avoid authored furniture\n"<<std::flush;
    reset();room(2,2,8,9);room(15,2,21,9);doc.addOnLayer({"LIGHT","0","11.5","6.5","3.2","0","0","1","7","3000"},0);auto lamp=doc.lines[doc.records("LIGHT",0).back()];
    join(5.5f,5.5f,18.5f,5.5f);require(!lastCorridorAnchors.empty(),"Could not route with an overhead authored lamp");require(std::find(doc.lines.begin(),doc.lines.end(),lamp)!=doc.lines.end(),"Corridor changed an authored lamp");
    auto lightBox=creationBox(NativeDocument::fields(lamp));for(auto i:doc.records("STRUCT",0)){auto f=NativeDocument::fields(doc.lines[i]);if(corridorWall(f))require(!creation::overlaps(*lightBox,*creationBox(f)),"A side wall embeds an authored overhead object");}save(L"overhead.rmc");
    reset();room(2,2,9,8);room(2,15,9,21);for(int x=4;x<=5;++x)doc.paint(0,0,x,8,'.');doc.addOnLayer({"DOOR","0","4","6","8.5","0","0","0","0","0","-1"},0);auto door=doc.lines[doc.records("DOOR",0).back()];
    join(5.5f,5.5f,5.5f,18.5f);require(!lastCorridorAnchors.empty(),"Could not route around a protected door");require(std::find(doc.lines.begin(),doc.lines.end(),door)!=doc.lines.end(),"Corridor changed an authored door");
    auto doorBox=creationBox(NativeDocument::fields(door));for(int c:layerPassageMask(doc,0))require(!creation::overlaps(*doorBox,{c%24+.5f,c/24+.5f,1,1,0,1.75f}),"Route still passes through the authored door");save(L"door-detour.rmc");
    report<<"PASS: walking clearance and full-height wall clearance protect doors and overhead content\n"<<std::flush;
    reset();room(2,2,8,9);room(15,2,21,9);room(9,15,14,21);join(5.5f,5.5f,18.5f,5.5f);auto firstMask=layerPassageMask(doc,0);auto junction=std::find_if(firstMask.begin(),firstMask.end(),[](int c){return c%24==11&&c/24==5;});require(junction!=firstMask.end(),"No central corridor junction for branch test");int branch=*junction;
    join(branch%24+.5f,branch/24+.5f,11.5f,18.5f);require(!lastCorridorAnchors.empty(),"A branch could not open an existing corridor wall");save(L"branch.rmc");
    report<<"PASS: corridors branch into the existing passage network without routing through another room\n"<<std::flush;
    reset();room(2,2,8,9);room(15,2,21,9);for(int y=2;y<=9;++y){doc.paint(0,0,8,y,'.');doc.paint(0,0,15,y,'.');}
    doc.addOnLayer({"STRUCT","0","8","2","9","10","0","4.5","0","18"},0);doc.addOnLayer({"STRUCT","0","15","2","16","10","0","4.5","0","18"},0);
    join(5.5f,5.5f,18.5f,5.5f);require(!lastCorridorAnchors.empty(),"Native wall panels could not be opened");bool remains=false;for(auto i:doc.records("STRUCT",0)){auto f=NativeDocument::fields(doc.lines[i]);remains|=f[9]=="18";}require(remains,"Opening a wall deleted all surrounding panel geometry");save(L"native-walls.rmc");
    report<<"PASS: native STRUCT wall apertures retain surrounding wall fragments and materials\n"<<std::flush;
    for(bool outerBorder:{false,true}){
    reset(true);if(outerBorder)for(int c=0;c<24;++c){doc.paint(0,0,c,0,'#');doc.paint(0,0,c,23,'#');doc.paint(0,0,0,c,'#');doc.paint(0,0,23,c,'#');}
    room(2,2,10,10);room(15,12,22,21);paintNativeFloor(3,3,10,10,15);paintNativeFloor(16,13,22,21,15);paintNativeFloor(4,7,4.25f,9,18);
    tool=2;upperFloorVoid=true;gestureNow(6,4,7,6);upperFloorVoid=false;
    doc.addOnLayer({"PRIM","0","ramp","6.5","5.5","0","1","4","3.5","0","15"},1);auto ramp=doc.lines[doc.records("PRIM",0).back()];
    std::vector<std::string> floors;for(auto i:doc.records("STRUCT",0))if(nativeFloorSlab(NativeDocument::fields(doc.lines[i])))floors.push_back(doc.lines[i]);
    for(int y=0;y<24;++y)doc.paint(0,0,12,y,'#');join(4.5f,8.5f,18.5f,16.5f);require(!lastCorridorAnchors.empty(),"Upper native floor corridor failed");
    for(const auto& line:floors)require(std::find(doc.lines.begin(),doc.lines.end(),line)!=doc.lines.end(),"Corridor changed existing concrete or stripe geometry");require(std::find(doc.lines.begin(),doc.lines.end(),ramp)!=doc.lines.end(),"Corridor changed the ramp");
    auto gap=[&]{for(int y=4;y<=6;++y)for(int x=6;x<=7;++x)if(doc.tile(0,1,x,y)!='.')return false;return true;};require(gap(),"Corridor filled the ramp opening");
    auto mask=layerPassageMask(doc,1);require(!mask.empty(),"No saved passage reservation");autoDressingEnabled=true;dressRoomCells(roomAt(4,8),0,true,6);publish();require(gap(),"Dressing filled a ramp opening after the corridor edit");
    for(auto i:doc.records("FIXTURE",0))if(doc.automatic(i,1)){auto b=creationBox(NativeDocument::fields(doc.lines[i]));require(!b||!passageIntersects(*b),"Managed furniture blocked the reserved passage");}save(outerBorder?L"upper-bordered.rmc":L"upper-concrete.rmc",true);
    auto source=doc.text();auto duplicate=duplicateLayerSet({1});require(duplicate.size()==1&&layerPassageMask(doc,duplicate[0])==mask,"Layer duplication lost passage reservations");
    transformFloor(duplicate[0],0,1);std::set<int> expected;for(int c:mask)expected.insert(c/24*24+23-c%24);auto flipped=layerPassageMask(doc,duplicate[0]);require(std::set<int>(flipped.begin(),flipped.end())==expected,"Floor flip did not transform the reservation");
    deleteFloorLayers(duplicate);require(layerPassageMask(doc,1)==mask,"Floor deletion corrupted surviving passage reservations");doc.loadText(source);layer=1;refreshLayerDock();handleLayerCommand(LayerMerge,0);require(layerPassageMask(doc,0)==mask,"Merge Down lost passage reservations");doc.loadText(source);layer=1;
    }
    report<<"PASS: upper concrete/stripes with open or solid outer borders, real floors, preserved ramp gaps, lower-storey wall, dressing reservation and layer lifecycle\n"<<std::flush;
    reset();room(2,2,8,9);room(15,2,21,9);doc.addOnLayer({"FIXTURE","0","6","12","12","0","2","24","2","0","1"},0);auto unchanged=doc.text();history.clear();join(5.5f,5.5f,18.5f,5.5f);require(doc.text()==unchanged&&history.empty(),"An impossible route partially edited the map");
    join(5.5f,5.5f,6.5f,6.5f);require(doc.text()==unchanged&&history.empty(),"A same-room drag mutated the map");join(0,0,18.5f,5.5f);require(doc.text()==unchanged&&history.empty(),"An invalid endpoint mutated the map");
    report<<"PASS: impossible, same-room and invalid-endpoint drags are non-destructive\n";
    std::ofstream list(folder/L"exports.txt");for(const auto& file:exports)list<<utf8(file.wstring())<<'\n';
  }
