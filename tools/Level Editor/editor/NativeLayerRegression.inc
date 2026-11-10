  void verifyLayerRegressions() {
    auto require=[](bool okay,const char* reason){if(!okay)throw std::runtime_error(std::string("Layer regression: ")+reason);};
    std::ofstream report(root/L"native-editor-layer-regression-test.txt");
    auto reset=[&]{doc.newCampaign("Layer repair regression");doc.addRoom("Layer room");map=layer=0;record=-1;selectedRecords.clear();history.clear();future.clear();selectedLayerGroup=-1;layerDraft=0;
      autoDressingEnabled=false;autoDetail=false;richDetail=false;smartSnap=false;mapHeightSelection=floorTransformSelection=false;dirty=false;changeCount=0;refresh();refreshLayerDock();};

    reset();doc.addOnLayer({"FIXTURE","0","6","6","6","0","1.87","0.6","0.99","0","1"},0);refresh();selectRecord(int(doc.records("FIXTURE",0).back()));
    handleLayerCommand(LayerNew,0);SetWindowTextW(layerName,L"Keep this draft");SetWindowTextW(layerElevationInput,L"0");SetWindowTextW(layerThicknessInput,L"0.25");
    auto before=doc.text();int previousRecord=record;auto previousSelection=selectedRecords;for(int i=0;i<50;++i)history.push_back(before+"# snapshot "+std::to_string(i));future.push_back(before);
    auto previousHistory=history,previousFuture=future;bool failed=false;try{handleLayerCommand(LayerCommit,0);}catch(const std::runtime_error&){failed=true;}
    require(failed&&doc.text()==before&&floorLayers().size()==1&&layer==0,"Rejected new floor left a partial document or nonexistent active floor");
    require(history==previousHistory&&future==previousFuture,"Rejected new floor discarded a capped Undo entry or Redo");
    require(record==previousRecord&&selectedRecords==previousSelection&&!dirty&&changeCount==0,"Rejected layer edit changed selection or dirty state");
    require(layerDraft==1&&label(layerName)==L"Keep this draft"&&label(layerElevationInput)==L"0","Rejected new floor lost the artist's editable draft");
    report<<"PASS: rejected layer creation restores document, active floor, selection, full Undo/Redo and editable draft\n"<<std::flush;

    reset();float clearance=ceilingHeight()-layerElevation();moveLayerHeight(0,5);
    require(std::abs((ceilingHeight()-layerElevation())-clearance)<.001f,"Raising the top floor reduced existing headroom");
    auto light=NativeDocument::fields(doc.lines[doc.records("LIGHT",0).front()]);require(std::abs(number(light[4],0)-9.3f)<.001f&&number(light[4],0)<ceilingHeight(),"Moved top-floor light is outside its roof");
    require(validateDocument(),"Engine rejected raised top-floor content");report<<"PASS: raised top storey preserves native headroom and light placement\n"<<std::flush;

    reset();doc.addAutomatic({"FIXTURE","0","6","5.5","5.5","0","1.87","0.6","0.99","0","1"},0);doc.addOnLayer({"FIXTURE","0","6","12.5","5.5","0","1.87","0.6","0.99","0","1"},0);
    rememberDressing({125,126,149,150},2);auto copies=duplicateLayerSet({0});require(copies.size()==1,"Could not duplicate dressed floor");int copied=copies.front();
    auto fixtures=doc.records("FIXTURE",0);require(fixtures.size()==4&&doc.automatic(fixtures[2],copied)&&!doc.automatic(fixtures[3]),"Full-floor duplicate changed generated/user ownership");
    bool settings=false;for(auto i:doc.records("# EDITOR_DRESS",0)){auto f=NativeDocument::fields(doc.lines[i]);settings|=f.size()==5&&f[2]==std::to_string(copied)&&f[3]=="2"&&dressingCells(f[4])==std::vector<int>({125,126,149,150});}
    require(settings,"Full-floor duplicate lost room dressing settings");
    auto folder=root/L".build"/L"layer-regressions";fs::create_directories(folder);doc.save(folder/L"duplicated-dressing.rmc");NativeDocument reopened;reopened.open(folder/L"duplicated-dressing.rmc");require(reopened.text()==doc.text()&&reopened.automatic(fixtures[2],copied),"Generated ownership did not survive save/reopen");
    auto edited=NativeDocument::fields(doc.lines[fixtures[2]]);edited[3]="6";doc.replace(fixtures[2],edited);require(!doc.automatic(fixtures[2])&&doc.automatic(fixtures[0],0),"Editing copied furniture promoted the original or left the copy generated");
    require(validateDocument(),"Engine rejected duplicated dressing");report<<"PASS: floor duplication and save/reopen preserve generated/user ownership and room settings; manual copy edits remain local\n"<<std::flush;

    reset();createFloorLayer();doc.paint(0,1,5,5,'=');doc.addAutomatic({"FIXTURE","0","6","5.5","5.5","3.5","1.87","0.6","0.99","0","1"},1);rememberDressing({125,126},4);refreshLayerDock();handleLayerCommand(LayerMerge,0);
    require(layer==0&&floorLayers().size()==1&&doc.automatic(doc.records("FIXTURE",0).front(),0),"Merge Down lost generated ownership");settings=false;
    for(auto i:doc.records("# EDITOR_DRESS",0)){auto f=NativeDocument::fields(doc.lines[i]);settings|=f.size()==5&&f[2]=="0"&&f[3]=="4";}require(settings,"Merge Down lost source room settings");
    require(validateDocument(),"Engine rejected merged dressing");report<<"PASS: Merge Down retains generated furniture and source room dressing preferences\n"<<std::flush;

#ifdef RAWMETAL_UNIFIED
    reset();doc.addOnLayer({"FIXTURE","0",std::to_string(retro::model::lab_clean_bench),"8","8.28","0","1.18","0.56","0.81","0","1"},0);
    doc.addOnLayer({"FIXTURE","0",std::to_string(retro::model::lab_field_meter),"7.72","8.3","0.81","0.25","0.328","0.148","-2.1","0"},0);
    doc.addOnLayer({"CLUTTER","0","0","4","4","0","0"},0);doc.addOnLayer({"CLUTTER","0","7","8.3","8.32","0.81","0",std::to_string(retro::model::anl_cell_charged),"0.216","0.201","0.406","0","0","cell_broken"},0);
    doc.addOnLayer(NativeDocument::fields("LEAD|0||||0|0|0|0.8|0.007|8|1|record_red|1||||0|1|+|+"),0);require(validateDocument(),"Native seated cable baseline is invalid");
    auto resolved=retro::resolveEditorLeadEndpoints(doc.text(),0);require(resolved.size()==1&&resolved[0][0]>7&&resolved[0][0]<9&&resolved[0][3]>8&&resolved[0][3]<9,"Native resolver did not seat the instrument socket and cell terminal");auto original=doc.lines[doc.records("LEAD",0).front()];auto copiedCable=cloneNativeSet({doc.records("LEAD",0).front()},1,2,.5f);require(copiedCable.size()==1,"Cable-only duplication failed");
    auto cable=NativeDocument::fields(doc.lines[copiedCable.front()]);require(cable[13].empty()&&cable[18].empty()&&cable[19].empty()&&cable[20].empty(),"Copied cable remains attached to uncopied originals");
    for(size_t f=0;f<6;++f)require(std::abs(number(cable[2+f],-999)-(resolved.front()[f]+(f%3==0?1.f:f%3==1?2.f:.5f)))<.001f,"Detached cable endpoint did not use engine-resolved coordinates");
    require(doc.lines[doc.records("LEAD",0).front()]==original,"Cable duplication altered its original");require(validateDocument(),"Engine rejected detached engine-resolved cable copy");
    auto both=cloneNativeSet({doc.records("FIXTURE",0)[1],doc.records("CLUTTER",0)[1],doc.records("LEAD",0).front()},2,0,0);require(both.size()==3,"Attached set copy failed");
    cable=NativeDocument::fields(doc.lines[doc.records("LEAD",0).back()]);require(cable[13]=="2"&&cable[18]=="2"&&cable[19]=="+"&&cable[20]=="+","Cable did not attach to copied fixture/prop");require(validateDocument(),"Engine rejected copied seated cable and its owners");
    report<<"PASS: native socket-resolved cable copies detach uncopied owners, translate real endpoints and reconnect copied owners without changing originals\n"<<std::flush;
#endif
  }
