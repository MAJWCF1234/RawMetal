  void verifyInteractions() {
    auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(message);};
    std::vector<std::string> passed;
    auto reset=[&]{
      doc.newCampaign("Layer interaction regression");doc.addRoom("Room");map=layer=0;record=-1;
      selectedRecords.clear();history.clear();future.clear();tool=0;smartSnap=false;richDetail=false;autoDressingEnabled=false;
      mapHeightSelection=floorTransformSelection=verticalDragging=bodyDragging=objectDrag=drag=selectingArea=false;
      layersDock=false;viewMode=1;refresh();layout();
    };
    auto mouse=[&](UINT message,float x,float y,bool shift=false){auto v=planView();
      SendMessageW(plan,message,MK_LBUTTON|(shift?MK_SHIFT:0),MAKELPARAM(int(std::round(v.ox+x*v.cell)),int(std::round(v.oy+y*v.cell))));};
    reset();
    doc.addOnLayer({"FIXTURE","0","6","7.5","7.5","0","1.87","0.6","0.99","0","1","0.65","1"},0);
    doc.addOnLayer({"FIXTURE","0","6","11.5","7.5","0","1.87","0.6","0.99","0","1","0.65","1"},0);
    auto first=doc.records("FIXTURE",0)[0],second=doc.records("FIXTURE",0)[1];
    createFloorLayer();doc.addOnLayer({"FIXTURE","0","6","7.5","7.5","3.5","1.87","0.6","0.99","0","1"},1);
    auto upper=doc.lines[doc.records("FIXTURE",0).back()];layer=0;refresh();
    mouse(WM_LBUTTONDOWN,7.5f,7.5f);mouse(WM_LBUTTONUP,7.5f,7.5f);
    mouse(WM_LBUTTONDOWN,11.5f,7.5f,true);mouse(WM_LBUTTONUP,11.5f,7.5f,true);
    require(selectedRecords.size()==2,"Shift-click did not extend the real selector");
    auto before=doc.text();auto historyCount=history.size();
    mouse(WM_LBUTTONDOWN,11.5f,7.5f);mouse(WM_MOUSEMOVE,12.5f,8.5f);mouse(WM_LBUTTONUP,12.5f,8.5f);
    require(selectedRecords.size()==2,"Dragging a selected object discarded the group");
    auto a=NativeDocument::fields(doc.lines[first]),b=NativeDocument::fields(doc.lines[second]);
    require(number(a[3],0)==8.5f&&number(a[4],0)==8.5f&&number(b[3],0)==12.5f&&number(b[4],0)==8.5f,"Group drag did not move both objects by the same offset");
    require(std::find(doc.lines.begin(),doc.lines.end(),upper)!=doc.lines.end(),"A group drag moved content on another floor");
    require(history.size()==historyCount+1,"A group drag creates more than one Undo");
    SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==before,"Undo did not restore the whole group exactly");
    selectRecord(int(first));selectionPick(int(second),true);copySelected();require(doc.records("FIXTURE",0).size()==5,"Group Copy lost an object");
    removeSelected();require(doc.records("FIXTURE",0).size()==3,"Group Delete removed the wrong objects");
    passed.push_back("actual Shift-click/group drag, Copy/Delete, floor filtering and one-step Undo");

    selectRecord(int(first));auto f=NativeDocument::fields(doc.lines[first]);auto corners=nativeCorners(schemas[size_t(kind)].fields,f);auto fixed=corners[0];
    mouse(WM_LBUTTONDOWN,corners[2].first,corners[2].second);mouse(WM_MOUSEMOVE,corners[2].first+1,corners[2].second+.5f);mouse(WM_LBUTTONUP,corners[2].first+1,corners[2].second+.5f);
    auto resized=NativeDocument::fields(doc.lines[size_t(record)]);auto afterCorners=nativeCorners(schemas[size_t(kind)].fields,resized);
    require(std::hypot(afterCorners[0].first-fixed.first,afterCorners[0].second-fixed.second)<.02f,"Corner resize moved the fixed opposite corner");
    require(number(resized[6],0)>number(f[6],0)+.9f&&resized[2]==f[2]&&resized[8]==f[8]&&resized[11]==f[11],"Corner resize damaged model identity, height or native flags");
    before=doc.text();historyCount=history.size();auto handle=*handleFor(size_t(record));
    mouse(WM_LBUTTONDOWN,handle.x,handle.y);mouse(WM_MOUSEMOVE,handle.x+1,handle.y+1);finishDrag(true);drag=false;
    require(doc.text()==before&&history.size()==historyCount,"Cancel did not restore transforms and layer ownership");
    passed.push_back("real corner resizing, fixed anchor, native field preservation and cancel");

    auto locked=floorLayer(0);locked.locked=true;storeLayerView(locked);before=doc.text();tool=3;gestureNow(4,4,4,4);
    require(doc.text()==before,"A locked floor accepted a wall edit");locked.locked=false;storeLayerView(locked);
    layer=1;layersDock=true;refresh();refreshLayerDock();
    doc.addOnLayer({"CLUTTER","0","0","13","12","3.5","0"},1);
    auto copies=duplicateLayerSet({0,1},"Copied floors");require(copies.size()==2&&floorLayers().size()==4,"Layer duplication did not preserve both floors");
    require(std::abs(floorLayer(copies[1]).elevation-floorLayer(copies[0]).elevation-3.5f)<.001f,"Layer duplication collapsed story heights");
    require(layerContent(copies).size()>=4,"Layer duplication dropped native content");
    require(validateDocument(),"The engine rejected duplicated layers");
    deleteFloorLayers(copies);require(floorLayers().size()==2,"Layer deletion did not leave a valid compact stack");
    auto visible=floorLayer(1);visible.opacity=.5f;visible.mask[2]=0;storeLayerView(visible);
    auto saved=doc.text();NativeDocument reopened;reopened.loadText(saved);doc=std::move(reopened);
    require(floorLayer(1).opacity==.5f&&floorLayer(1).mask[2]==0,"Layer view controls did not save/reopen");
    transformFloor(1,0,1);require(validateDocument(),"A floor flip broke native record fields");
    passed.push_back("layer lock, duplicate/group, story spacing, deletion, masks/opacity, save/reopen and flip");

    // Drive the 3D body drag on its picked face: horizontal movement and
    // elevation are delta-snapped from the grab, with one exact Undo.
    reset();doc.addOnLayer({"FIXTURE","0","6","7.5","7.5","0","1.87","0.6","0.99","0","1"},0);refresh();pickSerial=-1;
    before=doc.text();historyCount=history.size();
    applyHoverRay("HOVER 4 7.5 0.5 1 0 0");require(viewHoverRecord>=0&&!hoverGuides().empty(),"3D hover failed to outline the selectable object");
    applyHoverRay("CLEAR");require(hoverGuides().empty(),"3D hover remains after the pointer leaves");
    applyPick("PICK 81 1 4 7.5 0.5 1 0 0");require(bodyDragging,"Picking an object did not start its 3D body drag");
    float distance=bodyAnchorX-4;std::ostringstream bodyRay;bodyRay<<"PICK 81 1 4 7.5 0.5 1 "<<1.f/distance<<' '<<.5f/distance;
    applyPick(bodyRay.str());auto movedFields=NativeDocument::fields(doc.lines[size_t(record)]);
    require(std::abs(number(movedFields[4],0)-8.5f)<.001f&&std::abs(number(movedFields[5],0)-.5f)<.001f,"3D body drag failed to move the native XYZ fields");
    applyPick("PICK 81 0 4 7.5 0.5 1 0 0");require(history.size()==historyCount+1,"3D body drag is not one Undo");
    SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==before,"3D movement Undo did not restore the document");
    passed.push_back("3D hover/leave and camera-facing XYZ movement through real pick events, with exact Undo");

    reset();layersDock=true;handleLayerCommand(LayerNew,0);require(layerDraft==1&&IsWindowEnabled(layerElevationInput),"New Layer draft cannot be edited");
    SetWindowTextW(layerName,L"Artist catwalk");SetWindowTextW(layerElevationInput,L"3.5");SetWindowTextW(layerThicknessInput,L"0.25");handleLayerCommand(LayerCommit,0);
    require(layer==1&&floorLayer(1).name=="Artist catwalk","New Layer dock did not create the authored floor");
    tool=5;gestureNow(5,5,14,14);tool=0;
    handleLayerCommand(LayerNewGroup,0);SetWindowTextW(layerName,L"Catwalk group");handleLayerCommand(LayerCommit,0);
    int groupId=floorLayer(1).group;require(groupId>=0,"New Layer Group did not collect the selected floor");
    selectedLayerGroup=groupId;refreshLayerDock();auto child=floorLayer(1);historyCount=history.size();
    SendMessageW(layerOpacity,TBM_SETPOS,TRUE,50);layerOpacityChanged(false);layerOpacityChanged(true);
    require(history.size()==historyCount+1&&floorLayer(1).opacity==child.opacity&&effectiveFloorView(child).opacity==.5f,"Group opacity changed children or split one gesture into multiple Undo entries");
    areaX=5;areaY=5;areaEndX=10;areaEndY=10;handleLayerCommand(LayerMaskArea,0);
    require(floorGroup(groupId).mask[6]!=(0xffffffu)&&floorLayer(1).mask==child.mask,"Group mask destructively replaced child masks");
    handleLayerCommand(LayerMaskClear,0);handleLayerCommand(LayerOpaque,0);
    auto planPixel=[&]{RECT r{};GetClientRect(plan,&r);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bitmap);SendMessageW(plan,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);auto v=planView();auto pixel=GetPixel(dc,int(v.ox+12.5f*v.cell),int(v.oy+12.5f*v.cell));SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return pixel;};
    auto opaquePixel=planPixel();handleLayerCommand(LayerHalf,0);auto halfPixel=planPixel();require(opaquePixel!=halfPixel&&halfPixel!=CLR_INVALID,"Opacity does not change the actual 2D floor preview");handleLayerCommand(LayerOpaque,0);
    handleLayerCommand(LayerLock,0);
    require(effectiveFloorView(floorLayer(1)).locked,"Group lock does not lock floor content");handleLayerCommand(LayerLock,0);
    selectedLayerGroup=-1;refreshLayerDock();selectFloorFootprint();auto oldBounds=tileBounds();
    auto floorSnapshot=doc.text();resizeHandle=2;dragDocument=floorSnapshot;captureFloorDrag();resizeFloorLayout(18,17);
    require(tileBounds()[0]==oldBounds[0]&&tileBounds()[1]==oldBounds[1]&&tileBounds()[2]==18&&tileBounds()[3]==17,"Floor footprint resize did not keep the opposite corner anchored");
    require(validateDocument(),"Resized floor footprint is not native-valid");doc.loadText(floorSnapshot);floorTransformSelection=false;objectMoved=false;
    doc.addOnLayer({"CLUTTER","0","0","12","12","3.5","0"},1);selectRecord(-1);refreshLayerDock();
    handleLayerCommand(LayerDuplicate,0);require(floorLayers().size()==3,"Duplicate Layers dock command failed");
    auto count=doc.records("CLUTTER",0).size();handleLayerCommand(LayerMerge,0);
    require(floorLayers().size()==2&&doc.records("CLUTTER",0).size()==count&&validateDocument(),"Merge Down deleted native content or left invalid layers");
    handleLayerCommand(LayerFromVisible,0);require(floorLayers().size()==4&&validateDocument(),"New from Visible lost the story stack or native validity");
    passed.push_back("real layer dock creation/group/mask/lock/opacity, anchored floor footprint, Duplicate/Merge and New from Visible");

    reset();auto mapStart=doc.text();historyCount=history.size();
    for(int edge=0;edge<4;++edge){map=0;layer=0;addAdjacentMap(edge);auto pos=mapOrigin();require(map==edge+1&&layer==0,"Adding a map kept a stale floor or map selection");require(pos==std::pair<float,float>{edge==2?-24.f:edge==3?24.f:0.f,edge==0?-24.f:edge==1?24.f:0.f},"New map is not on its chosen edge");require(validateDocument(),"Engine rejected a chosen-edge map join");}
    map=0;auto occupied=doc.text();try{addAdjacentMap(3);require(false,"An occupied edge was overwritten");}catch(const std::runtime_error&){}require(doc.text()==occupied,"Rejected map join changed the document");
    require(history.size()==historyCount+4,"Map joins are not independent Undo transactions");
    // A deliberately distant room exercises draw culling independently of map IDs.
    doc.addBlankMap("Distant room");auto distant=doc.records("MAP",5).front();auto farFields=NativeDocument::fields(doc.lines[distant]);farFields[3]=farFields[4]="240";doc.replace(distant,farFields);
    auto campaignFolder=root/L".build"/L"interaction";fs::create_directories(campaignFolder);doc.save(campaignFolder/L"map-joins.rmc");
    passed.push_back("all four map edges, native origins, new-map camera/floor selection, occupied-edge rejection and atomic Undo");

    reset();auto untouchedMap=doc.lines[doc.records("MAP",0).front()];std::vector<std::string> untouchedRows;for(auto i:doc.records("ROW",0))untouchedRows.push_back(doc.lines[i]);
    joinMapPassage=false;addAdjacentMap(3);require(doc.lines[doc.records("MAP",0).front()]==untouchedMap,"Adding a map without a passage changed the source header");
    auto sourceRows=doc.records("ROW",0);for(size_t i=0;i<sourceRows.size();++i)require(doc.lines[sourceRows[i]]==untouchedRows[i],"Adding a map without a passage changed user tiles");joinMapPassage=true;
    passed.push_back("optional adjoining-map passage leaves original walls and map settings intact when disabled");

    // A creature's quarry and hunter name (CREATURE fields 10 and 9) through the inspector's own boxes:
    // the picker lists the map's people, the plan draws the line, deleting people renumbers or clears it.
    reset();SendMessageW(roleList,CB_SETCURSEL,2,0);placePerson(5.5f,5.5f);placePerson(9.5f,5.5f);
    tool=CreatureTool;SendMessageW(creatureList,CB_SETCURSEL,6,0);place(16.5f,16.5f);tool=0;
    auto creatureAt=[&]{return doc.records("CREATURE",0).front();};
    selectRecord(int(creatureAt()));layout();syncCreatureControls();
    require(creaturePicked()&&SendMessageW(preyList,CB_GETCOUNT,0,0)==3,"The quarry picker does not list nobody plus the map's two people");
    historyCount=history.size();
    for(LRESULT i=0;i<SendMessageW(preyList,CB_GETCOUNT,0,0);++i)if(SendMessageW(preyList,CB_GETITEMDATA,i,0)==1)SendMessageW(preyList,CB_SETCURSEL,i,0);
    SendMessageW(window,WM_COMMAND,MAKEWPARAM(PreyList,CBN_SELCHANGE),reinterpret_cast<LPARAM>(preyList));
    auto beast=NativeDocument::fields(doc.lines[creatureAt()]);
    require(beast.size()==11&&beast[10]=="1"&&beast[9].empty()&&history.size()==historyCount+1,"Picking a quarry did not write CREATURE field 10 as one Undo");
    SetWindowTextW(hunterList,L"old_tom");SendMessageW(window,WM_COMMAND,MAKEWPARAM(HunterList,CBN_KILLFOCUS),reinterpret_cast<LPARAM>(hunterList));
    beast=NativeDocument::fields(doc.lines[creatureAt()]);require(beast.size()==11&&beast[9]=="old_tom"&&beast[10]=="1","Naming the hunter did not write CREATURE field 9");
    require(validateDocument(),"The engine rejected a creature with a quarry and a hunter name");
    {
      RECT r{};GetClientRect(plan,&r);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
      void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bitmap);SendMessageW(plan,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);
      auto v=planView();int red=0;
      for(float t=.25f;t<=.75f;t+=.01f){float x=16.5f+(9.5f-16.5f)*t,y=16.5f+(5.5f-16.5f)*t;
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)red+=GetPixel(dc,int(v.ox+x*v.cell)+dx,int(v.oy+y*v.cell)+dy)==ui::Creature;}
      SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
      require(red>8,"The plan does not draw the line from the creature to its quarry");
    }
    {
      auto beforeCopy=doc.text();int quarryKey=-1;
      for(auto i:doc.records("ACTOR_KEY",0))if(NativeDocument::fields(doc.lines[i])[2]=="1"){quarryKey=int(i);break;}
      selectRecord(int(creatureAt()));selectionPick(quarryKey,true);copySelected();
      auto copiesOf=doc.records("CREATURE",0);require(copiesOf.size()==2&&doc.records("ACTOR",0).size()==3,"Copying a creature with its quarry lost one of them");
      require(NativeDocument::fields(doc.lines[copiesOf.back()])[10]=="2","A creature copied with its quarry does not hunt the copied person");
      SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==beforeCopy,"Undo did not take the copied creature and person back");
    }
    selectRecord(int(doc.records("ACTOR_KEY",0).front()));removeSelected();
    beast=NativeDocument::fields(doc.lines[creatureAt()]);require(beast.size()==11&&beast[10]=="0","Deleting an earlier person did not renumber the quarry");
    auto quarryKept=doc.text();
    selectRecord(int(doc.records("ACTOR_KEY",0).front()));removeSelected();
    beast=NativeDocument::fields(doc.lines[creatureAt()]);require(beast.size()==10&&beast[9]=="old_tom","Deleting the quarry did not clear CREATURE field 10");
    require(validateDocument(),"The engine rejected the creature after its quarry was deleted");
    SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==quarryKept,"Undo did not bring the quarry back");
    passed.push_back("creature quarry picker (people by name and place), hunter name box, plan line, renumbered and cleared on delete, engine-valid, one-step Undo");

    reset();createFloorLayer();refresh();tool=2;SendMessageW(upperFloorTiles,CB_SETCURSEL,1,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(UpperFloorTiles,CBN_SELCHANGE),reinterpret_cast<LPARAM>(upperFloorTiles));gestureNow(6,6,7,7);
    require(doc.tile(0,1,6,6)=='.',"The upper-floor open-space brush cannot reproduce native empty cells");SendMessageW(upperFloorTiles,CB_SETCURSEL,0,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(UpperFloorTiles,CBN_SELCHANGE),reinterpret_cast<LPARAM>(upperFloorTiles));
    tool=5;gestureNow(5,5,18,14);doc.addOnLayer({"STRUCT","0","5","5","18","14","4.5","4.75","0","3"},1);tool=0;selectRecord(int(doc.records("STRUCT",0).back()));
    pickSerial=-1;applyPick("PICK 91 1 8 2 4 0 1 0.16666667");require(verticalDragging&&verticalKind==2,"The thin roof underside could not be grabbed in 3D");
    applyPick("PICK 91 1 8 2 4 0 1 0.66666667");applyPick("PICK 91 0 8 2 4 0 1 0.66666667");
    auto roofFields=NativeDocument::fields(doc.lines[size_t(record)]);require(std::abs(number(roofFields[6],0)-6.f)<.001f&&std::abs(number(roofFields[7],0)-6.25f)<.001f,"Roof underside drag did not preserve slab thickness");
    placePlayer(12.5f,8.5f);auto roofFolder=root/L".build"/L"interaction";fs::create_directories(roofFolder);doc.save(roofFolder/L"roof-edit.rmc");
    passed.push_back("upper-floor open-space brush and actual 3D underside drag lifting a thin roof with unchanged thickness");

    // A real garage: painted concrete, a local yellow stripe, an erased
    // ramp opening and a second storey made with Duplicate Layers.
    reset();createFloorLayer();refresh();tool=2;floorPaintMaterial=15;gestureNow(2,2,21,21);
    require(!doc.records("STRUCT",0).empty(),"Painting an upper concrete floor only made grating");
    tool=PaintTool;choosePaint({"mat-18",L"Yellow paint",18,0});brush=1;paintGesture(12,8,12.25f,18);
    bool yellow=false,concrete=false;for(auto i:doc.records("STRUCT",0)){auto f=NativeDocument::fields(doc.lines[i]);yellow|=f[9]=="18";concrete|=f[9]=="15";}require(yellow&&concrete,"Painting a stripe replaced the surrounding concrete floor");
    tool=2;upperFloorVoid=true;gestureNow(5,5,7,18);upperFloorVoid=false;
    require(doc.tile(0,1,6,12)=='.',"The ramp gap was filled back in");
    tool=9;SendMessageW(primList,CB_SETCURSEL,3,0);gestureNow(6.5f,12.f,6.5f,12.f);
    auto ramp=doc.records("PRIM",0).back();auto rampFields=NativeDocument::fields(doc.lines[ramp]);rampFields[5]="0";rampFields[6]="3";rampFields[7]="14";rampFields[8]="3.5";rampFields[9]="0";rampFields[10]="15";doc.replace(ramp,rampFields);
    duplicateLayerSet({1});
    // Offset the next solid ramp so its mass does not seal the first ramp's
    // headroom. Repaint its old opening and cut the new one with floor tools.
    auto upperRamp=doc.records("PRIM",0).back();selectRecord(int(upperRamp));moveSelected(4,0);
    tool=2;floorPaintMaterial=15;gestureNow(5,5,7,18);upperFloorVoid=true;gestureNow(9,5,11,18);upperFloorVoid=false;
    placePlayer(10.5f,10.5f);auto garageFolder=root/L".build"/L"interaction";fs::create_directories(garageFolder);doc.save(garageFolder/L"parking-garage.rmc");require(validateDocument(),"The painted multi-storey garage is not native-valid");
    passed.push_back("painted concrete upper floors, local yellow stripe, erased ramp gap, ramp primitive and duplicated garage storey");

    // Recreate the artist's catwalk problem, then fix it through the same 3D edge drag.
    reset();autoDressingEnabled=false;tool=1;gestureNow(2,2,20,20);
    SendMessageW(window,WM_COMMAND,AddFloor,0);require(layer==1&&ceilingHeight()>=5.9f,"New upper floor left the old low roof");
    tool=5;gestureNow(5,5,18,14);tool=3;gestureNow(9,6,9,13);
    tool=0;selectRecord(-1);auto mapIndex=doc.records("MAP",0).front();auto mapFields=NativeDocument::fields(doc.lines[mapIndex]);
    mapFields[9]="4.5";doc.replace(mapIndex,mapFields); // reproduce an older cramped map
    pickSerial=-1;
    applyPick("PICK 71 1 6 8 4 1 0 0.25");applyPick("PICK 71 0 6 8 4 1 0 0.25");
    require(mapHeightSelection,"3D wall/ceiling selection did not expose a height edge");
    applyPick("PICK 72 1 6 8 4 1 0 0.25");require(verticalDragging,"The visible roof edge could not be grabbed");
    historyCount=history.size();applyPick("PICK 72 1 6 8 4 1 0 1.25");applyPick("PICK 72 0 6 8 4 1 0 1.25");
    require(std::abs(ceilingHeight()-6.5f)<.001f,"Upward edge drag did not raise the native ceiling");
    require(history.size()==historyCount+1,"A roof edge drag is not one Undo");
    require(layerElevation()==3.5f,"Raising the roof moved the catwalk itself");
    placePlayer(12.5f,8.5f);applySurfaceSet(2);
    auto exportFolder=root/L".build"/L"interaction";fs::create_directories(exportFolder);
    path=exportFolder/(L"Editor-interaction-regression-"+std::to_wstring(GetCurrentProcessId())+L".rmc");
    require(validateDocument(),"Engine rejected the roof repair before export");saveTo(path);
    NativeDocument roundtrip;roundtrip.open(path);require(roundtrip.text()==doc.text(),"The saved export differs from the editable document");
    std::ofstream(exportFolder/L"latest-export.txt")<<utf8(path.wstring())<<'\n';
    passed.push_back("new-floor headroom, actual 3D edge drag, unchanged deck, player start and native export/reopen");
    verifyDoorRotation(passed,mouse,reset);
    verifyAirLightFilm(passed,mouse,reset);

    std::ofstream report(root/L"native-editor-interaction-test.txt");
    for(const auto& message:passed)report<<"PASS: "<<message<<'\n';
    report<<"EXPORT: "<<utf8(path.wstring())<<'\n';
    verifyCorridors();
  }
  // Doors that stand either way (DOOR field 21 axis, 22 hinge): the Door tool
  // fits a north-south gap along Y, R turns a picked door home in four, each
  // inspector box is one Undo, save/reopen keeps every byte, Turn copy and Flip
  // copy carry the frame, and the plan draws all eight leaves where they swing.
  template <class Mouse, class Reset> void verifyDoorRotation(std::vector<std::string> &passed, Mouse &mouse, Reset &reset) {
    auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(message);};
    auto exportPath=path;
    auto lastDoor=[&]{return doc.records("DOOR",0).back();};
    auto firstDoor=[&]{return doc.records("DOOR",0).front();}; // the door the click fitted
    auto fieldsOf=[&](size_t i){return NativeDocument::fields(doc.lines[i]);};
    auto shown=[](HWND h){return (GetWindowLongW(h,GWL_STYLE)&WS_VISIBLE)!=0;};
    auto box=[&](HWND list,int id,int choice){SendMessageW(list,CB_SETCURSEL,choice,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(list));};
    reset();
    for(int y=1;y<23;++y)if(y!=7&&y!=8)doc.paint(0,0,10,y,'#'); // a north-south wall with a two-cell gap
    refresh();
    tool=DoorTool;doorTurn=0;doorToolSwings=false;layout();
    auto ghost=plannedDoor(10.5f,7.5f,10.5f,7.5f);require(ghost.alongY&&ghost.left==7&&ghost.right==9&&ghost.line==10.5f,"The Door ghost does not take the north-south wall's axis");
    MSG turnKey{plan,WM_KEYDOWN,'R',0};shortcut(turnKey);require(!plannedDoor(10.5f,7.5f,10.5f,7.5f).alongY,"R does not turn the Door tool's ghost");shortcut(turnKey);shortcut(turnKey);shortcut(turnKey);
    require(doorTurn==0&&plannedDoor(10.5f,7.5f,10.5f,7.5f).alongY,"Four turns of the ghost do not bring it back to the wall");
    size_t doorsBefore=doc.records("DOOR",0).size();
    mouse(WM_LBUTTONDOWN,10.5f,7.5f);mouse(WM_LBUTTONUP,10.5f,7.5f);
    require(doc.records("DOOR",0).size()==doorsBefore+1,"A click in a wall gap with the Door tool fitted no door");
    auto f=fieldsOf(firstDoor());
    require(f.size()==22&&f[21]=="1"&&number(f[2],0)==7&&number(f[3],0)==9&&number(f[4],0)==10.5f&&f[10]=="-1","A click in a north-south gap did not fit a door along Y");
    tool=DoorTool;gestureNow(4.5f,12.f,4.5f,15.f);auto dragged=fieldsOf(lastDoor());
    require(dragged.size()==22&&dragged[21]=="1"&&number(dragged[2],0)==12&&number(dragged[3],0)==15&&number(dragged[4],0)==4.5f,"A drag down the plan did not fit a door along Y");
    gestureNow(13,5.5f,16,5.7f);auto across=fieldsOf(lastDoor());
    require(across.size()==11&&number(across[2],0)==13&&number(across[3],0)==16&&number(across[4],0)==5.5f,"A drag across the plan no longer fits a plain door along X");
    tool=0;
    passed.push_back("Door tool: a click in a north-south gap and a drag down the plan fit doors along Y, a drag across stays along X, R turns the ghost");

    // R on the picked door: four quarter turns about its centre, each one Undo, home again.
    selectRecord(int(firstDoor()));layout();require(doorPicked()&&shown(doorKindList)&&shown(doorAxisList),"A picked door shows no door boxes");
    auto historyCount=history.size();box(doorKindList,DoorKindList,1);
    f=fieldsOf(firstDoor());require(f[8]=="1"&&history.size()==historyCount+1,"Opens by: Swings did not write a swinging door as one Undo");
    layout();require(shown(doorSwingList)&&shown(doorHingeList),"A swinging door shows no swing side or hinge boxes");
    auto home=doc.lines[firstDoor()];auto start=*nativeDoor(f);historyCount=history.size();
    std::set<std::tuple<bool,bool,bool>> seen;
    for(int turn=1;turn<=4;++turn){MSG key{plan,WM_KEYDOWN,'R',0};shortcut(key);
      auto d=*nativeDoor(fieldsOf(firstDoor()));auto c=d.centre();
      require(std::abs(c.first-start.centre().first)<1e-4f&&std::abs(c.second-start.centre().second)<1e-4f&&std::abs((d.right-d.left)-(start.right-start.left))<1e-4f,"Turning a door moved its centre or changed its width");
      require(d.alongY==(turn%2==0),"A quarter turn did not stand the door the other way");
      seen.insert({d.alongY,d.hingeRight,d.swing<0});}
    require(seen.size()==4,"Four turns did not pass through four different orientations");
    require(doc.lines[firstDoor()]==home,"Four quarter turns did not bring the door home byte for byte");
    require(history.size()>=historyCount+4,"Each turn is not its own Undo");
    passed.push_back("R turns a picked swinging door a quarter about its centre: four orientations, same width, home byte for byte");

    // The inspector's boxes, one Undo each, and Undo all the way back.
    auto boxesFrom=doc.text();historyCount=history.size();
    box(doorSwingList,DoorSwingList,1);f=fieldsOf(firstDoor());require(f.size()==22&&f[14]=="-1","Swings to: the other side did not write swing -1");
    box(doorHingeList,DoorHingeList,1);f=fieldsOf(firstDoor());require(f.size()==23&&f[22]=="1","Hinge at: the other end did not write hinge 1");
    box(doorAxisList,DoorAxisList,0);f=fieldsOf(firstDoor());auto flat=*nativeDoor(f);
    require(!flat.alongY&&f.size()==23&&f[21].empty()&&number(f[2],0)==9.5f&&number(f[3],0)==11.5f&&number(f[4],0)==8,"Stands: Along X did not turn the door about its centre");
    require(history.size()==historyCount+3,"The door boxes do not make one Undo each");
    require(validateDocument(),"The engine rejected a door edited through its boxes");
    box(doorAxisList,DoorAxisList,1);require(nativeFlag(fieldsOf(firstDoor()),21),"Stands: Along Y did not stand the door along Y again");
    for(int i=0;i<4;++i)SendMessageW(window,WM_COMMAND,Undo,0);
    require(doc.text()==boxesFrom,"Undo did not take the door boxes back exactly");
    // Entry and exit doors stay along X: the box and R both refuse.
    doc.addOnLayer({"DOOR","0","3","5","1.5","0","1","0","0","0","-1"},0);size_t seam=lastDoor();refresh();selectRecord(int(seam));layout();
    auto seamLine=doc.lines[seam];box(doorAxisList,DoorAxisList,1);rotateSelected();
    require(doc.lines[seam]==seamLine&&SendMessageW(doorAxisList,CB_GETCURSEL,0,0)==0,"An entry door was stood along Y");
    eraseNativeObject(doc,seam);refresh();
    passed.push_back("door boxes: opens by, swing side, hinge end and axis about the centre, one Undo each, exact Undo, entry doors kept along X");

    // Lock shown, Breaks down and Starts open (DOOR fields 24, 23, 16) through their controls.
    {
      selectRecord(int(firstDoor()));layout();
      require(shown(doorLockList)&&shown(doorStrengthList)&&shown(doorOpenSlider),"A swinging door shows no lock, strength or starts-open controls");
      auto playFrom=doc.text();auto doorBefore=doc.lines[firstDoor()];historyCount=history.size();
      box(doorLockList,DoorLockList,1);f=fieldsOf(firstDoor());require(f.size()==25&&f[24]=="1"&&f[23].empty(),"Lock shown: Keycard reader did not write DOOR field 24");
      box(doorStrengthList,DoorStrengthList,1);f=fieldsOf(firstDoor());require(f.size()==25&&f[23]=="150"&&f[24]=="1","Breaks down: 150 did not write DOOR field 23");
      SetWindowTextW(doorStrengthList,L"275");SendMessageW(window,WM_COMMAND,MAKEWPARAM(DoorStrengthList,CBN_KILLFOCUS),reinterpret_cast<LPARAM>(doorStrengthList));
      f=fieldsOf(firstDoor());require(f[23]=="275","A strength typed in Breaks down was not kept");
      SendMessageW(doorOpenSlider,TBM_SETPOS,TRUE,35);SendMessageW(window,WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,0),reinterpret_cast<LPARAM>(doorOpenSlider));
      f=fieldsOf(firstDoor());require(std::abs(number(f[16],0)-.35f)<1e-4f&&!nativeFlag(f,17)&&nativeDoor(f)->breakable(),"Starts open did not leave the door ajar");
      require(history.size()==historyCount+4,"Lock, strength and starts-open changes are not one Undo each");
      require(validateDocument(),"The engine rejected a door with a lock, a strength and an ajar leaf");
      auto playFolder=root/L".build"/L"interaction";fs::create_directories(playFolder);auto playSaved=playFolder/L"doors-locks.rmc";saveTo(playSaved);
      NativeDocument back;back.open(playSaved);require(back.text()==doc.text(),"Save/reopen changed a door's lock, strength or open amount");path=exportPath;
      box(doorLockList,DoorLockList,0);box(doorStrengthList,DoorStrengthList,0);SendMessageW(doorOpenSlider,TBM_SETPOS,TRUE,0);SendMessageW(window,WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,0),reinterpret_cast<LPARAM>(doorOpenSlider));
      require(doc.lines[firstDoor()]==doorBefore,"Clearing lock, strength and open did not give back the exact short record");
      for(int i=0;i<7;++i)SendMessageW(window,WM_COMMAND,Undo,0);
      require(doc.text()==playFrom,"Undo did not take the lock, strength and open amount back exactly");
    }
    passed.push_back("door play fields through the controls: Lock shown (24), Breaks down by list and typed (23), Starts open slider (16, not opening), one Undo each, save/reopen byte-identical, engine-valid, exact Undo");

    // Save and reopen keep every byte; the engine reads the doors.
    require(validateDocument(),"The engine rejected doors along Y");
    auto folder=root/L".build"/L"interaction";fs::create_directories(folder);auto saved=folder/L"doors-either-way.rmc";saveTo(saved);
    NativeDocument reopened;reopened.open(saved);require(reopened.text()==doc.text(),"Save/reopen changed a door along Y");
    path=exportPath;

    // Area copy: Turn copy stands the door along X, Flip copy swings it the other way.
    selectRecord(-1);auto source=*nativeDoor(fieldsOf(firstDoor()));
    regionStamp=captureRegion(9,6,11,9);require(std::any_of(regionStamp.objects.begin(),regionStamp.objects.end(),[](auto& o){return o.fields[0]=="DOOR";}),"Copy area left the door behind");
    rotation=1;mirror=0;pasteRegion(14,14);auto turned=*nativeDoor(fieldsOf(lastDoor()));
    require(!turned.alongY&&turned.left==15&&turned.right==17&&turned.line==15.5f&&!turned.hingeRight&&turned.swing<0&&turned.swinging,"Turn copy did not turn the door into a wall along X");
    rotation=0;mirror=1;pasteRegion(6,16);auto flipped=*nativeDoor(fieldsOf(lastDoor()));
    require(flipped.alongY&&flipped.left==17&&flipped.right==19&&flipped.line==7.5f&&flipped.swing==-source.swing&&flipped.hingeRight==source.hingeRight,"Flip copy did not mirror the door's swing");
    rotation=2;mirror=0;pasteRegion(2,2);auto half=*nativeDoor(fieldsOf(lastDoor()));
    require(half.alongY&&half.hingeRight!=source.hingeRight&&half.swing==-source.swing,"A half-turned copy did not turn hinge and swing with it");
    rotation=0;
    require(validateDocument(),"The engine rejected turned and flipped door copies");
    passed.push_back("save/reopen byte-identical with doors along Y; Turn copy, Flip copy and a half turn carry axis, hinge and swing; engine-valid");

    // The plan: all eight swinging doors (axis x swing side x hinge end) and two sliding ones.
    reset();
    for(int x=1;x<18;++x)doc.paint(0,0,x,6,'#');
    for(int y=1;y<23;++y)doc.paint(0,0,19,y,'#');
    for(int x=1;x<18;++x)doc.paint(0,0,x,15,'#');
    std::vector<NativeDoor> drawn;
    for(int i=0;i<8;++i){NativeDoor d;d.swinging=true;d.alongY=i>=4;d.left=float(2+(i%4)*4);d.right=d.left+2;d.line=d.alongY?19.5f:6.5f;d.swing=(i%4)<2?1.f:-1.f;d.hingeRight=i%2==1;d.lock=i%4;if(i==4)d.open=.35f;if(i==5)d.strength=150;
      for(int a=int(d.left);a<int(d.right);++a)doc.paint(0,0,d.alongY?19:a,d.alongY?a:6,'.');
      std::vector<std::string> record={"DOOR","0","0","1","0","0","0","0","0","0","-1"};storeNativeDoor(record,d);doc.addOnLayer(record,0);drawn.push_back(d);}
    for(int i=0;i<2;++i){NativeDoor d;d.alongY=i==1;d.left=i?18:6;d.right=d.left+3;d.line=i?19.5f:15.5f;
      for(int a=int(d.left);a<int(d.right);++a)doc.paint(0,0,d.alongY?19:a,d.alongY?a:15,'.');
      std::vector<std::string> record={"DOOR","0","0","1","0","0","0","0","0","0","-1"};storeNativeDoor(record,d);doc.addOnLayer(record,0);drawn.push_back(d);}
    tool=0;selectRecord(-1);refresh();layout();InvalidateRect(plan,nullptr,TRUE);
    require(validateDocument(),"The engine rejected the eight door orientations");
    RECT r{};GetClientRect(plan,&r);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bitmap);SendMessageW(plan,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();
    auto v=planView();
    auto hits=[&](std::pair<float,float> from,std::pair<float,float> axis,float width){int n=0;
      for(float t=.45f;t<=.81f;t+=.05f){int x=int(v.ox+(from.first+axis.first*width*t)*v.cell),y=int(v.oy+(from.second+axis.second*width*t)*v.cell);
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)n+=GetPixel(dc,x+dx,y+dy)==ui::Door;}
      return n;};
    for(size_t i=0;i<8;++i){auto d=drawn[i];float w=d.right-d.left,angle=d.open>0?d.open:1.f;
      require(hits(d.hinge(),d.leafAxis(angle),w)>=(d.breakable()?4:8),"The plan does not draw a swinging leaf from its hinge toward its swing side, at its open angle");
      if(d.open>0)require(hits(d.hinge(),d.leafAxis(1),w)<4,"The plan draws an ajar leaf wide open");
      auto wrongSide=d;wrongSide.swing=-d.swing;auto wrongEnd=d;wrongEnd.hingeRight=!d.hingeRight;
      // (an ajar leaf and its other-end twin cross mid-way, so that one is checked shut-drawn doors only)
      require(hits(wrongSide.hinge(),wrongSide.leafAxis(angle),w)<4&&(d.open>0||hits(wrongEnd.hinge(),wrongEnd.leafAxis(angle),w)<4),"The plan draws a leaf on the wrong side or at the wrong end");
      // The lock glyph on the wall beside the latch post, and none where no lock is shown.
      auto latch=d.latch(),hinge=d.hinge();float ux=(latch.first-hinge.first)/w,uy=(latch.second-hinge.second)/w;
      int gx=int(v.ox+(latch.first+ux*.3f)*v.cell),gy=int(v.oy+(latch.second+uy*.3f)*v.cell),r2=std::max(3,int(v.cell*.12f)),glyph=0;
      for(int dx=-r2;dx<=r2;++dx)for(int dy=-r2;dy<=r2;++dy)glyph+=GetPixel(dc,gx+dx,gy+dy)==ui::Door;
      require(d.lock?glyph>=6:glyph<3,d.lock?"The plan does not mark a door's lock beside the latch":"The plan marks a lock on a door that shows none");}
    for(size_t i=8;i<10;++i){auto d=drawn[i];require(hits(d.world(d.left,d.line),d.world(1,0),d.right-d.left)>=8,"The plan does not draw a sliding door along its axis");}
    {std::ofstream out(folder/L"doors-plan.bmp",std::ios::binary);BITMAPFILEHEADER head{};head.bfType=0x4D42;head.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
      head.bfSize=DWORD(head.bfOffBits+size_t(r.right)*size_t(r.bottom)*4);out.write(reinterpret_cast<const char*>(&head),sizeof head);
      out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof info.bmiHeader);out.write(static_cast<const char*>(bits),std::streamsize(size_t(r.right)*size_t(r.bottom)*4));}
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    passed.push_back("plan draws all eight swinging doors (leaf from the hinge toward the swing side, never the other side or end, an ajar leaf at its angle, a breakable leaf cracked), lock glyphs beside the latch only where shown, and sliding doors along their axis; picture in .build/interaction/doors-plan.bmp");
  }
  // The Emitter tool, the picture's light and the Film camera tool (NativeEmitterTool.inc,
  // NativeLightView.inc, NativeFilmTool.inc), through the plan's mouse, the inspector's
  // boxes and sliders, R and Undo; the engine reads, exports, draws and films the result.
  // Pictures land in .build/interaction: air-plan.bmp, air-a/b.ppm (the air moving),
  // light-0..3.ppm (lit, probe fill, finish, no probes), third-person.ppm, first-person.ppm,
  // films-plan.bmp and the film's frames under "Film test film".
  template <class Mouse, class Reset> void verifyAirLightFilm(std::vector<std::string> &passed, Mouse &mouse, Reset &reset) {
    auto require=[](bool okay,const std::string& message){if(!okay)throw std::runtime_error(message);};
    auto exportPath=path;
    auto folder=root/L".build"/L"interaction";fs::create_directories(folder);
    auto fieldsOf=[&](size_t i){return NativeDocument::fields(doc.lines[i]);};
    auto shown=[](HWND h){return (GetWindowLongW(h,GWL_STYLE)&WS_VISIBLE)!=0;};
    auto box=[&](HWND list,int id,int choice){SendMessageW(list,CB_SETCURSEL,choice,0);SendMessageW(window,WM_COMMAND,MAKEWPARAM(id,CBN_SELCHANGE),reinterpret_cast<LPARAM>(list));};
    auto slide=[&](HWND s,int at){SendMessageW(s,TBM_SETPOS,TRUE,at);SendMessageW(window,WM_HSCROLL,MAKEWPARAM(TB_ENDTRACK,0),reinterpret_cast<LPARAM>(s));};
    auto about=[](const std::string& v,float want,float tolerance=.002f){return std::abs(number(v,-1e9f)-want)<=tolerance;};
    // The plan as it is drawn, read pixel by pixel, then kept as a picture.
    auto planShot=[&](const wchar_t* name,auto&& inspect){
      InvalidateRect(plan,nullptr,TRUE);RECT r{};GetClientRect(plan,&r);HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
      void* bits=nullptr;auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(dc,bitmap);SendMessageW(plan,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();
      {std::ofstream out(folder/name,std::ios::binary);BITMAPFILEHEADER head{};head.bfType=0x4D42;head.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);
       head.bfSize=DWORD(head.bfOffBits+size_t(r.right)*size_t(r.bottom)*4);out.write(reinterpret_cast<const char*>(&head),sizeof head);
       out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof info.bmiHeader);out.write(static_cast<const char*>(bits),std::streamsize(size_t(r.right)*size_t(r.bottom)*4));}
      try{inspect(dc,planView());}catch(...){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);throw;}
      SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);};
    // The whole builder window (its right pane shows the tool's boxes), kept as a picture.
    auto windowShot=[&](const wchar_t* name){
      RECT was{};GetWindowRect(window,&was);SetWindowPos(window,nullptr,-6000,0,1600,960,SWP_NOZORDER|SWP_NOACTIVATE);ShowWindow(window,SW_SHOWNOACTIVATE);layout();RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN);
      RECT r{};GetClientRect(window,&r);if(r.right<=0||r.bottom<=0)return;HDC screen=GetDC(window);HDC dc=CreateCompatibleDC(screen);auto bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);auto old=SelectObject(dc,bitmap);
      PrintWindow(window,dc,PW_CLIENTONLY|PW_RENDERFULLCONTENT);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
      std::vector<char> bits(size_t(r.right)*size_t(r.bottom)*4);SelectObject(dc,old);GetDIBits(dc,bitmap,0,UINT(r.bottom),bits.data(),&info,DIB_RGB_COLORS);
      std::ofstream out(folder/name,std::ios::binary);BITMAPFILEHEADER head{};head.bfType=0x4D42;head.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);head.bfSize=DWORD(head.bfOffBits+bits.size());
      out.write(reinterpret_cast<const char*>(&head),sizeof head);out.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof info.bmiHeader);out.write(bits.data(),std::streamsize(bits.size()));
      DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,screen);ShowWindow(window,SW_HIDE);SetWindowPos(window,nullptr,was.left,was.top,was.right-was.left,was.bottom-was.top,SWP_NOZORDER|SWP_NOACTIVATE);};
    auto colourNear=[](HDC dc,int x,int y,int reach,COLORREF colour){int n=0;for(int dx=-reach;dx<=reach;++dx)for(int dy=-reach;dy<=reach;++dy)n+=GetPixel(dc,x+dx,y+dy)==colour;return n;};
    // The engine run as the builder runs its picture, hidden: the document, the control and
    // look files in a session folder of their own, N frames, the last one captured.
    auto engineRun=[&](const std::wstring& args,const fs::path& cwd,bool wait=true)->PROCESS_INFORMATION{
      std::wstring line=L"\""+engineExecutable().wstring()+L"\" "+args;STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION p{};
      require(CreateProcessW(nullptr,line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&p)!=0,"The engine could not be started for the picture checks");
      if(wait){if(WaitForSingleObject(p.hProcess,240000)==WAIT_TIMEOUT)TerminateProcess(p.hProcess,9);WaitForSingleObject(p.hProcess,5000);}
      return p;};
    auto finish=[&](PROCESS_INFORMATION p){DWORD code=1;GetExitCodeProcess(p.hProcess,&code);CloseHandle(p.hThread);CloseHandle(p.hProcess);return int(code);};
    struct Look{int mode=-1,third=0,playing=0;unsigned bakes=0;double ms=0;int sources=0,particles=0;float clock=0;int outside=0,rebake=0;};
    auto readLookFile=[&](const fs::path& file){Look s;std::ifstream in(file);std::string tag;in>>tag>>s.mode>>s.third>>s.playing>>s.bakes>>s.ms>>s.sources>>s.particles>>s.clock>>s.outside>>s.rebake;if(tag!="LOOK")s.mode=-1;return s;};
    auto picture=[&](const std::wstring& name,const std::string& look,int frames,int play=0)->Look{
      auto session=folder/(name+L"-session");fs::create_directories(session);
      doc.save(session/L"working.rmc");
      std::ofstream(session/L"control.txt")<<"1 "<<map<<' '<<play<<" 0 0 "<<(play?1:0)<<" 0 0 0\n0\n0\n";
      std::ofstream(session/L"look.txt")<<look;std::error_code ec;fs::remove(session/L"look-status.txt",ec);fs::remove(folder/(name+L".ppm"),ec);
      auto p=engineRun(L"\"--editor-pack="+(session/L"working.rmc").wstring()+L"\" \"--editor-control="+(session/L"control.txt").wstring()+L"\" \"--editor-status="+(session/L"status.txt").wstring()+
                       L"\" --editor-frames="+std::to_wstring(frames)+L" \"--editor-capture="+(folder/(name+L".ppm")).wstring()+L"\" --editor-hidden",session);
      require(finish(p)==0,"The engine picture failed for "+utf8(name));
      require(fs::exists(folder/(name+L".ppm")),"The engine picture wrote no capture for "+utf8(name));
      return readLookFile(session/L"look-status.txt");};
    auto pixels=[&](const std::wstring& name){std::ifstream in(folder/(name+L".ppm"),std::ios::binary);std::string magic;int w=0,h=0,d=0;in>>magic>>w>>h>>d;in.get();std::vector<unsigned char> rgb(size_t(std::max(0,w*h*3)));in.read(reinterpret_cast<char*>(rgb.data()),std::streamsize(rgb.size()));return rgb;};
    auto differ=[&](const std::wstring& a,const std::wstring& b){auto p=pixels(a),q=pixels(b);if(p.size()!=q.size()||p.empty())return -1;int n=0;
      for(size_t i=0;i+2<p.size();i+=3)n+=std::abs(int(p[i])-int(q[i]))+std::abs(int(p[i+1])-int(q[i+1]))+std::abs(int(p[i+2])-int(q[i+2]))>24;return n;};

    // ---- The Emitter tool: every preset placed by a click, each one Undo, all engine-valid.
    reset();
    {auto start=doc.records("MAP",0).front();auto m=fieldsOf(start);m[5]="12";m[6]="1.6";m[20]="1.570796";m[21]="0";doc.replace(start,m);}
    tool=EmitterTool;layout();
    require(shown(emitterPresetList)&&SendMessageW(emitterPresetList,CB_GETCOUNT,0,0)==EmitterPresetCount,"The Emitter tool shows no preset list");
    std::vector<size_t> placed;std::vector<std::pair<float,float>> spots;
    for(int i=0;i<EmitterPresetCount;++i){
      float x=4.f+float(i%4)*5.25f,y=6.f+float(i/4)*5.f;spots.push_back({x,y});
      box(emitterPresetList,EmitterPresetList,i);auto before=doc.records("EMITTER",0).size();auto historyCount=history.size();
      mouse(WM_LBUTTONDOWN,x,y);mouse(WM_LBUTTONUP,x,y);
      require(doc.records("EMITTER",0).size()==before+1&&history.size()==historyCount+1,"A click with the Emitter tool did not place one emitter as one Undo ("+utf8(emitterPresets[i].name)+")");
      auto f=fieldsOf(doc.records("EMITTER",0).back());const auto& p=emitterPresets[i];
      require(f.size()==14&&about(f[2],x)&&about(f[3],y)&&f[12]==std::to_string(p.kind)&&about(f[8],1.f/p.life,.01f)&&f[11]==std::to_string(p.count)&&about(f[9],p.size)&&about(f[13],p.strength),
              "The "+utf8(p.name)+" preset did not write its kit helper's numbers: "+doc.lines[doc.records("EMITTER",0).back()]);
      require(p.fromCeiling?about(f[4],creationCeiling()-p.lift,.01f):about(f[4],p.lift,.01f),"The "+utf8(p.name)+" preset did not leave at its height");
      placed.push_back(doc.records("EMITTER",0).back());}
    require(validateDocument(),"The engine refused the presets: "+utf8(notice));
    passed.push_back("Emitter tool: all "+std::to_string(EmitterPresetCount)+" presets (steam leak and jet, smoke column, spark shower, dust motes, drips, spray, fire, toxic pool, ceiling grit, arc glow, old plume) placed by plan clicks with their kit helpers' numbers, one Undo each, engine-valid");

    // ---- A picked emitter: What, Restyle, Points, Strength, R, its switch and timed window, one Undo each.
    tool=0;auto jet=[&]{return doc.records("EMITTER",0)[1];};selectRecord(int(jet()));layout();
    require(emitterPicked()&&shown(emitterKindList)&&shown(emitterRestyleList)&&shown(emitterHeadingSlider)&&shown(emitterStrengthSlider),"A picked emitter shows no What, Restyle, Points or Strength");
    viewMode=0;layout();windowShot(L"air-window.bmp");viewMode=1;layout();
    auto editFrom=doc.text();auto historyCount=history.size();
    slide(emitterHeadingSlider,90);auto f=fieldsOf(jet());
    require(about(f[5],0,.01f)&&about(f[6],2,.01f),"Points 90 did not carry the jet south at its reach: "+doc.lines[jet()]);
    {MSG key{plan,WM_KEYDOWN,'R',0};shortcut(key);}f=fieldsOf(jet());
    require(about(f[5],-1.414f,.01f)&&about(f[6],1.414f,.01f),"R did not turn the picked jet an eighth round: "+doc.lines[jet()]);
    slide(emitterStrengthSlider,250);f=fieldsOf(jet());require(f[13]=="2.5","Strength 250 did not write 2.5: "+doc.lines[jet()]);
    box(emitterKindList,EmitterKindList,7);f=fieldsOf(jet());
    require(f[12]=="7"&&about(f[8],1.f/1.2f,.01f)&&about(f[5],-.707f,.01f)&&about(f[6],.707f,.01f)&&about(f[2],spots[1].first),"What: Spray did not take the spray's numbers and keep the heading: "+doc.lines[jet()]);
    selectRecord(int(jet()));box(emitterRestyleList,EmitterRestyleList,1+2);f=fieldsOf(jet());
    require(f[12]=="1"&&f[11]=="22"&&SendMessageW(emitterRestyleList,CB_GETCURSEL,0,0)==0,"Restyle: Smoke column did not restyle the emitter: "+doc.lines[jet()]);
    selectRecord(int(jet()));
    {auto set=[&](size_t field,const wchar_t* v){SetWindowTextW(fieldInputs[field-2],v);};set(14,L"vent_on");set(15,L"1");set(17,L"4");set(18,L"1.5");apply(true);}
    f=fieldsOf(jet());require(f.size()==19&&f[14]=="vent_on"&&f[17]=="4"&&f[18]=="1.5","The inspector did not gate the emitter on a switch and a timed window: "+doc.lines[jet()]);
    require(history.size()==historyCount+6,"What, Restyle, Points, Strength, R and the fields are not one Undo each ("+std::to_string(history.size()-historyCount)+")");
    require(validateDocument(),"The engine refused the edited emitter: "+utf8(notice));
    auto edited=doc.text();
    auto dust=doc.records("EMITTER",0)[4];selectRecord(int(dust));auto dustLine=doc.lines[dust];slide(emitterHeadingSlider,45);
    require(doc.lines[dust]==dustLine&&!IsWindowEnabled(emitterHeadingSlider),"Dust motes took a heading");
    for(int i=0;i<6;++i)SendMessageW(window,WM_COMMAND,Undo,0);
    require(doc.text()==editFrom,"Undo did not take the emitter's edits back exactly");
    for(int i=0;i<6;++i)SendMessageW(window,WM_COMMAND,Redo,0);
    require(doc.text()==edited,"Redo did not bring the emitter's edits back exactly");
    passed.push_back("picked emitter: Points slider, R (an eighth), Strength slider, What (takes that kind's numbers, keeps the heading), Restyle from a preset, switch and timed window through the fields; one Undo each, exact Undo/Redo; dust refuses a heading");

    // ---- The plan: every glyph in its kind's colour, the carry arrow, the dust box.
    selectRecord(-1);
    planShot(L"air-plan.bmp",[&](HDC dc,const auto& v){
      for(size_t i=0;i<placed.size();++i){auto g=fieldsOf(doc.records("EMITTER",0)[i]);int kind=emitterKindOf(g);
        int x=int(v.ox+number(g[2],0)*v.cell),y=int(v.oy+number(g[3],0)*v.cell);
        require(colourNear(dc,x,y,int(v.cell*.6f),emitterColor(kind))>=6,"The plan does not draw the "+utf8(emitterKindNames[kind])+" emitter's glyph in its colour");}
      auto leak=fieldsOf(doc.records("EMITTER",0)[0]);int x=int(v.ox+number(leak[2],0)*v.cell),y=int(v.oy+number(leak[3],0)*v.cell),along=0;
      for(float t=.35f;t<=.65f;t+=.05f)along+=colourNear(dc,x+int(t*v.cell),y,1,emitterColor(2));
      require(along>=4,"The plan does not draw the steam leak's arrow east");
      auto motes=fieldsOf(dust);int bx=int(v.ox+(number(motes[2],0)-1.5f)*v.cell),by=int(v.oy+number(motes[3],0)*v.cell);
      require(colourNear(dc,bx,by,2,emitterColor(4))>=1,"The plan does not draw the box dust motes fill");});
    passed.push_back("plan draws every emitter kind's glyph in its colour, the carry arrow and the box dust fills; picture in .build/interaction/air-plan.bmp");

    // ---- Save/reopen, and the engine's own exporter writes them back the same.
    {auto saved=folder/L"air-export.rmc";saveTo(saved);NativeDocument back;back.open(saved);require(back.text()==doc.text(),"Save/reopen changed an emitter");path=exportPath;
     std::error_code ec;fs::remove(saved.wstring()+L".exported.rmc",ec);
     require(finish(engineRun(L"\"--editor-export="+saved.wstring()+L"\"",folder))==0,"The engine could not export the emitters");
     NativeDocument exported;exported.open(saved.wstring()+L".exported.rmc");auto mine=doc.records("EMITTER",0),theirs=exported.records("EMITTER",0);
     require(mine.size()==theirs.size(),"The engine's export lost emitters ("+std::to_string(theirs.size())+" of "+std::to_string(mine.size())+")");
     for(size_t i=0;i<mine.size();++i){auto a=fieldsOf(mine[i]),b=NativeDocument::fields(exported.lines[theirs[i]]);a.resize(20);b.resize(20);
       for(size_t k=2;k<20;++k){if(k==14){require(a[k].empty()==b[k].empty(),"The export lost an emitter's switch");continue;}
         require(std::abs(number(a[k],k==13?1.f:k==15?1.f:0.f)-number(b[k],k==13?1.f:k==15?1.f:0.f))<.002f,"The export changed field "+std::to_string(k)+": "+doc.lines[mine[i]]+" / "+exported.lines[theirs[i]]);}}}
    passed.push_back("save/reopen byte-identical; the engine's exporter (--editor-export) writes every emitter back with the same numbers, switch and window");

    // ---- The picture: the emitters show and move in the builder's 3D view (the clock runs between edits).
    {auto start=doc.records("EMITTER",0);auto g=fieldsOf(start[1]);g.resize(14);doc.replace(start[1],g);} // the smoke column ungated for the picture
    auto a=picture(L"air-a","LOOK 0 0 0 1\n",30),b=picture(L"air-b","LOOK 0 0 0 1\n",120);
    require(a.mode==0&&a.sources>=8&&a.particles>100,"The builder's picture draws no air ("+std::to_string(a.sources)+" sources, "+std::to_string(a.particles)+" particles)");
    require(b.clock>a.clock+.3f,"The picture's clock does not run between edits");
    int moved=differ(L"air-a",L"air-b");require(moved>800,"The air does not move in the picture ("+std::to_string(moved)+" pixels changed)");
    auto still=picture(L"air-still","LOOK 0 0 0 0\n",60);require(still.clock<.05f,"Air moves: off still ran the clock");
    passed.push_back("3D view: "+std::to_string(b.sources)+" emitters, "+std::to_string(b.particles)+" particles drawn by the engine in the builder's picture; the clock runs between edits ("+std::to_string(moved)+" pixels moved) and holds when asked; air-a.ppm, air-b.ppm");

    // ---- Light: lit, probe fill, finish, no probes; Rebake while the picture runs; a crystal's glow said.
    addAuthored({"FIXTURE","0",std::to_string(retro::model::res_crystal_l),"20","20","0","1.83","1.66","2.91","0","1"});
    selectRecord(int(doc.records("FIXTURE",0).back()));layout();
    require(crystalGlow()==L"Cyan glow 4.0, reaches 6.0 m","A picked crystal does not say its glow: "+utf8(crystalGlow()));
    {auto c=fieldsOf(size_t(record));c[8]="1.455";doc.replace(size_t(record),c);require(crystalGlow()==L"Cyan glow 2.0, reaches 4.2 m","A half-size crystal's glow is not halved: "+utf8(crystalGlow()));c[8]="2.91";doc.replace(size_t(record),c);}
    selectRecord(-1);lightMenuChosen(LightMenuBase+1);require(lightView==1&&lookText()=="LOOK 1 0 0 1\n","The Light menu's Probe fill did not reach the look file: "+lookText());
    lightMenuChosen(LightMenuBase+5);require(!airMoves&&lookText()=="LOOK 1 0 0 0\n","Air moves did not switch off");lightMenuChosen(LightMenuBase+5);
    lightMenuChosen(LightMenuBase+4);require(rebakeSerial==1&&lookText()=="LOOK 1 0 1 1\n","Rebake did not ask the picture");
    SendMessageW(window,WM_COMMAND,ThirdPersonToggle,0);require(thirdPersonPlay&&lookText()=="LOOK 1 1 1 1\n","The play toggle did not ask for the shoulder camera");
    SendMessageW(window,WM_COMMAND,ThirdPersonToggle,0);lightMenuChosen(LightMenuBase);rebakeSerial=0;
    std::array<Look,4> modes;for(int m=0;m<4;++m)modes[size_t(m)]=picture(L"light-"+std::to_wstring(m),"LOOK "+std::to_string(m)+" 0 0 0\n",24);
    for(int m=1;m<4;++m){require(modes[size_t(m)].mode==m,"The picture did not take light mode "+std::to_string(m));int d=differ(L"light-0",L"light-"+std::to_wstring(m));
      require(d>2000,"Light mode "+std::to_string(m)+" looks the same as lit ("+std::to_string(d)+" pixels)");}
    {auto session=folder/L"rebake-session";fs::create_directories(session);doc.save(session/L"working.rmc");std::error_code ec;fs::remove(session/L"look-status.txt",ec);
     std::ofstream(session/L"control.txt")<<"1 0 0 0 0 0 0 0 0\n0\n0\n";std::ofstream(session/L"look.txt")<<"LOOK 0 0 0 0\n";
     auto p=engineRun(L"\"--editor-pack="+(session/L"working.rmc").wstring()+L"\" \"--editor-control="+(session/L"control.txt").wstring()+L"\" \"--editor-status="+(session/L"status.txt").wstring()+L"\" --editor-frames=600 --editor-hidden",session,false);
     Look first;for(int i=0;i<2400&&(first=readLookFile(session/L"look-status.txt")).bakes<1;++i)Sleep(25);
     require(first.bakes>=1,"The picture never baked its probes");
     {std::ofstream(session/L"look.txt.tmp")<<"LOOK 0 0 1 0\n";}MoveFileExW((session/L"look.txt.tmp").c_str(),(session/L"look.txt").c_str(),MOVEFILE_REPLACE_EXISTING);
     Look again;for(int i=0;i<2400&&(again=readLookFile(session/L"look-status.txt")).bakes<=first.bakes;++i)Sleep(25);
     if(WaitForSingleObject(p.hProcess,240000)==WAIT_TIMEOUT)TerminateProcess(p.hProcess,9);finish(p);
     require(again.bakes>first.bakes&&again.rebake==1,"Rebake did not bake the probes again in the running picture ("+std::to_string(first.bakes)+" then "+std::to_string(again.bakes)+")");
     passed.push_back("light view: lit, probe fill (--detail-probes), finish (--detail-finish) and no probes (--no-probes) as runtime switches, each a different picture (light-0..3.ppm); Rebake bakes again in the running picture ("+std::to_string(int(again.ms))+" ms); a crystal's glow said from its model and size; the Light menu and play toggle reach the look file");}

    // ---- Play as Claire over her shoulder: she is drawn from outside; first person she is not.
    // Her start 3 m off the north wall, so the shoulder camera has room behind her.
    {auto start=doc.records("MAP",0).front();auto m=fieldsOf(start);m[6]="4.5";doc.replace(start,m);}
    {auto third=picture(L"third-person","LOOK 0 1 0 1\n",150,1),first=picture(L"first-person","LOOK 0 0 0 1\n",150,1);
     require(third.playing==1&&third.third==1&&third.outside==1,"Play over the shoulder did not draw Claire from outside");
     require(first.playing==1&&first.outside==0,"First-person play drew Claire from outside");
     require(differ(L"third-person",L"first-person")>5000,"The shoulder camera's picture is the first-person one");
     passed.push_back("play as Claire: the shoulder camera draws her from outside (third-person.ppm), first person does not (first-person.ppm)");}

    // ---- Film cameras: a click, R and a drag place named cameras; the inspector moves one and turns one on Claire.
    reset();
    {auto start=doc.records("MAP",0).front();auto m=fieldsOf(start);m[5]="12";m[6]="12";m[20]="-1.570796";doc.replace(start,m);}
    for(int i=0;i<3;++i){tool=EmitterTool;emitterPreset=i==0?2:i==1?0:10;place(8.f+4.f*float(i),9.f);}
    tool=FilmTool;filmTurn=0;layout();
    require(shown(GetDlgItem(window,FilmWrite))&&shown(GetDlgItem(window,FilmRender)),"The Film camera tool shows no Write or Render");
    historyCount=history.size();
    mouse(WM_LBUTTONDOWN,3.f,12.f);mouse(WM_LBUTTONUP,3.f,12.f);
    selectRecord(-1);{MSG key{plan,WM_KEYDOWN,'R',0};shortcut(key);}require(filmTurn==1,"R did not turn the next camera");
    selectRecord(-1);mouse(WM_LBUTTONDOWN,3.f,3.f);mouse(WM_LBUTTONUP,3.f,3.f);
    gestureNow(12.f,20.f,12.f,10.f);
    auto cams=doc.records(FilmKind,0);require(cams.size()==3&&history.size()==historyCount+3,"Click, click and drag did not place three cameras as three Undo");
    auto c0=filmCamera(fieldsOf(cams[0])),c1=filmCamera(fieldsOf(cams[1])),c2=filmCamera(fieldsOf(cams[2]));
    require(c0.name=="shot_1"&&c1.name=="shot_2"&&c2.name=="shot_3"&&c0.yaw==0&&c1.yaw==45&&c2.yaw==270&&about(fieldsOf(cams[0])[5],1.6f),"Cameras were not named, turned and aimed as placed: "+doc.lines[cams[2]]);
    selectRecord(int(cams[2]));layout();require(filmPicked()&&!fieldShown.empty(),"A picked camera shows no fields");
    {auto set=[&](size_t field,const wchar_t* v){SetWindowTextW(fieldInputs[field-2],v);};set(8,L"2");set(9,L"16");set(10,L"16");set(12,L"300");set(7,L"-10");apply(true);}
    c2=filmCamera(fieldsOf(cams[2]));require(c2.seconds==2&&c2.bx==16&&c2.by==16&&c2.byaw==300&&c2.pitch==-10,"The inspector did not make a moving shot: "+doc.lines[cams[2]]);
    selectRecord(int(cams[0]));{MSG key{plan,WM_KEYDOWN,'R',0};shortcut(key);}require(filmCamera(fieldsOf(cams[0])).yaw==45,"R did not turn the picked camera");
    selectRecord(int(cams[1]));{auto set=[&](size_t field,const wchar_t* v){SetWindowTextW(fieldInputs[field-2],v);};set(14,L"1");set(2,L"claire look");apply(true);}
    require(filmCamera(fieldsOf(cams[1])).claire&&filmCamera(fieldsOf(cams[1])).name=="claire look","A camera was not turned on Claire");
    require(history.size()==historyCount+6,"Camera edits are not one Undo each");
    require(validateDocument(),"The engine refused a map with film cameras: "+utf8(notice));
    auto filmed=doc.text();for(int i=0;i<6;++i)SendMessageW(window,WM_COMMAND,Undo,0);
    require(doc.records(FilmKind,0).empty(),"Undo did not take the cameras away");for(int i=0;i<6;++i)SendMessageW(window,WM_COMMAND,Redo,0);
    require(doc.text()==filmed,"Redo did not bring the cameras back exactly");
    selectRecord(-1);
    planShot(L"films-plan.bmp",[&](HDC dc,const auto& v){
      for(auto i:doc.records(FilmKind,0)){auto c=filmCamera(fieldsOf(i));require(colourNear(dc,int(v.ox+c.ax*v.cell),int(v.oy+c.ay*v.cell),4,ui::Film)>=8,"The plan does not draw camera "+c.name);}
      auto c=filmCamera(fieldsOf(doc.records(FilmKind,0)[2]));int dashes=0;for(float t=.3f;t<=.7f;t+=.05f)dashes+=colourNear(dc,int(v.ox+(c.ax+(c.bx-c.ax)*t)*v.cell),int(v.oy+(c.ay+(c.by-c.ay)*t)*v.cell),1,ui::Film)>0;
      require(dashes>=4,"The plan does not draw the moving shot's path");
      require(colourNear(dc,int(v.ox+c.bx*v.cell),int(v.oy+c.by*v.cell),3,ui::Film)>=3,"The plan does not draw the moving shot's end");});
    selectRecord(int(doc.records(FilmKind,0)[2]));
    tool=FilmTool;viewMode=0;layout();windowShot(L"films-window.bmp");viewMode=1;layout();
    {std::vector<retro::EditorGuide> guides;filmGuides(guides);require(guides.size()==17,"The picked camera's view is not outlined in 3D ("+std::to_string(guides.size())+")");}
    passed.push_back("Film camera tool: click (R turns the next), drag aims, names shot_1..3; inspector makes a 2 s move and a camera on Claire; R turns a picked one; one Undo each, exact Undo/Redo; engine-valid; plan wedges, path and end (films-plan.bmp); 3D frustum guides");

    // ---- The --film list beside the project, and the engine films it into pictures.
    {auto project=folder/L"Film test.rmc";saveTo(project);
     auto pictures=folder/L"Film test film";std::error_code ec;fs::remove_all(pictures,ec);
     auto jobs=writeFilmLists();require(jobs.size()==1&&jobs[0].list==folder/L"Film test.film.txt"&&jobs[0].folder==pictures,"The film list is not beside the project");
     std::ifstream list(jobs[0].list);std::vector<std::string> lines;for(std::string l;std::getline(list,l);)lines.push_back(l);
     require(lines.size()>2&&lines[0].starts_with("custom 0 12.000 12.000 0.000 -90.000 0 ")&&lines[0].ends_with(utf8(project.wstring())),"The film list does not start from the project at the map's start: "+(lines.empty()?std::string():lines[0]));
     int stills=0,moving=0,claire=0;for(auto& l:lines){if(l.starts_with("f shot_1 ")||l.starts_with("f shot_3 "))++stills;if(l.starts_with("f shot_3_"))++moving;if(l.starts_with("cam "))++claire;}
     require(stills==1&&moving==21&&claire==2,"The film list's frames are wrong (still "+std::to_string(stills)+", moving "+std::to_string(moving)+", Claire "+std::to_string(claire)+")");
     int frames=0,code=filmRender(filmEngine(),jobs[0],frames);
     require(code==0&&frames==1+21+1,"The engine did not film the list ("+std::to_string(frames)+" pictures, exit "+std::to_string(code)+")");
     for(auto name:{L"shot_1.bmp",L"shot_3_0000.bmp",L"shot_3_0020.bmp",L"claire_look_0000.bmp"})require(fs::exists(pictures/name),"No film picture "+utf8(name));
     require(fs::file_size(pictures/L"shot_3_0000.bmp")>1000000,"A film picture is not a whole frame");
     path=exportPath;
     passed.push_back("film list beside the project (custom stage at the map's start, a still, a 21-frame move, Claire's replay camera); RawMetal.exe --film filmed "+std::to_string(frames)+" pictures into \"Film test film\"");}
  }
