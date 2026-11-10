  void verifySelectionRegressions() {
    auto require=[](bool okay,const char* reason){if(!okay)throw std::runtime_error(std::string("Selection regression: ")+reason);};
    auto reset=[&]{doc.newCampaign("Selection repair regression");doc.addRoom("Room");map=layer=0;record=-1;
      selectedRecords.clear();history.clear();future.clear();tool=0;snap=.25f;smartSnap=richDetail=autoDressingEnabled=false;
      mapHeightSelection=floorTransformSelection=verticalDragging=bodyDragging=objectDrag=drag=selectingArea=false;refresh();layout();};
    auto fixture=[&](float x,float y,float w,float d,float yaw=0){doc.addOnLayer({"FIXTURE","0","6",nativeNumber(x),nativeNumber(y),"0",nativeNumber(w),nativeNumber(d),".99",nativeNumber(yaw),"1"},0);return int(doc.records("FIXTURE",0).back());};
    auto mouse=[&](UINT message,float x,float y){auto view=planView();SendMessageW(plan,message,MK_LBUTTON,MAKELPARAM(int(std::round(view.ox+x*view.cell)),int(std::round(view.oy+y*view.cell))));};
    std::ofstream report(root/L"native-editor-selection-regression.txt");

    // A drag can publish live preview frames before Escape or before the
    // pointer comes back to its press point. Neither commits an edit.
    for(bool cancel:{true,false}){reset();fixture(8,8,2,1);selectRecord(-1);dirty=false;changeCount=21;
      auto before=doc.text();future={before};auto redoBefore=future;
      mouse(WM_LBUTTONDOWN,8,8);mouse(WM_MOUSEMOVE,9,9);require(objectDrag&&doc.text()!=before,"Plan drag did not edit its preview snapshot");
      publish();require(dirty&&changeCount>21,"The test did not exercise a published drag preview");
      if(cancel){MSG escape{};escape.hwnd=plan;escape.message=WM_KEYDOWN;escape.wParam=VK_ESCAPE;require(shortcut(escape),"Escape did not cancel the real selector drag");}
      else{mouse(WM_MOUSEMOVE,8,8);mouse(WM_LBUTTONUP,8,8);}
      require(doc.text()==before&&history.empty()&&future==redoBefore&&!dirty&&changeCount==21,"Abandoned or zero-distance drag changed dirty state, edit count or Undo/Redo");}
    report<<"PASS: actual plan drag previews, Escape cancellation and return to origin retain clean state and Undo/Redo\n";

    reset();int first=fixture(6,6,2,1),second=fixture(12,10,1,3);selectRecord(first);selectionPick(second,true);
    auto originalFirst=NativeDocument::fields(doc.lines[size_t(first)]),originalSecond=NativeDocument::fields(doc.lines[size_t(second)]);
    for(int turn=0;turn<4;++turn)SendMessageW(window,WM_COMMAND,TurnSelected,0);
    auto checkPose=[&](int index,const std::vector<std::string>& original){auto actual=NativeDocument::fields(doc.lines[size_t(index)]);
      require(std::abs(number(actual[3],0)-number(original[3],0))<.001f&&std::abs(number(actual[4],0)-number(original[4],0))<.001f,"Repeated group quarter-turns drift rectangular props");
      for(size_t i=0;i<original.size();++i)if(i!=3&&i!=4&&i!=9)require(actual[i]==original[i],"Group rotation changed model, dimensions or native flags");};
    checkPose(first,originalFirst);checkPose(second,originalSecond);require(history.size()==4,"Group quarter-turns are not separate Undo entries");
    require(validateDocument(),"Engine rejected rotated native group");
    report<<"PASS: four real group quarter-turn commands restore rectangular prop positions and preserve native fields\n";

    reset();doc.addOnLayer({"STAIR","0","5","5","9","11","0","3.5","18","1","1","1","0.08","1"},0);
    int stair=int(doc.records("STAIR",0).back());second=fixture(14,8,1,1);selectRecord(stair);selectionPick(second,true);
    SendMessageW(window,WM_COMMAND,TurnSelected,0);auto rotated=NativeDocument::fields(doc.lines[size_t(stair)]);
    require(rotated[9]=="0"&&rotated[10]=="1","Group rotation did not turn the stair climbing direction with its footprint");
    for(int turn=0;turn<3;++turn)SendMessageW(window,WM_COMMAND,TurnSelected,0);
    rotated=NativeDocument::fields(doc.lines[size_t(stair)]);require(rotated[9]=="1"&&rotated[10]=="1","Four group turns did not restore the stair climbing direction");
    require(validateDocument(),"Engine rejected group-rotated native stairs");
    report<<"PASS: native stair climb axis and high end follow group rotation\n";

    reset();first=fixture(12,12,6,.5f,1.57079632679f);
    selectArea(11.9f,9.2f,12.1f,9.4f,false);require(selected(first),"Area selection misses a rotated prop's visible long edge");
    selectArea(9.2f,11.9f,9.4f,12.1f,false);require(!selected(first),"Area selection uses the unrotated empty footprint");
    report<<"PASS: area selection follows the native rotated footprint\n";

    for(int operation=0;operation<4;++operation){reset();first=fixture(7,7,2,1);second=fixture(11,10,1,2);selectRecord(first);selectionPick(second,true);
      auto floor=floorLayer(0);floor.locked=true;storeLayerView(floor);auto before=doc.text();auto selection=selectedRecords;
      history.assign(50,before);future={before};auto undoBefore=history,redoBefore=future;dirty=false;changeCount=7;
      bool rejected=false;try{if(operation==0)moveMultiple(1,0);else if(operation==1)copyMultiple();else if(operation==2)removeMultiple();else rotateMultiple();}catch(const std::runtime_error&){rejected=true;}
      require(rejected,"Locked group operation was not refused");require(doc.text()==before&&history==undoBefore&&future==redoBefore,"Rejected group operation erased Redo or displaced the oldest Undo entry");
      require(selectedRecords==selection&&record==second&&!dirty&&changeCount==7,"Rejected group operation changed selection or dirty state");}
    report<<"PASS: rejected group move/copy/delete/rotate retain full Undo, Redo, selection and unchanged native document\n";

    reset();auto mapRecord=doc.records("MAP",0).front();auto mapFields=NativeDocument::fields(doc.lines[mapRecord]);mapFields[9]="4.500";doc.replace(mapRecord,mapFields);
    doc.addOnLayer({"STRUCT","0","5","5","9","9","4.25","4.5","0","3"},0);selectRecord(int(doc.records("STRUCT",0).back()));
    dirty=false;changeCount=31;auto before=doc.text();pickSerial=-1;applyPick("PICK 401 1 7 2 3 0 1 0.5");require(verticalDragging&&verticalKind==1,"Roof top edge could not be grabbed");
    applyPick("PICK 401 1 7 2 3 0 1 1.16666667");require(std::abs(ceilingHeight()-6.5f)<.001f,"Upward roof edge did not expand the map ceiling");
    applyPick("PICK 401 1 7 2 3 0 1 0.66666667");require(std::abs(ceilingHeight()-5.f)<.001f,"Returning from an overshoot left the map ceiling too high");
    applyPick("PICK 401 1 7 2 3 0 1 0.41666667");require(NativeDocument::fields(doc.lines[mapRecord])[9]=="4.500","Returning the edge inside the original ceiling retained expansion or changed its native field text");
    applyPick("PICK 401 1 7 2 3 0 1 0.5");applyPick("PICK 401 0 7 2 3 0 1 0.5");
    require(doc.text()==before&&history.empty()&&!dirty&&changeCount==31,"Returning a roof edge to its press point changed the document, dirty state or Undo");
    report<<"PASS: real 3D roof-edge overshoot, reversal and return restore automatic ceiling expansion with no spurious Undo\n";
  }
