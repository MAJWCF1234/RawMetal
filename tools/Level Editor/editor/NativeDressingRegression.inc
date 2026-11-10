  void verifyDressingRegressions() {
    auto require=[](bool okay,const char* message){if(!okay)throw std::runtime_error(std::string("Dressing regression: ")+message);};
    auto reset=[&]{doc.newCampaign("Dressing regression");doc.addRoom("Room");map=layer=0;record=-1;selectedRecords.clear();history.clear();future.clear();autoDressingEnabled=false;autoDetail=false;richDetail=false;smartSnap=false;objectDrag=brushing=scrubbing=false;refresh();};
    std::ofstream report(root/L"native-editor-dressing-regression-test.txt");
    reset();
    for(auto i:doc.records("LAYER",0)){auto f=NativeDocument::fields(doc.lines[i]);f[4]="5";doc.replace(i,f);}
    for(auto i:doc.records("MAP",0)){auto f=NativeDocument::fields(doc.lines[i]);f[7]="5";f[9]="9.5";doc.replace(i,f);}
    for(auto i:doc.records("LIGHT",0)){auto f=NativeDocument::fields(doc.lines[i]);f[4]="9.3";doc.replace(i,f);}
    doc.addOnLayer({"DOOR","0","6","8","7","0","0","0","0","0","-1"},0);
    creation::Box approach{7,8,.5f,.5f,5,6};
    require(!creationFits(approach),"Furniture can block a door approach when the base layer is elevated");
    eraseNativeObject(doc,doc.records("DOOR",0).front());
    require(creationFits(approach),"Door approach fixture is blocked for a reason unrelated to the door");
    report<<"PASS: elevated-base doors reserve their actual world-space approach clearance\n"<<std::flush;

    reset();paintNativeFloor(10.2f,10,10.4f,11,18);
    auto stripe=doc.lines[doc.records("STRUCT",0).front()];std::vector<int> cell{10*24+10};
    addCreationFloor(cell,15);
    require(std::find(doc.lines.begin(),doc.lines.end(),stripe)!=doc.lines.end(),"Automatic floor changed the authored stripe");
    float area=0;int generated=0;std::vector<creation::Box> patches;
    for(auto i:doc.records("STRUCT",0)){auto f=NativeDocument::fields(doc.lines[i]);if(!nativeFloorSlab(f))continue;
      area+=(number(f[4],0)-number(f[2],0))*(number(f[5],0)-number(f[3],0));generated+=doc.automatic(i,0);
      patches.push_back({(number(f[2],0)+number(f[4],0))*.5f,(number(f[3],0)+number(f[5],0))*.5f,number(f[4],0)-number(f[2],0),number(f[5],0)-number(f[3],0),number(f[6],0),number(f[7],0)});
    }
    require(std::abs(area-1)<.0005f&&generated==2,"A partial painted stripe prevented the rest of its floor cell from being dressed");
    for(size_t a=0;a<patches.size();++a)for(size_t b=a+1;b<patches.size();++b)require(!creation::overlaps(patches[a],patches[b]),"Automatic floor overlaps an existing authored patch");
    auto finished=doc.text();addCreationFloor(cell,15);require(doc.text()==finished,"Repeating floor creation duplicates partial-cell pieces");
    require(validateDocument(),"Engine rejected the painted and automatically filled floor");
    auto folder=root/L".build"/L"dressing";fs::create_directories(folder);path=folder/L"partial-floor.rmc";saveTo(path);NativeDocument reopened;reopened.open(path);require(reopened.text()==doc.text(),"Partial floor did not save/reopen exactly");
    report<<"PASS: exact partial-cell automatic floor coverage preserves authored stripes without overlaps, survives repeat and native save/reopen\n"<<std::flush;

    reset();createFloorLayer();float z=layerElevation();
    std::vector<std::string> visual{"STRUCT","0","10","10","12","12",nativeNumber(z-.25f),nativeNumber(z),"0","15","0"};
    doc.addOnLayer(visual,layer);auto visualLine=doc.lines[doc.records("STRUCT",0).back()];
    auto shaped=visual;shaped[10]="1";shaped.push_back("7");shaped.push_back("0");doc.addOnLayer(shaped,layer);auto shapedLine=doc.lines[doc.records("STRUCT",0).back()];
    require(!nativeFloorSlab(visual)&&!nativeFloorSlab(shaped),"A non-solid or shaped decoration is classified as a full rectangular floor");
    auto support=nativeWalkableCells();require(!support[10*24+10],"Decorative architecture falsely supplies full-cell upstairs support");
    paintNativeFloor(10,10,11,11,15);
    require(std::find(doc.lines.begin(),doc.lines.end(),visualLine)!=doc.lines.end()&&std::find(doc.lines.begin(),doc.lines.end(),shapedLine)!=doc.lines.end(),"Floor painting cuts unrelated decorative geometry");
    require(validateDocument(),"Engine rejected native architecture after floor painting");
    report<<"PASS: decorative/non-solid native structures are not full-cell floors and survive floor painting\n";
  }
