  bool dragWasDirty=false;long long dragChanges=0;
  void captureDragState(){dragDocument=doc.text();dragWasDirty=dirty;dragChanges=changeCount;}
  std::vector<int> selectedRecords;
  std::vector<std::pair<size_t,std::string>> dragSelection;
  int resizeHandle=-1;
  bool selectingArea=false,areaAdd=false;
  float areaX=0,areaY=0,areaEndX=0,areaEndY=0;
  bool selected(int index) const {return std::find(selectedRecords.begin(),selectedRecords.end(),index)!=selectedRecords.end();}
  void selectionPick(int index,bool extend) {
    if(!extend){selectRecord(index);return;}
    if(index<0||!activeFloorRecord(size_t(index)))return;
    auto found=std::find(selectedRecords.begin(),selectedRecords.end(),index);
    if(found!=selectedRecords.end()){
      selectedRecords.erase(found);int primary=selectedRecords.empty()?-1:selectedRecords.back();
      selectRecord(primary,true);
    }else{selectedRecords.push_back(index);selectRecord(index,true);}
  }
  std::vector<size_t> selectedNativeSet() const {
    std::set<size_t> result;std::set<int> people;
    auto add=[&](int i){if(i<0||size_t(i)>=doc.lines.size()||!handleFor(size_t(i)))return;
      result.insert(size_t(i));auto f=NativeDocument::fields(doc.lines[size_t(i)]);
      if(f[0].starts_with("ACTOR_")&&f.size()>2)people.insert(int(number(f[2],-1)));};
    if(selectedRecords.empty())add(record);else for(int i:selectedRecords)add(i);
    for(auto i:doc.onMap(map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()<3)continue;
      if((f[0]=="ACTOR"&&f.size()>7&&people.count(int(number(f[7],-1))))||
         (f[0].starts_with("ACTOR_")&&people.count(int(number(f[2],-1)))))result.insert(i);}
    return {result.begin(),result.end()};
  }
  void selectArea(float ax,float ay,float bx,float by,bool extend) {
    if(!extend)selectedRecords.clear();
    float l=std::min(ax,bx),r=std::max(ax,bx),t=std::min(ay,by),b=std::max(ay,by);
    for(auto i:objects())if(activeFloorRecord(i)){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      auto corners=nativeCorners(schemas[size_t(schema)].fields,f);float left=1e9f,right=-1e9f,top=1e9f,bottom=-1e9f;
      for(auto [x,y]:corners){left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
      if(!corners.empty()&&right>=l&&left<=r&&bottom>=t&&top<=b&&!selected(int(i)))selectedRecords.push_back(int(i));}
    selectRecord(selectedRecords.empty()?-1:selectedRecords.back(),true);
  }
  void captureSelectionDrag() {
    if(floorTransformSelection){captureFloorDrag();return;}
    dragSelection.clear();
    for(auto i:selectedNativeSet())if(int(i)!=record)dragSelection.emplace_back(i,doc.lines[i]);
  }
  int cornerAt(float x,float y) const {
    if(floorTransformSelection){if(effectiveFloorView(floorLayer(layer)).locked)return -1;auto b=tileBounds();std::vector<std::pair<float,float>> points{{float(b[0]),float(b[1])},{float(b[2]),float(b[1])},{float(b[2]),float(b[3])},{float(b[0]),float(b[3])}};
      float best=9.f/std::max(1.f,planView().cell);int hit=-1;for(size_t i=0;i<points.size();++i){float d=std::hypot(x-points[i].first,y-points[i].second);if(d<best){best=d;hit=int(i);}}return hit;}
    if(record<0||selectedRecords.size()>1||!activeFloorRecord(size_t(record)))return -1;
    auto f=NativeDocument::fields(doc.lines[size_t(record)]);int schema=schemaFor(f);if(schema<0)return -1;
    auto corners=nativeCorners(schemas[size_t(schema)].fields,f);
    float radius=9.f/std::max(1.f,planView().cell);int result=-1;float distance=radius;
    for(size_t i=0;i<corners.size();++i){auto copy=f;if(!resizeNative(copy,schemas[size_t(schema)].fields,int(i),corners[i].first,corners[i].second,0))continue;
      float d=std::hypot(x-corners[i].first,y-corners[i].second);if(d<=distance){distance=d;result=int(i);}}
    return result;
  }
  void applyCornerDrag(float x,float y) {
    if(floorTransformSelection){resizeFloorLayout(x,y);return;}
    auto f=objectFields;int schema=schemaFor(f);
    if(schema<0||!resizeNative(f,schemas[size_t(schema)].fields,resizeHandle,x,y,snap))return;
    doc.replace(size_t(record),f);objectMoved=f!=objectFields;updateInspector();
  }
  // A rejected group operation must preserve the Redo branch and the oldest
  // Undo entry even when the history is full. Keep only the displaced entry,
  // rather than copying every saved map on each nudge.
  template<class Operation> void editNativeSelection(Operation&& operation) {
    auto original=doc;auto previousFuture=std::move(future);size_t previousHistory=history.size();
    std::string displaced=previousHistory>=50?history.front():std::string{};
    int previousRecord=record,previousKind=kind;auto previousSelection=selectedRecords;
    bool previousDirty=dirty;auto previousChanges=changeCount;
    remember();
    try{operation();}
    catch(...){doc=std::move(original);if(!history.empty())history.pop_back();
      if(previousHistory>=50)history.insert(history.begin(),std::move(displaced));
      future=std::move(previousFuture);record=previousRecord;kind=previousKind;selectedRecords=std::move(previousSelection);
      dirty=previousDirty;changeCount=previousChanges;throw;}
  }
  void moveMultiple(float dx,float dy) {
    auto indices=selectedNativeSet();if(indices.empty()||(dx==0&&dy==0))return;
    editNativeSelection([&]{
      for(auto i:indices){if(!editableRecord(i))throw std::runtime_error("Unlock the selected floor before moving its content");
        auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);
        if(schema>=0&&translateNative(f,schemas[size_t(schema)].fields,dx,dy))doc.replace(i,f);}
      publish();selectRecord(record,true);
    });
  }
  void copyMultiple() {
    auto indices=selectedNativeSet();if(indices.empty())return;
    editNativeSelection([&]{for(auto i:indices)if(!editableRecord(i))throw std::runtime_error("Unlock selected floors before copying content");
      auto copied=cloneNativeSet(indices,snap>0?snap:.25f,snap>0?snap:.25f,0);
      selectedRecords.clear();for(auto i:copied)if(handleFor(i))selectedRecords.push_back(int(i));
      record=selectedRecords.empty()?-1:selectedRecords.back();publish();selectRecord(record,true);
    });
  }
  void removeMultiple() {
    auto indices=selectedNativeSet();if(indices.empty())return;
    editNativeSelection([&]{for(auto i:indices)if(!editableRecord(i))throw std::runtime_error("Unlock selected floors before deleting content");
      eraseNativeSet(indices);selectedRecords.clear();record=-1;publish();refreshRecords();
    });
  }
  void rotateMultiple() {
    auto indices=selectedNativeSet();if(indices.empty())return;
    float l=1e9f,r=-1e9f,t=1e9f,b=-1e9f;
    for(auto i:indices){auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      for(auto [x,y]:nativeCorners(schemas[size_t(schema)].fields,f)){l=std::min(l,x);r=std::max(r,x);t=std::min(t,y);b=std::max(b,y);}}
    if(l>r||t>b)return;float cx=(l+r)*.5f,cy=(t+b)*.5f;
    editNativeSelection([&]{for(auto i:indices){if(!editableRecord(i))throw std::runtime_error("Unlock selected floors before rotating content");
      auto f=NativeDocument::fields(doc.lines[i]);int schema=schemaFor(f);if(schema<0)continue;
      // A door turns with the group as a frame: in a wall that now runs the other way.
      if(auto door=nativeDoor(f)){auto turned=transformedDoor(*door,[&](float px,float py){return std::pair{cx+py-cy,cy-px+cx};});
        if(turned.alongY&&turned.seam)throw std::runtime_error("An entry or exit door stays in a wall that runs left to right. Leave it out of the group to turn the rest.");
        storeNativeDoor(f,turned);doc.replace(i,f);continue;}
      auto h=nativeHandle(schemas[size_t(schema)].fields,f);if(!h)continue;
      for(auto [x,y]:h->points){float px=number(f[x],0),py=number(f[y],0);f[x]=nativeNumber(cx+py-cy);f[y]=nativeNumber(cy-px+cx);}
      if(!nativeLineKind(f[0])&&h->points.size()==2){auto [ax,ay]=h->points[0];auto [bx,by]=h->points[1];if(number(f[ax],0)>number(f[bx],0))std::swap(f[ax],f[bx]);if(number(f[ay],0)>number(f[by],0))std::swap(f[ay],f[by]);}
      auto yaw=nativeField(schemas[size_t(schema)].fields,f,"yaw");if(yaw!=size_t(-1))f[yaw]=nativeNumber(std::remainder(number(f[yaw],0)+1.57079632679f,6.28318530718f));
      auto facing=nativeField(schemas[size_t(schema)].fields,f,"facing");if(facing!=size_t(-1))f[facing]=std::to_string((int(number(f[facing],0))+1)%4);
      auto along=nativeField(schemas[size_t(schema)].fields,f,"alongY"),ascending=nativeField(schemas[size_t(schema)].fields,f,"ascending");
      if(along!=size_t(-1)&&ascending!=size_t(-1)){int state=(f[along]=="1"?0:1)+(f[ascending]=="1"?0:2);state=(state+1)%4;f[along]=state&1?"0":"1";f[ascending]=state&2?"0":"1";}
      doc.replace(i,f);}
      publish();selectRecord(record,true);
    });
  }
  bool floorTransformSelection=false;
  std::array<int,4> floorDragBounds{};
  float resizePlane=0;
  std::array<int,4> tileBounds() const {
    std::array<int,4> b{24,24,0,0};bool found=false;
    for(int y=0;y<24;++y)for(int x=0;x<24;++x){char ch=doc.tile(map,layer,x,y);
      if(ch=='_'||(layer>0&&ch=='.'))continue;found=true;b[0]=std::min(b[0],x);b[1]=std::min(b[1],y);b[2]=std::max(b[2],x+1);b[3]=std::max(b[3],y+1);}
    return found?b:std::array<int,4>{0,0,24,24};
  }
  void selectFloorFootprint() {
    for(auto i:doc.records("LAYER",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>2&&int(number(f[2],-1))==layer){selectRecord(int(i));break;}}
    floorTransformSelection=true;tool=0;repaintTools();if(!session.empty())writeControl();
    message(L"Floor layout selected. Drag a corner to resize the floor and its architecture.");
  }
  void resizeFloorLayout(float x,float y) {
    NativeDocument original;original.loadText(dragDocument);auto old=floorDragBounds;
    auto b=old;bool left=resizeHandle==0||resizeHandle==3,top=resizeHandle<2;
    if(left)b[0]=std::clamp(int(std::round(x)),0,b[2]-1);else b[2]=std::clamp(int(std::round(x)),b[0]+1,24);
    if(top)b[1]=std::clamp(int(std::round(y)),0,b[3]-1);else b[3]=std::clamp(int(std::round(y)),b[1]+1,24);
    if(b==old){if(objectMoved)doc.loadText(dragDocument);objectMoved=false;return;}
    // Work from the press snapshot, so neither rounding nor repeated movement accumulates.
    std::array<std::string,24> rows;for(int yy=0;yy<24;++yy){rows[size_t(yy)]=std::string(24,'_');for(int xx=0;xx<24;++xx)rows[size_t(yy)][size_t(xx)]=original.tile(map,layer,xx,yy);}
    for(int yy=old[1];yy<old[3];++yy)for(int xx=old[0];xx<old[2];++xx)rows[size_t(yy)][size_t(xx)]='_';
    for(int yy=b[1];yy<b[3];++yy)for(int xx=b[0];xx<b[2];++xx){
      int sx=old[0]+int(std::round(float(xx-b[0])*(old[2]-old[0]-1)/std::max(1,b[2]-b[0]-1))),sy=old[1]+int(std::round(float(yy-b[1])*(old[3]-old[1]-1)/std::max(1,b[3]-b[1]-1)));
      rows[size_t(yy)][size_t(xx)]=original.tile(map,layer,sx,sy);
    }
    for(auto i:doc.records("ROW",map)){auto f=NativeDocument::fields(doc.lines[i]);if(f.size()>4&&int(number(f[2],-1))==layer){f[4]=rows[size_t(int(number(f[3],0)))];doc.replace(i,f);}}
    float sx=float(b[2]-b[0])/std::max(1,old[2]-old[0]),sy=float(b[3]-b[1])/std::max(1,old[3]-old[1]);
    for(const auto& member:dragSelection){auto f=NativeDocument::fields(member.second);
      if(f[0]!="STRUCT"&&f[0]!="STAIR"&&f[0]!="DOOR"&&f[0]!="WATER")continue;
      int schema=schemaFor(f);if(schema<0)continue;auto h=nativeHandle(schemas[size_t(schema)].fields,f);
      if(h){std::set<size_t> done;for(auto [xi,yi]:h->points){if(done.insert(xi).second)f[xi]=nativeNumber(b[0]+(number(f[xi],0)-old[0])*sx);if(done.insert(yi).second)f[yi]=nativeNumber(b[1]+(number(f[yi],0)-old[1])*sy);}}
      else if(f[0]=="DOOR"&&f.size()>4){f[2]=nativeNumber(b[0]+(number(f[2],0)-old[0])*sx);f[3]=nativeNumber(b[0]+(number(f[3],0)-old[0])*sx);f[4]=nativeNumber(b[1]+(number(f[4],0)-old[1])*sy);}
      doc.replace(member.first,f,true);
    }
    // Resample the managed room masks with the same cell mapping as the floor.
    for(auto i:doc.records("# EDITOR_DRESS",map)){auto fields=NativeDocument::fields(doc.lines[i]);if(fields.size()<5||int(number(fields[2],-1))!=layer)continue;
      for(auto j:original.records("# EDITOR_DRESS",map)){auto source=NativeDocument::fields(original.lines[j]);if(source.size()<5||source[2]!=fields[2]||source[3]!=fields[3])continue;
        auto cells=dressingCells(source[4]);std::set<int> wanted(cells.begin(),cells.end());std::vector<int> resized;
        for(int yy=b[1];yy<b[3];++yy)for(int xx=b[0];xx<b[2];++xx){int sx=old[0]+int(std::round(float(xx-b[0])*(old[2]-old[0]-1)/std::max(1,b[2]-b[0]-1))),sy=old[1]+int(std::round(float(yy-b[1])*(old[3]-old[1]-1)/std::max(1,b[3]-b[1]-1)));if(wanted.count(sy*24+sx))resized.push_back(yy*24+xx);}
        fields[4]=dressingMask(resized);doc.replace(i,fields,true);break;}}
    // Passage reservations follow the original floor cells on every move,
    // including any cells outside the resized rectangle that stay untouched.
    auto passage=layerPassageMask(original,layer);std::set<int> passageSource(passage.begin(),passage.end());std::vector<int> passageResized;
    for(int c:passage){int xx=c%24,yy=c/24;bool inOld=xx>=old[0]&&xx<old[2]&&yy>=old[1]&&yy<old[3],inNew=xx>=b[0]&&xx<b[2]&&yy>=b[1]&&yy<b[3];if(!inOld&&!inNew)passageResized.push_back(c);}
    for(int yy=b[1];yy<b[3];++yy)for(int xx=b[0];xx<b[2];++xx){int sourceX=old[0]+int(std::round(float(xx-b[0])*(old[2]-old[0]-1)/std::max(1,b[2]-b[0]-1))),sourceY=old[1]+int(std::round(float(yy-b[1])*(old[3]-old[1]-1)/std::max(1,b[3]-b[1]-1)));if(passageSource.count(sourceY*24+sourceX))passageResized.push_back(yy*24+xx);}
    storeLayerPassageMask(layer,passageResized);
    objectMoved=doc.text()!=dragDocument;updateInspector();
  }
  void captureFloorDrag() {
    floorDragBounds=tileBounds();dragSelection.clear();
    for(auto i:layerContent({layer}))dragSelection.emplace_back(i,doc.lines[i]);
  }
  bool beginFootprintDrag3D(float ox,float oy,float oz,float dx,float dy,float dz) {
    if(tool!=0||record<0||effectiveFloorView(floorLayer(layer)).locked||(!floorTransformSelection&&!activeFloorRecord(size_t(record))))return false;
    std::vector<std::pair<float,float>> corners;float z=layerElevation();
    if(floorTransformSelection){auto b=tileBounds();corners={{float(b[0]),float(b[1])},{float(b[2]),float(b[1])},{float(b[2]),float(b[3])},{float(b[0]),float(b[3])}};}
    else{if(selectedRecords.size()>1)return false;auto f=NativeDocument::fields(doc.lines[size_t(record)]);int schema=schemaFor(f);if(schema<0)return false;corners=nativeCorners(schemas[size_t(schema)].fields,f);
      if(auto box=creationBox(f))z=(box->bottom+box->top)*.5f;}
    float a=dx*dx+dy*dy+dz*dz,best=.024f;int corner=-1;
    for(size_t i=0;i<corners.size();++i){auto p=corners[i];float t=((p.first-ox)*dx+(p.second-oy)*dy+(z-oz)*dz)/a;if(t<=.05f)continue;
      float x=ox+t*dx-p.first,y=oy+t*dy-p.second,h=oz+t*dz-z,angle=std::sqrt(x*x+y*y+h*h)/(t*std::sqrt(a));
      if(angle<best){best=angle;corner=int(i);}}
    if(corner<0)return false;
    auto f=NativeDocument::fields(doc.lines[size_t(record)]);int schema=schemaFor(f);auto test=f;
    if(!floorTransformSelection&&(schema<0||!resizeNative(test,schemas[size_t(schema)].fields,corner,corners[size_t(corner)].first,corners[size_t(corner)].second,0)))return false;
    float fx,fy;if(!floorPoint(ox,oy,oz,dx,dy,dz,z,fx,fy))return false;
    resizeHandle=corner;resizePlane=z;viewGrabX=fx-corners[size_t(corner)].first;viewGrabY=fy-corners[size_t(corner)].second;
    objectOriginal=doc.lines[size_t(record)];objectFields=f;captureDragState();objectDrag=viewDrag=true;objectMoved=false;captureMates();return true;
  }  float bodyAnchorX=0,bodyAnchorY=0,bodyAnchorZ=0,bodyNx=1,bodyNy=0,bodyNz=0;
  bool bodyDragging=false;
  bool bodyRayPoint(float ox,float oy,float oz,float dx,float dy,float dz,float& x,float& y,float& z) const {
    float den=dx*bodyNx+dy*bodyNy+dz*bodyNz;if(std::abs(den)<.00001f)return false;
    float t=((bodyAnchorX-ox)*bodyNx+(bodyAnchorY-oy)*bodyNy+(bodyAnchorZ-oz)*bodyNz)/den;if(t<=0)return false;
    x=ox+t*dx;y=oy+t*dy;z=oz+t*dz;return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);
  }
  bool beginBodyDrag3D(float ox,float oy,float oz,float dx,float dy,float dz) {
    if(record<0)return false;auto box=creationBox(NativeDocument::fields(doc.lines[size_t(record)]));if(!box)return false;
    float t=0;if(!rayNativeBox(ox,oy,oz,dx,dy,dz,box->x,box->y,box->w,box->d,box->bottom,box->top,box->yaw,t))return false;
    float length=std::sqrt(dx*dx+dy*dy+dz*dz);if(length<.0001f)return false;
    bodyNx=dx/length;bodyNy=dy/length;bodyNz=dz/length;bodyAnchorX=ox+t*dx;bodyAnchorY=oy+t*dy;bodyAnchorZ=oz+t*dz;
    objectOriginal=doc.lines[size_t(record)];objectFields=NativeDocument::fields(objectOriginal);captureDragState();
    objectDrag=viewDrag=bodyDragging=true;objectMoved=false;resizeHandle=-1;heightOriginalCeiling=ceilingHeight();captureMates();return true;
  }
  void applyBodyElevation(float delta) {
    auto shift=[&](size_t at,const std::string& original){
      auto source=NativeDocument::fields(original);int schema=schemaFor(source);if(schema<0)return;
      auto elevated=source;shiftNativeElevation(elevated,schemas[size_t(schema)].fields,delta);
      auto current=NativeDocument::fields(doc.lines[at]);
      for(size_t i=0;i<std::min(source.size(),elevated.size());++i)if(source[i]!=elevated[i])current[i]=elevated[i];
      // Zero delta after a previous drag must restore the original elevations too.
      const auto& names=schemas[size_t(schema)].fields;
      for(const char* name:{"z","az","bz","z1","z2","bottom","top","base","bed","surface","minZ","maxZ","endZ"}){
        auto field=nativeField(names,source,name);if(field!=size_t(-1)&&field<current.size())current[field]=elevated[field];}
      doc.replace(at,current);
    };
    shift(size_t(record),objectOriginal);for(const auto& member:dragSelection)shift(member.first,member.second);for(const auto& member:dragMates)shift(member.first,member.second);
    objectMoved=doc.text()!=dragDocument;updateInspector();
  }
  void applyBodyDrag3D(float ox,float oy,float oz,float dx,float dy,float dz) {
    float x,y,z;if(!bodyRayPoint(ox,oy,oz,dx,dy,dz,x,y,z))return;
    auto snapDelta=[&](float value){return snap>0?std::round(value/snap)*snap:value;};
    float mx=snapDelta(x-bodyAnchorX),my=snapDelta(y-bodyAnchorY),mz=snapDelta(z-bodyAnchorZ);
    applyDragDelta(mx,my);applyBodyElevation(mz);
  }
