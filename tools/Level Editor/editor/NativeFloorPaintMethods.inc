  int floorPaintMaterial=15;bool floorMaterialExplicit=false;
  bool nativeFloorSlab(const std::vector<std::string>& f) const {
    if(f.size()<10||f[0]!="STRUCT"||(f.size()>10&&!f[10].empty()&&number(f[10],1)==0)||(f.size()>11&&!f[11].empty()&&number(f[11],0)!=0))return false;float z=layerElevation(),b=number(f[6],-999),t=number(f[7],999);
    return t>=z-.03f&&t<=z+.06f&&t>b&&t-b<=.55f;
  }
  void clearNativeFloor(float x0,float y0,float x1,float y1) {
    float z=layerElevation();std::vector<creation::Box> cuts{{(x0+x1)*.5f,(y0+y1)*.5f,x1-x0,y1-y0,z-.6f,z+.08f}};
    auto all=doc.records("STRUCT",map);for(auto it=all.rbegin();it!=all.rend();++it)if(nativeFloorSlab(NativeDocument::fields(doc.lines[*it])))cutCreationStructure(*it,cuts);
  }
  void paintNativeFloor(float x0,float y0,float x1,float y1,int material) {
    if(x1<x0)std::swap(x0,x1);if(y1<y0)std::swap(y0,y1);x0=std::clamp(x0,0.f,24.f);x1=std::clamp(x1,0.f,24.f);y0=std::clamp(y0,0.f,24.f);y1=std::clamp(y1,0.f,24.f);
    if(x1-x0<.02f||y1-y0<.02f)return;clearNativeFloor(x0,y0,x1,y1);
    float z=layerElevation(),bottom=layer>0?z-std::max(.02f,floorLayer(layer).thickness):z,top=layer>0?z:z+.02f;
    doc.addOnLayer({"STRUCT",std::to_string(map),nativeNumber(x0),nativeNumber(y0),nativeNumber(x1),nativeNumber(y1),nativeNumber(bottom),nativeNumber(top),"0",std::to_string(material)},layer);
    // Full cells retain native deck/room semantics; a partial stripe remains
    // exactly the authored rectangle rather than growing a grating tile.
    for(int y=std::max(0,int(std::ceil(y0)));y<std::min(24,int(std::floor(y1)));++y)for(int x=std::max(0,int(std::ceil(x0)));x<std::min(24,int(std::floor(x1)));++x){char previous=doc.tile(map,layer,x,y);if(previous=='_'||previous=='.'||previous=='=')doc.paint(map,layer,x,y,layer>0?'=':'.');}
  }
  bool floorPaintTarget(float x,float y,float z,int surface) const {
    if(surface==4||surface==2)return false;if(surface==3)return true;
    size_t at=0,field=0;if(thingAt(x,y,z,at,field))return nativeFloorSlab(NativeDocument::fields(doc.lines[at]));
    return (z<-999||std::abs(z-layerElevation())<.1f)&&doc.tile(map,layer,cellOf(x),cellOf(y))!='#';
  }
