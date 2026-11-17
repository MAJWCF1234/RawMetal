  // Map format 2 in the Builder (--editing-test; docs/Systems/map-format.md): the inspector's
  // named rows (add, edit, remove, labels from the guide), named fields kept through moves,
  // copies, undo/redo and save/reopen, the index leaving FORMAT and PREFAB blocks alone, and
  // Make prefab / Place prefab, with the engine checking every step it can.
  void verifyFormatTwo() {
    auto require=[](bool okay,const std::string& message){if(!okay)throw std::runtime_error("Format 2 regression: "+message);};
    std::ofstream report(root/L"native-editor-format-test.txt");
    NativeDocument blank;blank.newCampaign("Format two");blank.addRoom("Room");
    adoptDocument(std::move(blank),{},true);autoDressingEnabled=false;
    require(namedLabels.count("tint")&&namedLabels.count("frozen")&&namedLabels.count("faction"),"the guide's named-field labels were not read");
    remember();doc.addOnLayer({"FIXTURE","0","24","8","8","0","1.4","0.7","0.75","0","1"},0);publish();
    auto fixture=[&]{return int(doc.records("FIXTURE",0).back());};
    selectRecord(fixture());fillFields(false);
    require(positionalInputs>0&&namedKeys.size()==1&&namedKeys[0]=="+","a record without named fields shows only the add row");
    // Add three fields through the add row, one at a time, as a person would.
    for(const char* text:{"tint=1,0.5,0.5","p.code=1234","name=desk"}){
     SetWindowTextW(fieldInputs.back(),wide(text).c_str());apply(true);fillFields(false);}
    const auto withFields=doc.lines[size_t(record)];
    require(doc.format()==2,"adding a named field did not put FORMAT|2 at the top");
    require(withFields.ends_with("|tint=1,0.5,0.5|p.code=1234|name=desk"),"the add row did not write key=value fields in order: "+withFields);
    require(namedKeys.size()==4&&namedKeys[0]=="tint"&&namedKeys[3]=="+","the inspector does not list the named fields under the record's own");
    require(label(fieldLabels[positionalInputs]).find(L"Tint")!=std::wstring::npos,"the tint row is not labelled from the guide");
    report<<"PASS: the add row writes named fields, FORMAT|2 goes in, rows are labelled from the guide\n"<<std::flush;
    // Edit one, empty another (removed), and check the positional fields are untouched.
    SetWindowTextW(fieldInputs[positionalInputs],L"0.2,0.2,1");SetWindowTextW(fieldInputs[positionalInputs+1],L"");apply(true);
    require(doc.lines[size_t(record)].ends_with("|0|1|tint=0.2,0.2,1|name=desk"),"editing and emptying named rows went wrong: "+doc.lines[size_t(record)]);
    report<<"PASS: a named row edits in place; an emptied row is removed\n"<<std::flush;
    // A move, a turn and a copy keep the overrides; a copy drops the name.
    moveSelected(1,0);require(doc.lines[size_t(record)].find("|tint=0.2,0.2,1|name=desk")!=std::string::npos&&NativeDocument::fields(doc.lines[size_t(record)])[3]=="9","a move lost the named fields");
    rotateSelected();require(doc.lines[size_t(record)].find("|tint=0.2,0.2,1|name=desk")!=std::string::npos,"a turn lost the named fields");
    copySelected();const auto copy=doc.lines[size_t(record)];
    require(copy.find("tint=0.2,0.2,1")!=std::string::npos&&copy.find("name=")==std::string::npos,"a copy should keep the tint and drop the name: "+copy);
    report<<"PASS: move, turn and copy keep named fields; a copy leaves the name behind\n"<<std::flush;
    // Undo and redo step through the named edits exactly.
    const auto now=doc.text();SendMessageW(window,WM_COMMAND,Undo,0);const auto undone=doc.text();SendMessageW(window,WM_COMMAND,Redo,0);
    require(undone!=now&&doc.text()==now,"undo/redo did not restore the named edits exactly");
    report<<"PASS: undo and redo restore named edits exactly\n"<<std::flush;
    // FORMAT and a prefab block are no map's records: the index and the tools leave them.
    {const auto onTwo=doc.onMap(2);require(doc.records("FORMAT").empty()&&std::none_of(onTwo.begin(),onTwo.end(),[&](size_t i){return doc.lines[i].starts_with("FORMAT");}),"FORMAT|2 was indexed as a record of map 2");}
    // Make a prefab of the two desks and place it twice.
    const auto fixturesBefore=doc.records("FIXTURE",0).size();
    selectedRecords.clear();for(auto i:doc.records("FIXTURE",0))selectedRecords.push_back(int(i));record=selectedRecords.back();
    require(makePrefab(),"Make prefab refused two desks");
    require(doc.records("FIXTURE",0).size()==fixturesBefore-2&&doc.records("PREFAB_PLACE",0).size()==1,"Make prefab did not turn the desks into one PREFAB_PLACE");
    bool block=false;for(const auto& line:doc.lines)block|=line.starts_with("PREFAB|prefab_1");require(block,"no PREFAB block was written");
    for(auto i:doc.records("FIXTURE"))require(NativeDocument::fields(doc.lines[i])[1]!="@","a prefab's own line was indexed as a record");
    require(placePrefab()&&doc.records("PREFAB_PLACE",0).size()==2,"Place prefab did not add a second placement");
    const auto a=NativeDocument::fields(doc.lines[doc.records("PREFAB_PLACE",0)[0]]),b=NativeDocument::fields(doc.lines[doc.records("PREFAB_PLACE",0)[1]]);
    require(a.size()==8&&b.size()==8&&a[7]!=b[7],"two placements of a named prefab need different prefixes");
    rotateSelected();require(std::abs(number(NativeDocument::fields(doc.lines[size_t(record)])[6],0)-1.5708f)<.001f,"R did not turn the placement a quarter");
    require(validateDocument(),"the engine refused the document with named fields and two prefab placements");
    report<<"PASS: Make prefab writes a PREFAB block and a PREFAB_PLACE; Place prefab adds another with its own prefix; R turns it; the engine reads it\n"<<std::flush;
    // Undo takes Make prefab back in one step.
    SendMessageW(window,WM_COMMAND,Undo,0);SendMessageW(window,WM_COMMAND,Undo,0);SendMessageW(window,WM_COMMAND,Undo,0);
    require(doc.records("PREFAB_PLACE",0).empty()&&doc.records("FIXTURE",0).size()==fixturesBefore,"undoing Make prefab did not bring the desks back");
    SendMessageW(window,WM_COMMAND,Redo,0);SendMessageW(window,WM_COMMAND,Redo,0);SendMessageW(window,WM_COMMAND,Redo,0);
    // Save and reopen keep every byte.
    auto folder=session/L"format";fs::create_directories(folder);const auto file=folder/L"format-two.rmc";const auto before=doc.text();
    saveTo(file);NativeDocument reopened;reopened.open(file);
    require(reopened.text()==before,"save/reopen changed the text of a format 2 map");
    // An unknown key survives the inspector untouched.
    remember();doc.addOnLayer({"CLUTTER","0","0","5","5","-999","0","future.key=7"},0);publish();selectRecord(int(doc.records("CLUTTER",0).back()));fillFields(false);
    require(namedKeys.size()==2&&namedKeys[0]=="future.key",std::string("an unknown key is not listed"));
    SetWindowTextW(fieldInputs[0],L"1");apply(true);
    require(doc.lines[size_t(record)].ends_with("|future.key=7"),"editing a record lost an unknown key: "+doc.lines[size_t(record)]);
    require(validateDocument(),"the engine refused an unknown key it should keep");
    report<<"PASS: save/reopen is exact; an unknown key is listed, kept through edits, and accepted by the engine\n"<<std::flush;
  }
