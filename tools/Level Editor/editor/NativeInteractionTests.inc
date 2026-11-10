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

    std::ofstream report(root/L"native-editor-interaction-test.txt");
    for(const auto& message:passed)report<<"PASS: "<<message<<'\n';
    report<<"EXPORT: "<<utf8(path.wstring())<<'\n';
    verifyCorridors();
  }