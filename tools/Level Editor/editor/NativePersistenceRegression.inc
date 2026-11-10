  // History belongs to one project. Replacing the document must never let an
  // Undo restore another project's text under the newly opened file's name.
  void adoptDocument(NativeDocument next,const fs::path& nextPath,bool unsaved) {
    waitAutosave();
    if(playing)stopPlay();
    objectDrag=objectMoved=drag=brushing=viewDrag=viewRect=false;
    verticalDragging=bodyDragging=selectingArea=false;
    ghosting=ghostShown=livePending=lightPending=false;
    mapHeightSelection=floorTransformSelection=opacityDragging=false;
    dragMates.clear();dragSelection.clear();resizeHandle=-1;
    selectedRecords.clear();record=-1;viewHoverRecord=viewHoverCell=-1;
    selectedLayerGroup=-1;layerDraft=0;
    history.clear();future.clear();
    restoredFrom.clear();restoredAt=-1;
    std::fill(outline.begin(),outline.end(),0);ideas.clear();
    doc=std::move(next);path=nextPath;map=layer=0;
    auto maps=doc.records("MAP");if(!maps.empty()){auto f=NativeDocument::fields(doc.lines[maps.front()]);if(f.size()>1)map=int(number(f[1],0));}
    ++changeCount;dirty=unsaved;autosavedAt=unsaved?changeCount-1:changeCount;
    publish(false,false);showTitle();refresh();relayout();
  }
  void verifyPersistenceRegressions() {
    auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(std::string("Persistence regression: ")+message);};
    std::ofstream report(root/L"native-editor-persistence-regression-test.txt");
    auto folder=session/L"persistence";fs::create_directories(folder);
    NativeDocument first;first.newCampaign("First project");first.addRoom("Room");
    adoptDocument(std::move(first),folder/L"first.rmc",true);
    autoDressingEnabled=false;
    const auto recovery=autosavePath();autosaveTo(recovery);waitAutosave();
    require(autosaveOk&&fs::exists(recovery),"Could not create the recovery fixture");
    const auto before=doc.text();const auto oldPath=path;
    auto blocked=folder/L"blocked.rmc";fs::create_directories(blocked);
    bool rejected=false;try{saveTo(blocked);}catch(const std::exception&){rejected=true;}
    NativeDocument rescued;rescued.open(recovery);
    require(rejected&&rescued.text()==before&&dirty&&path==oldPath,"Failed Save As deleted recovery or changed the active project");
    require(!fs::exists(fs::path(blocked.wstring()+L".editor-tmp-"+std::to_wstring(GetCurrentProcessId()))),"Failed save left its temporary file behind");
    report<<"PASS: failed atomic Save As retains exact recovery, original path and unsaved state\n"<<std::flush;
    autosaveTo(blocked);waitAutosave();
    require(!autosaveOk&&!autosaveError.empty(),"Failed recovery replacement is reported as success or silently ignored");
    autosaveTo(recovery);waitAutosave();require(autosaveOk,"Recovery cannot resume after an error");
    auto saved=folder/L"saved.rmc";saveTo(saved);NativeDocument reopened;reopened.open(saved);
    require(reopened.text()==before&&!dirty&&path==saved&&!fs::exists(recovery),"Successful save did not preserve text and retire its recovery");
    report<<"PASS: recovery replacement errors surface; retry succeeds; completed save/reopen is exact\n"<<std::flush;

    remember();doc.paint(0,0,4,4,'#');publish();future.push_back(before);
    restoredFrom=before;restoredAt=changeCount;selectedLayerGroup=9;
    NativeDocument second;second.newCampaign("Second project");second.addRoom("Other room");auto secondText=second.text();
    auto secondPath=folder/L"second.rmc";adoptDocument(std::move(second),secondPath,false);
    require(history.empty()&&future.empty()&&restoredFrom.empty()&&selectedLayerGroup==-1&&selectedRecords.empty()&&record==-1,"Opening a project retains history, recovery or selection state");
    SendMessageW(window,WM_COMMAND,Undo,0);SendMessageW(window,WM_COMMAND,Redo,0);
    require(doc.text()==secondText&&path==secondPath&&!dirty,"Undo or Redo restored another project under the new project's path");
    remember();doc.addOnLayer({"FIXTURE","0","6","8","8","0","1","1","1","0","1"},0);publish();selectRecord(int(doc.records("FIXTURE",0).back()));
    scrubTo(0);require(doc.text()==secondText&&record==-1&&selectedRecords.empty(),"History slider retained object indices from a different document revision");
    auto unchangedCount=changeCount;auto unchangedDirty=dirty;scrubTo(0);require(changeCount==unchangedCount&&dirty==unchangedDirty,"Unmoved history slider created an edit");
    NativeDocument imported;imported.newCampaign("Imported project");imported.addRoom("Imported room");adoptDocument(std::move(imported),{},true);
    require(dirty&&autosavedAt!=changeCount,"An unsaved imported project is incorrectly considered already recovered");
    dirty=false;remember();SendMessageW(window,WM_COMMAND,New,0);auto blank=doc.text();
    require(history.empty()&&future.empty()&&path.empty(),"New retains a previous project's history or path");
    SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==blank,"Undo crosses the New-project boundary");
    remember();SendMessageW(window,WM_COMMAND,Starter,0);auto starter=doc.text();
    require(history.empty()&&future.empty()&&path.empty(),"Starter retains a previous project's history or path");
    SendMessageW(window,WM_COMMAND,Undo,0);require(doc.text()==starter,"Undo crosses the Starter-project boundary");
    report<<"PASS: project Open/New/Starter isolate Undo/Redo, selection and recovered-work state; unsaved imports remain eligible for recovery\n";
  }
