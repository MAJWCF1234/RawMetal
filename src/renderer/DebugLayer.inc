// Desktop and builder overlay, composited after the scene and before menus.
if(const unsigned debug=(m_environmentInspection?0u:debuglayer::visible)|
                         (m_editorMapDiagnostics?debuglayer::MapDiagnostics:0u);
   debug&&!game.titleScreen()&&!game.loading()&&!m_vrRendering){
 using namespace debuglayer;
 const Player& viewer=m_view;
 const float ca=std::cos(viewer.angle),sa=std::sin(viewer.angle);
 const float cp=std::cos(viewer.pitch/140.f),sp=std::sin(viewer.pitch/140.f);
 const auto camera=[&](Point3 p){
  const float x=p.x-viewer.pos.x,y=p.y-viewer.pos.y,z=p.z-viewer.z-viewer.eye;
  const float forward=x*ca+y*sa;
  return Point3{-x*sa+y*ca,z*cp-forward*sp,forward*cp+z*sp};
 };
 const float focalX=Projection{float(sceneHeight)/std::max(1,sceneWidth)}.focal(float(m_width));
 const float focalY=focalX*(float(m_height)/std::max(1,m_width))*float(sceneWidth)/std::max(1,sceneHeight);
 const auto screen=[&](Point3 p){
  return Vec2{m_width*.5f+p.x*focalX/p.z,m_height*.5f-p.y*focalY/p.z};
 };
 const auto line=[&](Point3 a,Point3 b,std::uint32_t color){
  a=camera(a);b=camera(b);
  constexpr float nearDepth=Projection::nearPlane;
  if(a.z<nearDepth&&b.z<nearDepth)return;
  if(a.z<nearDepth)a=a+(b-a)*((nearDepth-a.z)/(b.z-a.z));
  if(b.z<nearDepth)b=b+(a-b)*((nearDepth-b.z)/(a.z-b.z));
  const Vec2 from=screen(a),to=screen(b),delta=to-from;
  if(!std::isfinite(from.x)||!std::isfinite(from.y)||!std::isfinite(to.x)||!std::isfinite(to.y))return;
  float enter=0.f,leave=1.f;
  const auto clip=[&](float start,float change,float max){
   if(std::fabs(change)<1e-6f)return start>=0&&start<=max;
   auto t0=-start/change,t1=(max-start)/change;
   if(t0>t1)std::swap(t0,t1);
   enter=std::max(enter,t0);leave=std::min(leave,t1);
   return enter<=leave;
  };
  if(!clip(from.x,delta.x,float(m_width-1))||!clip(from.y,delta.y,float(m_height-1)))return;
  const auto first=from+delta*enter,last=from+delta*leave,span=last-first;
  const int steps=std::max(1,int(std::ceil(std::max(std::fabs(span.x),std::fabs(span.y)))));
  for(int i=0;i<=steps;++i){
   const Vec2 pixel=first+span*(float(i)/steps);
   rect(int(std::lround(pixel.x)),int(std::lround(pixel.y)),1,1,color);
  }
 };
 const auto label=[&](Point3 at,const char* words,std::uint32_t color){
  auto view=camera(at);
  if(view.z<Projection::nearPlane)return;
  auto p=screen(view);
  if(p.x>=0&&p.x<m_width&&p.y>=0&&p.y<m_height)
   uiText(int(p.x)+4,int(p.y)-10,words,color,3,11);
 };
 const auto box=[&](Vec2 center,Vec2 xAxis,float halfX,float halfY,float z0,float z1,std::uint32_t color){
  if(halfX<=0||halfY<=0||z1<=z0)return;
  const Vec2 dx=xAxis*halfX,dy=Vec2{-xAxis.y,xAxis.x}*halfY;
  const Vec2 corners[]={center-dx-dy,center+dx-dy,center+dx+dy,center-dx+dy};
  for(int i=0;i<4;++i){
   const auto p=corners[i],q=corners[(i+1)%4];
   line({p.x,p.y,z0},{q.x,q.y,z0},color);
   line({p.x,p.y,z1},{q.x,q.y,z1},color);
   line({p.x,p.y,z0},{p.x,p.y,z1},color);
  }
 };
 const auto bounds=[&](Vec2 lo,Vec2 hi,float z0,float z1,std::uint32_t color){
  const Vec2 size=hi-lo;
  box((lo+hi)*.5f,{1,0},size.x*.5f,size.y*.5f,z0,z1,color);
 };
 const auto nearby=[&](Vec2 point,float radius=0.f){
  const float range=22.f+radius;
  return lengthSq(point-viewer.pos)<range*range;
 };
 constexpr std::uint32_t fixtureColor=0xff5ef59eu,doorColor=0xffffb565u;
 constexpr std::uint32_t lightColor=0xffffde68u,chunkColor=0xff99a6ffu,hazardColor=0xffff6aa0u;
 const Vec2 origin=game.chunkOffset(game.level());
 int resident=0;
 for(int level=0;level<game.chunkCount();++level){
  if(!game.chunkResident(level))continue;
  ++resident;
  const World& w=game.chunkWorld(level);
  const Vec2 offset=game.chunkOffset(level)-origin;
  if(!nearby(offset+Vec2{12,12},17.f))continue;
  if(debug&Chunks){
   const float z=w.definition().spawnHeight+.06f;
   const Point3 corners[]={{offset.x,offset.y,z},{offset.x+24,offset.y,z},
    {offset.x+24,offset.y+24,z},{offset.x,offset.y+24,z}};
   for(int i=0;i<4;++i)line(corners[i],corners[(i+1)%4],chunkColor);
   char id[32];std::snprintf(id,sizeof(id),"CHUNK %d",level);
   label({offset.x+12,offset.y+12,z+1.4f},id,chunkColor);
  }
  if(debug&Colliders){
   for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x){
    if(w.tile(x,y)!='#'||!nearby(offset+Vec2{x+.5f,y+.5f}))continue;
    bounds(offset+Vec2{float(x),float(y)},offset+Vec2{x+1.f,y+1.f},
     w.floorHeight(x+.5f,y+.5f),w.wallHeight(x,y),Stroke);
   }
   for(const auto& s:w.structures()){
    const Vec2 lo=offset+Vec2{s.x1,s.y1},hi=offset+Vec2{s.x2,s.y2};
    if(s.solid&&nearby((lo+hi)*.5f,length(hi-lo)*.5f))bounds(lo,hi,s.bottom,s.top,Stroke);
   }
   for(const auto& f:w.fixtures()){
    if(!f.solid||!nearby(f.position+offset,std::max(f.width,f.depth)*.5f))continue;
    const float z=w.floorHeight(f.position.x,f.position.y)+f.base;
    box(f.position+offset,{std::cos(f.yaw),-std::sin(f.yaw)},
     f.width*f.core*.5f,f.depth*f.core*.5f,z,z+f.height,fixtureColor);
   }
   for(const auto& d:w.doors()){
    const Vec2 center=d.centre();
    if(d.broken||!nearby(center+offset,d.width()))continue;
    const Vec2 axis=d.swinging?d.leafAxis(d.open):d.tangent();
    const Vec2 leaf=d.swinging?d.hinge()+axis*(d.width()*.5f):center;
    // The swinging capsule is shown by its rectangular envelope.
    const float z=w.floorHeight(center.x,center.y)+d.z+(d.swinging?0.f:d.open*2.65f);
    box(leaf+offset,axis,d.width()*.5f+(d.swinging?.15f:0.f),d.swinging?.15f:.13f,
     z+.015f,z+(d.swinging?2.35f:2.48f),doorColor);
   }
  }
  if(debug&Lights){
   for(size_t i=0;i<w.lights().size();++i){
    const auto& lamp=w.lights()[i];
    const Vec2 p=lamp.position+offset;
    if(!nearby(p))continue;
    const float power=game.lampPower(lamp);
    const auto color=power>0?lightColor:0xff8e825du;
    const Point3 at{p.x,p.y,lamp.z};
    line(at-Point3{.22f,0,0},at+Point3{.22f,0,0},color);
    line(at-Point3{0,.22f,0},at+Point3{0,.22f,0},color);
    line(at-Point3{0,0,.22f},at+Point3{0,0,.22f},color);
    char info[40];std::snprintf(info,sizeof(info),"L%zu  %.1fM%s",i,lamp.range,power>0?"":" OFF");
    label(at,info,color);
   }
  }
  if(debug&Hazards){
   for(const auto& h:w.hazards()){
    const Vec2 lo=offset+Vec2{h.x1,h.y1},hi=offset+Vec2{h.x2,h.y2};
    if(nearby((lo+hi)*.5f,length(hi-lo)*.5f))bounds(lo,hi,h.bottom,h.top,hazardColor);
   }
  }
 }
 if(debug&Navigation){
  const auto point=[](const ai::NavNode& node){
   return Point3{node.position.x,node.position.y,node.feet+.12f};
  };
  for(size_t i=0;i<m_debugNavigation.nodes.size();++i){
   const auto& node=m_debugNavigation.nodes[i];
   if(!nearby(node.position))continue;
   const Point3 at=point(node);
   for(int k=0;k<node.linkCount;++k){
    const auto& edge=m_debugNavigation.links[size_t(node.firstLink+k)];
    if(size_t(edge.dest)>i)line(at,point(m_debugNavigation.nodes[size_t(edge.dest)]),NavigationStroke);
   }
   const auto color=node.hint==ai::NodeHint::Cover?0xffbc9effu:NavigationStroke;
   line(at+Point3{-.13f,0,0},at+Point3{.13f,0,0},color);
   line(at+Point3{0,-.13f,0},at+Point3{0,.13f,0},color);
  }
 }
 if(debug&Issues){
  size_t withoutLocation=0;
  for(const auto& issue:m_mapIssues){
   if(!issue.located){++withoutLocation;continue;}
   if(!nearby(issue.position))continue;
   const Vec2 p=issue.position;
   constexpr float radius=.22f;
   box(p,{1.f,0.f},radius,radius,issue.z,issue.z+.65f,IssueStroke);
   line({p.x,p.y,issue.z+.65f},{p.x,p.y,issue.z+.88f},IssueStroke);
   const std::string description=issue.system+" / "+issue.object;
   label({p.x,p.y,issue.z+.88f},description.c_str(),IssueStroke);
  }
  const int top=(debug&Stats)?137:56;
  const int panelWidth=std::min(m_width-8,320);
  if(panelWidth>0){
   char summary[96];
   std::snprintf(summary,sizeof(summary),"MAP DIAGNOSTICS    %zu ISSUES",m_mapIssues.size());
   fill(4,top,panelWidth,withoutLocation?43:30,rgb(8,14,22),.9f);
   uiText(10,top+3,summary,m_mapIssues.empty()?Stroke:IssueStroke,3,11);
   if(debug&Navigation)uiText(10,top+16,m_debugNavigation.complete?
    "COLLISION / NAV LINKS / RED ISSUES":"COLLISION / BUILDING NAV / RED ISSUES",Stroke,3,11);
   if(withoutLocation){
    std::snprintf(summary,sizeof(summary),"%zu ISSUES WITHOUT A WORLD POSITION",withoutLocation);
    uiText(10,top+29,summary,IssueStroke,3,11);
   }
  }
 }
 if(debug&Stats){
  const auto& p=game.player();const auto& w=game.world();
  const int panelWidth=std::min(m_width-8,390);
  if(panelWidth>0){
   fill(4,56,panelWidth,75,rgb(8,14,22),.9f);
   uiText(10,60,"DEBUG LAYER",Stroke,3,12);
   char row[128];
   std::snprintf(row,sizeof(row),"MAP %d  CHUNKS %d/%d  %s",game.level(),resident,game.chunkCount(),m_gpu?"VULKAN":"CPU");
   uiText(10,74,row,Stroke,3,11);
   std::snprintf(row,sizeof(row),"XYZ %.2f %.2f %.2f  %s",p.pos.x,p.pos.y,p.z,p.prone?"PRONE":p.crouched?"CROUCH":"STAND");
   uiText(10,87,row,Stroke,3,11);
   if(m_gpu)std::snprintf(row,sizeof(row),"FRAME %.1f MS  SCENE %.1f  SUBMIT %.1f  PRESENT %.1f",m_frameMs,m_sceneMs,m_submitMs,m_presentMs);
   else std::snprintf(row,sizeof(row),"FRAME %.1f MS",m_frameMs);
   uiText(10,100,row,Stroke,3,11);
   std::snprintf(row,sizeof(row),"DOORS %zu  LIGHTS %zu  HAZARDS %zu  EVENTS %zu",
    w.doors().size(),w.lights().size(),w.hazards().size(),w.scriptEvents().size());
   uiText(10,113,row,Stroke,3,11);
  }
 }
}
