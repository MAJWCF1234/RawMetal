  HWND layerStack=nullptr,layerPreview=nullptr,layerName=nullptr,layerElevationInput=nullptr,layerThicknessInput=nullptr,layerOpacity=nullptr,layerGroupList=nullptr;
  int layerDraft=0,selectedLayerGroup=-1;bool opacityDragging=false;
  std::vector<int> layerTargets() const {
    std::vector<int> result;if(layerStack){int count=int(SendMessageW(layerStack,LB_GETSELCOUNT,0,0));std::vector<int> rows(size_t(std::max(0,count)));
      if(count>0)SendMessageW(layerStack,LB_GETSELITEMS,count,reinterpret_cast<LPARAM>(rows.data()));
      for(int row:rows){int id=int(SendMessageW(layerStack,LB_GETITEMDATA,row,0));
        if(id>=0)result.push_back(id);else if(id<=-2)for(const auto& a:floorLayers())if(a.group==-id-2)result.push_back(a.id);}}
    if(result.empty())result.push_back(layer);std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
  }
  void refreshLayerDock() {
    if(!layerStack)return;auto a=floorLayer(layer);showFloorReferences=floorReferenceEnabled();
    SendMessageW(layerStack,LB_RESETCONTENT,0,0);
    auto add=[&](const std::wstring& text,int id){auto row=SendMessageW(layerStack,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));SendMessageW(layerStack,LB_SETITEMDATA,row,id);if((selectedLayerGroup<0&&id==layer)||(selectedLayerGroup>=0&&id==-selectedLayerGroup-2))SendMessageW(layerStack,LB_SETSEL,TRUE,row);};
    auto floors=floorLayers();auto order=floorStackOrder();
    std::sort(floors.begin(),floors.end(),[&](const auto& a,const auto& b){return std::find(order.begin(),order.end(),a.id)>std::find(order.begin(),order.end(),b.id);});
    std::vector<std::pair<int,std::string>> groups;
    for(auto i:doc.records("# EDITOR_LAYER_GROUP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>3)groups.emplace_back(int(number(f[2],0)),decoded(f[3]));}
    auto showFloor=[&](const FloorLayer& f){add(std::wstring(f.visible?L"● ":L"○ ")+(f.locked?L"▣ ":L"")+wide(f.name)+L"  · "+wide(nativeNumber(f.elevation))+L" m",f.id);};
    for(const auto& g:groups){add(L"▾ "+wide(g.second),-g.first-2);for(auto it=floors.rbegin();it!=floors.rend();++it)if(it->group==g.first)showFloor(*it);}
    for(auto it=floors.rbegin();it!=floors.rend();++it)if(it->group<0||std::none_of(groups.begin(),groups.end(),[&](const auto& g){return g.first==it->group;}))showFloor(*it);
    SetWindowTextW(layerName,wide(a.name).c_str());SetWindowTextW(layerElevationInput,wide(nativeNumber(a.elevation)).c_str());SetWindowTextW(layerThicknessInput,wide(nativeNumber(a.thickness)).c_str());
    SendMessageW(layerOpacity,TBM_SETPOS,TRUE,int(a.opacity*100+.5f));
    SendMessageW(layerGroupList,CB_RESETCONTENT,0,0);
    auto row=SendMessageW(layerGroupList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"No group"));SendMessageW(layerGroupList,CB_SETITEMDATA,row,-1);int chosen=0;
    for(const auto& g:groups){row=SendMessageW(layerGroupList,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide(g.second).c_str()));SendMessageW(layerGroupList,CB_SETITEMDATA,row,g.first);if(a.group==g.first)chosen=int(row);}
    SendMessageW(layerGroupList,CB_SETCURSEL,chosen,0);layerDraft=0;
    if(selectedLayerGroup>=0){auto g=floorGroup(selectedLayerGroup);SetWindowTextW(layerName,wide(g.name).c_str());SendMessageW(layerOpacity,TBM_SETPOS,TRUE,int(g.opacity*100));}
    EnableWindow(layerElevationInput,selectedLayerGroup<0);EnableWindow(layerThicknessInput,selectedLayerGroup<0);
    SetWindowTextW(GetDlgItem(window,LayerCommit),selectedLayerGroup>=0?L"Rename group":L"Apply floor");InvalidateRect(layerPreview,nullptr,FALSE);
  }
  void drawLayerPreview(HDC dc,RECT r) const {
    auto paper=CreateSolidBrush(ui::Field);FillRect(dc,&r,paper);DeleteObject(paper);
    int cell=std::max(1,int(std::min((r.right-r.left-8)/24,(r.bottom-r.top-8)/24)));int ox=r.left+(r.right-r.left-cell*24)/2,oy=r.top+(r.bottom-r.top-cell*24)/2;
    auto a=effectiveFloorView(floorLayer(layer));
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){char tile=doc.tile(map,layer,x,y);if(tile=='_'||!a.visible||!(a.mask[size_t(y)]&(1u<<x)))continue;
      auto brush=CreateSolidBrush(tile=='#'?ui::Ink:tile=='='?ui::Sage:ui::SandEdge);RECT q{ox+x*cell,oy+y*cell,ox+(x+1)*cell,oy+(y+1)*cell};FillRect(dc,&q,brush);DeleteObject(brush);}
  }
  void layoutLayerDock(int x,int y,int width,int height) {
    auto caption=[&](int x0,int y0,int span,const std::wstring& text){captions.push_back({RECT{x0,y0,x0+span,y0+20},text});};
    int space=height-y;int listHeight=std::clamp(int((space-332)*.65f),60,220);int previewHeight=std::clamp(space-332-listHeight,36,104);
    caption(x,y,width,L"Floors / layers");y+=28;
    placeControl(GetDlgItem(window,LayerMenu),x,y,100,30,TRUE);
    placeControl(GetDlgItem(window,LayerVisible),x+106,y,98,30,TRUE);
    placeControl(GetDlgItem(window,LayerLock),x+210,y,width-210,30,TRUE);y+=36;
    placeControl(layerStack,x,y,width,listHeight,TRUE);y+=listHeight+6;
    placeControl(layerPreview,x,y,width,previewHeight,TRUE);y+=previewHeight+6;
    caption(x,y,width,L"Name");y+=20;placeControl(layerName,x,y,width,26,TRUE);y+=32;
    caption(x,y,152,L"Elevation (m)");caption(x+160,y,152,L"Thickness (m)");y+=20;
    placeControl(layerElevationInput,x,y,150,26,TRUE);placeControl(layerThicknessInput,x+160,y,152,26,TRUE);y+=32;
    caption(x,y,70,L"Group");placeControl(layerGroupList,x+76,y-2,width-76,240,TRUE);y+=32;
    caption(x,y,76,L"Opacity");placeControl(layerOpacity,x+76,y-4,width-76,28,TRUE);y+=32;
    placeControl(GetDlgItem(window,LayerReferences),x,y,width,28,TRUE);
    SetWindowTextW(GetDlgItem(window,LayerReferences),showFloorReferences?L"Other floors: outlines on":L"Other floors: hidden");y+=34;
    placeControl(GetDlgItem(window,LayerCommit),x,y,width,30,TRUE);y+=34;
    float clearance=creationCeiling()-layerElevation();caption(x,y,width,(clearance<1.6f?L"Too low: ":L"Headroom: ")+wide(nativeNumber(clearance))+L" m  ·  player 1.6 m");
    for(HWND c:{layerStack,layerPreview,layerName,layerElevationInput,layerThicknessInput,layerOpacity,layerGroupList})ShowWindow(c,SW_SHOW);
    for(int id:{LayerMenu,LayerVisible,LayerLock,LayerReferences,LayerCommit})ShowWindow(GetDlgItem(window,id),SW_SHOW);
  }
  void showLayerMenu() {
    HMENU menu=CreatePopupMenu(),stack=CreatePopupMenu(),mask=CreatePopupMenu(),alpha=CreatePopupMenu(),transform=CreatePopupMenu();
    auto item=[](HMENU m,int id,const wchar_t* text){AppendMenuW(m,MF_STRING,id,text);};
    item(menu,LayerNew,L"New Layer...    Ctrl+Shift+N");item(menu,LayerFromVisible,L"New from Visible");item(menu,LayerNewGroup,L"New Layer Group");
    item(menu,LayerDuplicate,L"Duplicate Layers    Ctrl+Shift+D");item(menu,LayerMerge,L"Merge Down");item(menu,LayerDelete,L"Delete Layers");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    item(stack,LayerRaise,L"Move up in stack");item(stack,LayerLower,L"Move down in stack");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(stack),L"Stack");
    item(mask,LayerMaskArea,L"From selection");item(mask,LayerMaskInvert,L"Invert mask");item(mask,LayerMaskClear,L"Remove mask");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(mask),L"Mask");
    item(alpha,LayerOpaque,L"100% opaque");item(alpha,LayerHalf,L"50%");item(alpha,LayerGhost,L"20% reference");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(alpha),L"Transparency");
    item(transform,LayerFootprint,L"Resize floor footprint");item(transform,LayerFlipX,L"Flip horizontally");item(transform,LayerFlipY,L"Flip vertically");item(transform,LayerHeight,L"Edit elevation / thickness");AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(transform),L"Transform");
    RECT r{};GetWindowRect(GetDlgItem(window,LayerMenu),&r);int action=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,r.left,r.bottom,0,window,nullptr));DestroyMenu(menu);
    if(action)handleLayerCommand(action,0);
  }
  void layerOpacityChanged(bool final) {
    float value=float(SendMessageW(layerOpacity,TBM_GETPOS,0,0))/100.f;
    auto targets=layerTargets();if(selectedLayerGroup>=0){auto g=floorGroup(selectedLayerGroup);if(!opacityDragging&&std::abs(g.opacity-value)<.0001f)return;if(!opacityDragging){remember();opacityDragging=true;}g.opacity=value;storeFloorGroup(g);publish(true,false);if(final)opacityDragging=false;return;}
    if(!opacityDragging&&std::all_of(targets.begin(),targets.end(),[&](int id){return std::abs(floorLayer(id).opacity-value)<.0001f;}))return;
    if(!opacityDragging){remember();opacityDragging=true;}
    for(int id:layerTargets()){auto a=floorLayer(id);a.opacity=value;storeLayerView(a);}
    publish(true,false);InvalidateRect(layerPreview,nullptr,FALSE);if(final)opacityDragging=false;
  }
  void handleLayerCommand(int id,int notification) {
    if(id==InspectorTab||id==LayerTab){layersDock=id==LayerTab;if(layersDock)refreshLayerDock();relayout();return;}
    if(id==LayerStack&&notification==LBN_SELCHANGE){int row=int(SendMessageW(layerStack,LB_GETCARETINDEX,0,0)),chosen=int(SendMessageW(layerStack,LB_GETITEMDATA,row,0));
      if(chosen<=-2){selectedLayerGroup=-chosen-2;auto g=floorGroup(selectedLayerGroup);SetWindowTextW(layerName,wide(g.name).c_str());SendMessageW(layerOpacity,TBM_SETPOS,TRUE,int(g.opacity*100));EnableWindow(layerElevationInput,FALSE);EnableWindow(layerThicknessInput,FALSE);SetWindowTextW(GetDlgItem(window,LayerCommit),L"Rename group");}
      if(chosen>=0){selectedLayerGroup=-1;EnableWindow(layerElevationInput,TRUE);EnableWindow(layerThicknessInput,TRUE);SetWindowTextW(GetDlgItem(window,LayerCommit),L"Apply floor");layer=chosen;selectRecord(-1);auto a=floorLayer(layer);SetWindowTextW(layerName,wide(a.name).c_str());SetWindowTextW(layerElevationInput,wide(nativeNumber(a.elevation)).c_str());SetWindowTextW(layerThicknessInput,wide(nativeNumber(a.thickness)).c_str());SendMessageW(layerOpacity,TBM_SETPOS,TRUE,int(a.opacity*100));writeControl();InvalidateRect(plan,nullptr,FALSE);InvalidateRect(layerPreview,nullptr,FALSE);}return;}
    if(id==LayerMenu){showLayerMenu();return;}
    if(id==LayerFootprint||(id==LayerPreview&&notification==STN_CLICKED)){selectFloorFootprint();return;}
    if(id==LayerName||id==LayerElevationField||id==LayerThicknessField||id==LayerOpacity||id==LayerPreview)return;
    if(id==LayerGroupField&&notification!=CBN_SELCHANGE)return;
    if(id==LayerNew||id==LayerNewGroup){
      layersDock=true;selectedLayerGroup=-1;refreshLayerDock();layerDraft=id==LayerNew?1:2;
      SetWindowTextW(layerName,id==LayerNew?L"Upper floor":L"Layer group");
      if(id==LayerNew){float z=3.5f;for(const auto& a:floorLayers())z=std::max(z,a.elevation+3.5f);SetWindowTextW(layerElevationInput,wide(nativeNumber(z)).c_str());SetWindowTextW(layerThicknessInput,L"0.25");}
      SetWindowTextW(GetDlgItem(window,LayerCommit),L"Create");relayout();SetFocus(layerName);SendMessageW(layerName,EM_SETSEL,0,-1);return;
    }
    if(id==LayerHeight){layersDock=true;refreshLayerDock();relayout();SetFocus(layerElevationInput);SendMessageW(layerElevationInput,EM_SETSEL,0,-1);return;}
    auto before=doc.text();auto previousFuture=future;auto historyCount=history.size();auto targets=layerTargets();remember();
    try{
      if(id==LayerReferences)storeFloorReferences(!floorReferenceEnabled());
      else if(id==LayerVisible||id==LayerLock||id==LayerOpaque||id==LayerHalf||id==LayerGhost){
        if(selectedLayerGroup>=0){auto g=floorGroup(selectedLayerGroup);if(id==LayerVisible)g.visible=!g.visible;else if(id==LayerLock)g.locked=!g.locked;else g.opacity=id==LayerOpaque?1.f:id==LayerHalf?.5f:.2f;storeFloorGroup(g);}
        else {auto first=floorLayer(targets.front());for(int target:targets){auto a=floorLayer(target);
          if(id==LayerVisible)a.visible=!first.visible;else if(id==LayerLock)a.locked=!first.locked;else a.opacity=id==LayerOpaque?1.f:id==LayerHalf?.5f:.2f;storeLayerView(a);}}
      }else if(id==LayerCommit){
        auto name=utf8(label(layerName));if(name.empty())name="Floor";
        if(layerDraft==0&&selectedLayerGroup>=0){auto g=floorGroup(selectedLayerGroup);g.name=name;storeFloorGroup(g);}
        else if(layerDraft==2){int group=0;for(auto i:doc.records("# EDITOR_LAYER_GROUP",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2)group=std::max(group,int(number(f[2],0))+1);}
          doc.add({"# EDITOR_LAYER_GROUP",std::to_string(map),std::to_string(group),encoded(name)});for(int target:targets){auto a=floorLayer(target);a.group=group;storeLayerView(a);}}
        else{float elevation=std::stof(utf8(label(layerElevationInput))),thickness=std::stof(utf8(label(layerThicknessInput)));
          if(!std::isfinite(elevation)||!std::isfinite(thickness)||thickness<0||thickness>3)throw std::runtime_error("Use a finite elevation and thickness between 0 and 3 m");
          if(layerDraft==1){createFloorLayer(name);selectedLayerGroup=-1;}
          auto a=floorLayer(layer);for(const auto& other:floorLayers())if(other.id!=layer&&std::abs(other.elevation-elevation)<.25f)throw std::runtime_error("Floors need distinct elevations");
          moveLayerHeight(layer,elevation);
          for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(int(number(f[2],-1))==layer){f[3]=encoded(name);f[5]=nativeNumber(thickness);doc.replace(i,f);}}}
      }else if(id==LayerDuplicate){duplicateLayerSet(targets,selectedLayerGroup>=0?floorGroup(selectedLayerGroup).name+" copy":"");selectedLayerGroup=-1;}
      else if(id==LayerFromVisible){std::vector<int> visible;for(const auto& a:floorLayers())if(effectiveFloorView(a).visible&&effectiveFloorView(a).opacity>0)visible.push_back(a.id);duplicateLayerSet(visible,"From visible");
      }else if(id==LayerDelete){deleteFloorLayers(targets);if(selectedLayerGroup>=0){auto all=doc.records("# EDITOR_LAYER_GROUP",map);for(auto it=all.rbegin();it!=all.rend();++it){auto f=NativeDocument::fields(doc.lines[*it]);if(f.size()>2&&int(number(f[2],-1))==selectedLayerGroup)doc.eraseRecord(*it);}selectedLayerGroup=-1;}}
      else if(id==LayerMerge){
        auto floors=floorLayers();auto at=std::find_if(floors.begin(),floors.end(),[&](const auto& a){return a.id==layer;});
        if(at==floors.begin()||at==floors.end())throw std::runtime_error("Select a floor with another floor beneath it");
        auto source=*at,dest=*(at-1);if(effectiveFloorView(source).locked||effectiveFloorView(dest).locked)throw std::runtime_error("Unlock both floors before merging them");
        auto content=layerContent({source.id});for(auto i:content){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema>=0&&shiftNativeElevation(f,schemas[size_t(schema)].fields,dest.elevation-source.elevation))doc.replace(i,f,true);doc.setRecordLayer(i,dest.id);}
        for(int y=0;y<24;++y)for(int x=0;x<24;++x){char ch=doc.tile(map,source.id,x,y);if(ch!='_')doc.paint(map,dest.id,x,y,ch);}
        auto passage=layerPassageMask(doc,dest.id),sourcePassage=layerPassageMask(doc,source.id);passage.insert(passage.end(),sourcePassage.begin(),sourcePassage.end());storeLayerPassageMask(dest.id,passage);
        deleteFloorLayers({source.id});layer=std::min(dest.id,int(floorLayers().size())-1);
      }else if(id==LayerRotate||id==LayerFlipX||id==LayerFlipY){for(int target:targets)transformFloor(target,id==LayerRotate?1:0,id==LayerFlipX?1:id==LayerFlipY?2:0);}
      else if(id==LayerMaskArea||id==LayerMaskInvert||id==LayerMaskClear){
        if(id==LayerMaskArea&&std::hypot(areaEndX-areaX,areaEndY-areaY)<.2f)throw std::runtime_error("Drag a selection box in the plan first");
        auto changeMask=[&](auto& a){for(int y=0;y<24;++y){
          if(id==LayerMaskClear)a.mask[size_t(y)]=0xffffffu;else if(id==LayerMaskInvert)a.mask[size_t(y)]=(~a.mask[size_t(y)])&0xffffffu;
          else{a.mask[size_t(y)]=0;for(int x=0;x<24;++x)if(x+.5f>=std::min(areaX,areaEndX)&&x+.5f<=std::max(areaX,areaEndX)&&y+.5f>=std::min(areaY,areaEndY)&&y+.5f<=std::max(areaY,areaEndY))a.mask[size_t(y)]|=1u<<x;}}
          };
        if(selectedLayerGroup>=0){auto g=floorGroup(selectedLayerGroup);changeMask(g);storeFloorGroup(g);}else for(int target:targets){auto a=floorLayer(target);changeMask(a);storeLayerView(a);}
      }else if(id==LayerGroupField){int row=int(SendMessageW(layerGroupList,CB_GETCURSEL,0,0)),group=int(SendMessageW(layerGroupList,CB_GETITEMDATA,row,0));for(int target:targets){auto a=floorLayer(target);a.group=group;storeLayerView(a);}}
      else if(id==LayerRaise||id==LayerLower){
        reorderFloorStack(layer,id==LayerRaise);
      }
      publish(true,false);refresh();refreshLayerDock();layersDock=true;relayout();message(L"Layer edit saved in the project. Ctrl+Z restores it.");
    }catch(...){doc.loadText(before);history.resize(historyCount);future=std::move(previousFuture);refreshLayerDock();throw;}
  }