  bool mapHeightSelection=false,verticalDragging=false;
  float wallPickX=12,wallPickY=12,verticalX=0,verticalY=0,verticalNx=1,verticalNy=0,verticalStart=0;
  int verticalKind=1;float heightOriginalCeiling=4.5f;bool wallEdgeAlongX=true;
  std::vector<retro::EditorGuide> selectionGuides() const {
    std::vector<retro::EditorGuide> out;if(tool!=0||record<0||size_t(record)>=doc.lines.size())return out;
    if(floorTransformSelection){auto b=tileBounds();float z=layerElevation();std::vector<std::pair<float,float>> p{{float(b[0]),float(b[1])},{float(b[2]),float(b[1])},{float(b[2]),float(b[3])},{float(b[0]),float(b[3])}};
      for(size_t i=0;i<4;++i){auto a=p[i],c=p[(i+1)%4];out.push_back({a.first,a.second,z,c.first,c.second,z,3});}return out;}
    auto f=NativeDocument::fields(doc.lines[size_t(record)]);int schema=schemaFor(f);if(schema<0)return out;
    if(f[0]=="MAP"&&mapHeightSelection){
      float top=ceilingHeight(),bottom=layerElevation();
      out.push_back({wallPickX-(wallEdgeAlongX?.5f:0),wallPickY-(wallEdgeAlongX?0:.5f),top,wallPickX+(wallEdgeAlongX?.5f:0),wallPickY+(wallEdgeAlongX?0:.5f),top,1});
      out.push_back({wallPickX,wallPickY,bottom,wallPickX,wallPickY,top,0});return out;
    }
    if(selectedRecords.size()>1)return out;
    auto corners=nativeCorners(schemas[size_t(schema)].fields,f);if(corners.empty())return out;
    auto box=creationBox(f);float bottom=box?box->bottom:layerElevation(),top=box?box->top:bottom+.1f;
    if(f[0]=="STRUCT"&&f.size()>7){bottom=number(f[6],bottom);top=number(f[7],top);}
    const auto& names=schemas[size_t(schema)].fields;
    bool upper=nativeField(names,f,"top")!=size_t(-1)||nativeField(names,f,"height")!=size_t(-1)||nativeField(names,f,"h")!=size_t(-1);
    bool lower=nativeField(names,f,"bottom")!=size_t(-1);
    size_t n=corners.size();
    for(size_t i=0;i<n;++i){auto a=corners[i],b=corners[(i+1)%n];
      out.push_back({a.first,a.second,top,b.first,b.second,top,upper?1:0});
      out.push_back({a.first,a.second,bottom,b.first,b.second,bottom,lower?2:0});
      out.push_back({a.first,a.second,bottom,a.first,a.second,top,0});
      auto copy=f;if(resizeNative(copy,names,int(i),a.first,a.second,0))out.push_back({a.first,a.second,(bottom+top)*.5f,a.first,a.second,(bottom+top)*.5f,3});
    }return out;
  }
  bool verticalRay(float ox,float oy,float oz,float dx,float dy,float dz,float& z) const {
    float den=dx*verticalNx+dy*verticalNy;if(std::abs(den)<.00001f)return false;
    float t=((verticalX-ox)*verticalNx+(verticalY-oy)*verticalNy)/den;
    if(t<=0)return false;z=oz+dz*t;return std::isfinite(z);
  }
  bool beginHeightDrag(float ox,float oy,float oz,float dx,float dy,float dz) {
    float closest=.026f;bool found=false;
    for(const auto& g:selectionGuides())if(g.kind==1||g.kind==2){
      float vx=g.bx-g.ax,vy=g.by-g.ay,vz=g.bz-g.az;
      float wx=ox-g.ax,wy=oy-g.ay,wz=oz-g.az;
      float a=dx*dx+dy*dy+dz*dz,b=dx*vx+dy*vy+dz*vz,c=vx*vx+vy*vy+vz*vz;
      float d=dx*wx+dy*wy+dz*wz,e=vx*wx+vy*wy+vz*wz,den=a*c-b*b;
      if(a<.0001f||c<.0001f)continue;
      float u=den>.00001f?std::clamp((a*e-b*d)/den,0.f,1.f):.5f,t=(b*u-d)/a;if(t<=.05f)continue;
      float x=g.ax+u*vx,y=g.ay+u*vy,z=g.az+u*vz;
      float ex=ox+t*dx-x,ey=oy+t*dy-y,ez=oz+t*dz-z;
      float screen=std::sqrt(ex*ex+ey*ey+ez*ez)/(t*std::sqrt(a));
      if(screen<closest){closest=screen;verticalX=x;verticalY=y;verticalKind=g.kind;found=true;}
    }
    if(!found)return false;
    float length=std::hypot(dx,dy);if(length<.0001f)return false;
    verticalNx=dx/length;verticalNy=dy/length;
    if(!verticalRay(ox,oy,oz,dx,dy,dz,verticalStart))return false;
    if(effectiveFloorView(floorLayer(layer)).locked){message(L"Unlock this floor before resizing it.");return false;}
    heightOriginalCeiling=ceilingHeight();verticalDragging=objectDrag=viewDrag=true;objectMoved=false;
    objectOriginal=doc.lines[size_t(record)];objectFields=NativeDocument::fields(objectOriginal);dragDocument=doc.text();lastDragPublish=GetTickCount64();
    return true;
  }
  void applyHeightDelta(float delta) {
    delta=snap>0?std::round(delta/snap)*snap:delta;
    if(std::abs(delta)<.000001f){if(objectMoved)doc.loadText(dragDocument);objectMoved=false;updateInspector();return;}
    auto f=objectFields;int schema=schemaFor(f);if(schema<0)return;
    const auto& names=schemas[size_t(schema)].fields;
    auto set=[&](size_t at,float value){f[at]=nativeNumber(value);};
    if(f[0]=="MAP"&&f.size()>9){set(9,std::max(layerElevation()+.25f,number(objectFields[9],4.5f)+delta));}
    else{
      auto bottom=nativeField(names,f,"bottom"),top=nativeField(names,f,"top");
      auto height=nativeField(names,f,"height");if(height==size_t(-1))height=nativeField(names,f,"h");
      if(verticalKind==2&&bottom!=size_t(-1)){
        float value=number(objectFields[bottom],0)+delta;
        // A thin roof is lifted as a slab: stretching its underside into its
        // top would silently remove the roof instead of making headroom.
        if(top!=size_t(-1)&&number(objectFields[top],0)-number(objectFields[bottom],0)<=.5f){set(bottom,value);set(top,number(objectFields[top],0)+number(f[bottom],0)-number(objectFields[bottom],0));}
        else set(bottom,std::min(value,top==size_t(-1)?value:number(f[top],0)-.05f));
      }else if(top!=size_t(-1))set(top,std::max(bottom==size_t(-1)?.05f:number(f[bottom],0)+.05f,number(objectFields[top],0)+delta));
      else if(height!=size_t(-1))set(height,std::max(.05f,number(objectFields[height],1)+delta));
      else return;
      if(top!=size_t(-1)&&number(f[top],0)>ceilingHeight()&&number(objectFields[top],0)>=heightOriginalCeiling-.5f){
        for(auto i:doc.records("MAP",map)){auto m=NativeDocument::fields(doc.lines[i]);m[9]=nativeNumber(std::max(heightOriginalCeiling,number(f[top],0)));doc.replace(i,m);}
      }
    }
    doc.replace(size_t(record),f);objectMoved=doc.text()!=dragDocument;updateInspector();
    message(L"Height: "+wide(f[0]=="MAP"?f[9]:nativeNumber(delta))+L" m. Drag upward to make space; Esc cancels.");
  }
  bool pickMapSurface(float ox,float oy,float oz,float dx,float dy,float dz) {
    float len=std::sqrt(dx*dx+dy*dy+dz*dz);if(len<.0001f)return false;
    dx/=len;dy/=len;dz/=len;
    for(float t=.03f;t<60;t+=.03f){float x=ox+dx*t,y=oy+dy*t,z=oz+dz*t;if(x<0||x>=24||y<0||y>=24)continue;
      if(z>=ceilingHeight()-.02f||(z>=layerElevation()&&doc.tile(map,layer,int(x),int(y))=='#')){
        auto maps=doc.records("MAP",map);if(maps.empty())return false;selectRecord(int(maps.front()));mapHeightSelection=true;
        wallPickX=x;wallPickY=y;wallEdgeAlongX=std::abs(dy)>=std::abs(dx);if(!session.empty())writeControl();
        message(L"Wall / roof selected. Drag the highlighted upper edge upward to raise the ceiling.");return true;
      }}return false;
  }  int viewHoverRecord=-1,viewHoverCell=-1;
  std::string lastHoverLine;std::uint64_t lastHoverVersion=0;int lastHoverMap=-1,lastHoverLayer=-1;
  void applyHoverRay(const std::string& text) {
    if((text==lastHoverLine&&lastHoverVersion==doc.version&&lastHoverMap==map&&lastHoverLayer==layer)||tool!=0||playing||objectDrag||brushing)return;lastHoverLine=text;lastHoverVersion=doc.version;lastHoverMap=map;lastHoverLayer=layer;
    std::istringstream in(text);std::string tag;float ox,oy,oz,dx,dy,dz;
    if(!(in>>tag>>ox>>oy>>oz>>dx>>dy>>dz)||tag!="HOVER"){if(viewHoverRecord>=0||viewHoverCell>=0){viewHoverRecord=viewHoverCell=-1;if(!session.empty())writeControl();}return;}
    int hit=selectAlongRay(ox,oy,oz,dx,dy,dz),cell=-1;
    if(hit<0){float length=std::sqrt(dx*dx+dy*dy+dz*dz);if(length>.0001f)for(float t=.03f;t<60;t+=.04f){
      float x=ox+dx*t/length,y=oy+dy*t/length,z=oz+dz*t/length;if(x<0||x>=24||y<0||y>=24)continue;
      if(z>=layerElevation()&&z<=ceilingHeight()&&doc.tile(map,layer,int(x),int(y))=='#'){cell=int(y)*24+int(x);break;}
      if(z>=ceilingHeight())break;}}
    if(hit==viewHoverRecord&&cell==viewHoverCell)return;viewHoverRecord=hit;viewHoverCell=cell;
    if(!session.empty())writeControl();
  }
  std::vector<retro::EditorGuide> hoverGuides() const {
    std::vector<retro::EditorGuide> out;if(tool!=0||playing||objectDrag)return out;
    std::vector<std::pair<float,float>> corners;float bottom=layerElevation(),top=ceilingHeight();
    if(viewHoverRecord>=0&&viewHoverRecord!=record&&size_t(viewHoverRecord)<doc.lines.size()){
      auto f=NativeDocument::fields(doc.lines[size_t(viewHoverRecord)]);int schema=schemaFor(f);if(schema<0)return out;
      corners=nativeCorners(schemas[size_t(schema)].fields,f);if(auto box=creationBox(f)){bottom=box->bottom;top=box->top;}
    }else if(viewHoverCell>=0){float x=float(viewHoverCell%24),y=float(viewHoverCell/24);corners={{x,y},{x+1,y},{x+1,y+1},{x,y+1}};}else return out;
    for(size_t i=0;i<corners.size();++i){auto a=corners[i],b=corners[(i+1)%corners.size()];
      out.push_back({a.first,a.second,top,b.first,b.second,top,4});out.push_back({a.first,a.second,bottom,b.first,b.second,bottom,4});out.push_back({a.first,a.second,bottom,a.first,a.second,top,4});}
    return out;
  }