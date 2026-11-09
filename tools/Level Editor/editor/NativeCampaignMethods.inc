  bool joinMapPassage=true;
  // MAP origins are native world coordinates; map IDs never imply a direction.
  std::pair<float,float> adjacentOrigin(int edge) const {
    auto [x,y]=mapOrigin();return {x+(edge==3?24.f:edge==2?-24.f:0.f),y+(edge==1?24.f:edge==0?-24.f:0.f)};
  }
  bool mapEdgeAvailable(int edge) const {
    auto [x,y]=adjacentOrigin(edge);for(auto i:doc.records("MAP")){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<5)continue;
      if(std::abs(number(f[3],0)-x)<23.999f&&std::abs(number(f[4],0)-y)<23.999f)return false;}return true;
  }
  void addAdjacentMap(int edge) {
    if(edge<0||edge>3||!mapEdgeAvailable(edge))throw std::runtime_error("That edge is occupied by another map. Select it in the campaign list to continue building there.");
    int source=map,sourceLayer=layer;auto origin=adjacentOrigin(edge);float floor=bottomLayerElevation();
    auto fields=NativeDocument::fields(doc.lines[doc.records("MAP",source).front()]);auto original=doc;auto historyCount=history.size();auto priorFuture=future;remember();
    try{
      doc.addBlankMap("New map");int destination=int(doc.records("MAP").size())-1;
      auto index=doc.records("MAP",destination).front();auto added=NativeDocument::fields(doc.lines[index]);added[3]=nativeNumber(origin.first);added[4]=nativeNumber(origin.second);
      added[5]=added[6]="12.5";added[7]=nativeNumber(floor);
      for(size_t at:{size_t(8),size_t(9),size_t(10),size_t(11),size_t(18),size_t(19)})if(at<fields.size()&&at<added.size())added[at]=fields[at];
      added[9]=nativeNumber(std::max(floor+2.4f,number(added[9],4.5f)));
      int sourceFlag=12+edge,destinationFlag=12+(edge==0?1:edge==1?0:edge==2?3:2);
      if(joinMapPassage){fields[size_t(sourceFlag)]="1";added[size_t(destinationFlag)]="1";}doc.replace(index,added);
      doc.replace(doc.records("MAP",source).front(),fields);
      auto level=doc.records("LAYER",destination).front();auto layerFields=NativeDocument::fields(doc.lines[level]);layerFields[4]=nativeNumber(floor);doc.replace(level,layerFields);
      for(auto i:doc.records("LIGHT",destination)){auto f=NativeDocument::fields(doc.lines[i]);f[4]=nativeNumber(std::min(floor+3.2f,number(added[9],4.5f)-.2f));doc.replace(i,f);}
      // A short native floor opening joins the two edges. No translated scene
      // or special preview mesh is created; the game traverses these same rows.
      if(joinMapPassage)for(int across=11;across<=13;++across)for(int depth=0;depth<2;++depth){
        int sx=edge<2?across:edge==2?depth:23-depth,sy=edge>=2?across:edge==0?depth:23-depth;
        int tx=edge<2?across:edge==2?23-depth:depth,ty=edge>=2?across:edge==0?23-depth:depth;
        doc.paint(source,0,sx,sy,'.');doc.paint(destination,0,tx,ty,'.');}
      // Carry the surface set, with no floor ownership marker on this global record.
      for(auto i:doc.records("SURFACE",source)){auto f=NativeDocument::fields(doc.lines[i]);f[1]=std::to_string(destination);doc.add(f);}
      map=destination;layer=0;selectedLayerGroup=-1;selectedRecords.clear();record=-1;mapHeightSelection=floorTransformSelection=false;
      publish(true,false);refresh();writeControl();
      message(L"Map joined on the chosen edge. The 3D view follows the new map; Ctrl+Z restores the whole join.");
    }catch(...){doc=std::move(original);map=source;layer=sourceLayer;history.resize(historyCount);future=std::move(priorFuture);refresh();throw;}
  }
  void chooseMapEdge() {
    HMENU menu=CreatePopupMenu();const wchar_t* labels[]={L"North / top",L"South / bottom",L"West / left",L"East / right"};
    for(int edge=0;edge<4;++edge)AppendMenuW(menu,MF_STRING|(mapEdgeAvailable(edge)?0:MF_GRAYED),MapNorth+edge,labels[edge]);
    AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING|(joinMapPassage?MF_CHECKED:0),MapPassage,L"Open a passage between maps");
    RECT r{};GetWindowRect(GetDlgItem(window,AddMap),&r);
    for(;;){int chosen=int(TrackPopupMenu(menu,TPM_RETURNCMD|TPM_LEFTALIGN,r.left,r.bottom,0,window,nullptr));if(chosen==MapPassage){joinMapPassage=!joinMapPassage;CheckMenuItem(menu,MapPassage,MF_BYCOMMAND|(joinMapPassage?MF_CHECKED:MF_UNCHECKED));continue;}DestroyMenu(menu);if(chosen)addAdjacentMap(chosen-MapNorth);break;}
  }
