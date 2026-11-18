// Culling: faces buried where solids overlap are left out of the map's geometry (renderer/
// ShapeCulling.h). The tube draws its scene with culling, then without, and compares:
//
//   - triangles drawn by the architecture pass, both ways, and what the culling cost
//     (indexing the solids, clipping the faces, the whole pass);
//   - the pictures: a map with no shaped structures (openings, glass, footings) must come out
//     identical, pixel for pixel;
//   - with `culling-area X1 Y1 X2 Y2`, the geometry of every structure in that box, clipped the
//     way Scene3D clips it: no part kept lies inside another solid, no exterior part was lost,
//     and no two kept faces lie flush on each other (z-fighting).
//
// Fixture keys: culling-area X1 Y1 X2 Y2, culling-fewer 1 (culling must leave faces out),
// culling-shot LABEL (the pictures' name).
#include "lab/Lab.h"
#include "lab/LabAccess.h"
#include "lab/LabFixture.h"
#include "renderer/ShapeCulling.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace retro::lab {
namespace {
std::string fx(double v,int d=2){char b[32];std::snprintf(b,sizeof b,"%.*f",d,v);return b;}
struct Area {float x1=0,y1=0,x2=0,y2=0;bool set=false;};
Area areaOf(const Fixture& f){Area a;const auto t=f.value("culling-area");if(!t.empty()&&std::sscanf(t.c_str(),"%f %f %f %f",&a.x1,&a.y1,&a.x2,&a.y2)==4)a.set=true;return a;}

// The faces a structure is drawn with, for the geometry check: a box's six, a new shape's own.
// (The old curved shapes are clipped as Scene3D draws them; here they stand as solids only.)
void facesOf(const Structure& s,std::vector<cull::Polygon>& out){
 if(s.shape==ShapeBox){const Point3 a{s.x1,s.y1,s.bottom},b{s.x2,s.y2,s.top};
  const Point3 c[8]={{a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,b.y,a.z},{a.x,b.y,a.z},{a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{a.x,b.y,b.z}};
  constexpr int q[6][4]={{0,3,2,1},{4,5,6,7},{0,1,5,4},{2,3,7,6},{1,2,6,5},{3,0,4,7}};
  for(const auto& f:q)out.push_back({{c[f[0]]},{c[f[1]]},{c[f[2]]},{c[f[3]]}});return;}
 if(!cull::newShape(s.shape))return;
 std::vector<cull::ShapePiece> pieces;cull::shapePieces(s,pieces,true);
 for(const auto& p:pieces)for(const auto& f:p.faces){cull::Polygon poly;for(const auto& v:f.v)poly.push_back({v.p});out.push_back(poly);}}
bool insidePiece(const cull::ConvexPiece& p,Point3 q,float margin){for(const auto& pl:p.planes)if(cull::dotP(pl.n,q)-pl.d>-margin)return false;return true;}
bool insidePolygon(const cull::Polygon& poly,Point3 n,Point3 q){
 for(size_t i=0;i<poly.size();++i){const Point3 e=poly[(i+1)%poly.size()].p-poly[i].p;if(cull::dotP(cull::crossP(e,q-poly[i].p),n)<-1e-4f)return false;}return true;}
// The area two coplanar convex polygons share.
float sharedArea(const cull::Polygon& a,const cull::Polygon& b,Point3 n){
 cull::Polygon in=a,front,back;
 for(size_t i=0;i<b.size()&&in.size()>=3;++i){const Point3 e=b[(i+1)%b.size()].p-b[i].p;Point3 inward=cull::crossP(n,e);const float l=cull::lengthP(inward);if(l<1e-6f)continue;inward=inward*(1/l);
  cull::splitPolygon(in,inward,cull::dotP(inward,b[i].p),front,back);in=front;}
 return in.size()>=3?cull::polygonArea(in):0.f;}

struct Geometry {int faces=0,kept=0,inside=0,lost=0,fights=0;double before=0,after=0;std::string firstInside,firstLost,firstFight;};
Geometry checkGeometry(const World& w,const Area& area){
 Geometry g;const int base=World::Width*World::Height;
 // The boxes Scene3D starts from: wall tiles, then the solid boxes (Scene3D.cpp surfaceBoxes).
 std::vector<SurfaceBox> boxes;
 for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x)if(w.tile(x,y)=='#')boxes.push_back({{float(x),float(y),w.floorHeight(x+.5f,y+.5f)},{float(x+1),float(y+1),w.wallHeight(x,y)},y*World::Width+x});
 const auto& list=w.structures();
 for(size_t i=0;i<list.size();++i){const auto& s=list[i];
  if(s.rail||s.shape!=ShapeBox||w.isBreakableStructure(int(i))||s.material==1||s.material==6||s.material==9||seeThroughMaterial(s.material)||s.material==CollisionProxyMaterial||s.x2<=s.x1||s.y2<=s.y1||s.top<=s.bottom)continue;
  boxes.push_back({{s.x1,s.y1,s.bottom},{s.x2,s.y2,s.top},base+int(i)});}
 const auto index=cull::buildIndex(boxes,list,base,[&](size_t i){return !w.isBreakableStructure(int(i));});
 struct Kept {cull::Polygon poly;Point3 n;int owner;};std::vector<Kept> kept;std::vector<cull::Polygon> parts;
 for(size_t i=0;i<list.size();++i){const auto& s=list[i];const int owner=base+int(i);
  if(!index->hasOwner(owner)||s.x2<area.x1||s.x1>area.x2||s.y2<area.y1||s.y1>area.y2)continue;
  std::vector<cull::Polygon> faces;facesOf(s,faces);
  for(const auto& face:faces){++g.faces;g.before+=cull::polygonArea(face);const Point3 n=index->outward(face,owner);
   std::vector<cull::Polygon> visible;if(index->clip(face,owner,false,parts))visible=parts;else visible={face};
   for(const auto& part:visible){g.after+=cull::polygonArea(part);++g.kept;kept.push_back({part,n,owner});
    // Nothing kept lies inside another solid: its middle and corners (pulled 5 mm inward) are outside them all.
    Point3 mid{};for(const auto& v:part)mid=mid+v.p;mid=mid*(1.f/part.size());
    std::vector<Point3> probes{mid};for(const auto& v:part)probes.push_back(v.p+(mid-v.p)*.05f);
    bool in=false;for(auto q:probes)index->query(q,q,[&](const cull::ConvexPiece& p){if(p.owner!=owner&&insidePiece(p,q,2e-3f))in=true;});
    if(in&&g.inside++==0)g.firstInside="structure "+std::to_string(i)+" near "+fx(mid.x)+","+fx(mid.y)+","+fx(mid.z);}
   // Nothing that shows was lost: a point of the face no other solid covers lies in a part kept.
   Point3 lo=face[0].p,hi=face[0].p;for(const auto& v:face){lo={std::min(lo.x,v.p.x),std::min(lo.y,v.p.y),std::min(lo.z,v.p.z)};hi={std::max(hi.x,v.p.x),std::max(hi.y,v.p.y),std::max(hi.z,v.p.z)};}
   const Point3 a=face[0].p,e1=face[1].p-face[0].p,e2=face.back().p-face[0].p;
   for(int u=1;u<8;++u)for(int v=1;v<8;++v){const Point3 q=a+e1*(u/8.f)+e2*(v/8.f);if(!insidePolygon(face,n,q))continue;
    bool covered=false;index->query(q,q,[&](const cull::ConvexPiece& p){if(p.owner!=owner&&insidePiece(p,q,-2e-3f))covered=true;});
    if(covered)continue;bool shown=false;for(const auto& part:visible)shown|=insidePolygon(part,n,q);
    if(!shown&&g.lost++==0)g.firstLost="structure "+std::to_string(i)+" at "+fx(q.x)+","+fx(q.y)+","+fx(q.z);}}}
 // No two kept faces of different solids lie flush on each other.
 for(size_t a=0;a<kept.size();++a)for(size_t b=a+1;b<kept.size();++b){const auto& A=kept[a];const auto& B=kept[b];
  if(A.owner==B.owner||cull::dotP(A.n,B.n)<.999f||std::fabs(cull::dotP(A.n,B.poly[0].p-A.poly[0].p))>1e-3f)continue;
  if(const float shared=sharedArea(A.poly,B.poly,A.n);shared>1e-4f&&g.fights++==0)g.firstFight="structures "+std::to_string(A.owner-base)+" and "+std::to_string(B.owner-base)+": "+fx(shared,4)+" m2";}
 return g;}

std::map<std::string,Geometry>& notes(){static std::map<std::string,Geometry> n;return n;}
int shapedStructures(const World& w){int n=0;for(const auto& s:w.structures())n+=s.shape!=ShapeBox&&cull::solidStructure(s);return n;}
}
const bool cullingTube=add({
 .name="culling",.summary="buried faces left out where solids overlap: triangles and cost both ways, identical pictures without shapes, no hidden or z-fighting faces kept",
 .fixtures={"culling-*"},
 .setup=[](Scene& s){if(s.renderer)s.renderer->setFaceCulling(true);
  if(const Area a=areaOf(*s.fixture);a.set){const auto g=checkGeometry(s.g().world(),a);
   s.board["cull.faces"]=g.faces;s.board["cull.kept"]=g.kept;s.board["cull.before"]=g.before;s.board["cull.after"]=g.after;s.board["cull.inside"]=g.inside;s.board["cull.lost"]=g.lost;s.board["cull.fights"]=g.fights;
   s.board["cull.geometry"]=1;notes()[s.fixture->name]=g;}},
 // The camera held where the fixture puts it (a raised view would otherwise fall to the floor).
 .step=[](Scene& s,float){const auto* f=s.fixture;if(f->hasCamera)LabAccess::setCamera(s.g(),f->cameraPos,f->cameraZ,f->cameraYaw,f->cameraPitch);},
 // Three passes: culled (warming every cache), unculled, culled again; the last two are compared.
 .capture=[](Scene& s,const Frame&){
  if(!s.renderer)return;const auto label=s.fixture->value("culling-shot","culling");const int third=std::max(2,s.frames/3);
  if(s.frame==third)s.renderer->setFaceCulling(false);
  if(s.frame==2*third){const auto st=s.renderer->cullStats(&s.g().world());s.board["off.tris"]=double(st.triangles);s.board["off.pass"]=st.passMs;
   const auto all=s.renderer->cullStatsTotal();s.board["off.all.tris"]=double(all.triangles);s.board["off.all.pass"]=all.passMs;s.shoot(label+"-unculled");s.renderer->setFaceCulling(true);}
  if(s.last()){const auto st=s.renderer->cullStats(&s.g().world());s.board["on.tris"]=double(st.triangles);s.board["on.index"]=st.indexMs;s.board["on.clip"]=st.clipMs;s.board["on.pass"]=st.passMs;
   s.board["on.cut"]=st.facesCut;s.board["on.buried"]=st.facesBuried;s.board["on.pieces"]=st.pieces;s.shoot(label+"-culled");
   const auto all=s.renderer->cullStatsTotal();s.board["on.all.tris"]=double(all.triangles);s.board["on.all.pass"]=all.passMs;s.board["on.all.clip"]=all.clipMs;s.board["on.all.index"]=all.indexMs;s.board["on.all.pieces"]=all.pieces;
   const auto& off=s.shots[s.shots.size()-2].pixels;const auto& on=s.shots.back().pixels;long long diff=0;for(size_t i=0;i<std::min(on.size(),off.size());++i)diff+=on[i]!=off[i];
   s.board["diff"]=double(diff);s.board["shaped"]=shapedStructures(s.g().world());}},
 .metrics=[](Scene& s,Report& r){
  const double on=s.board["on.tris"],off=s.board["off.tris"];
  r.metric("triangles_culled",on);r.metric("triangles_unculled",off);r.metric("triangles_saved_pct",off>0?100*(off-on)/off:0,"%");
  r.metric("index_ms",s.board["on.index"],"ms");r.metric("clip_ms",s.board["on.clip"],"ms");r.metric("pass_ms_culled",s.board["on.pass"],"ms");r.metric("pass_ms_unculled",s.board["off.pass"],"ms");
  r.metric("all_maps_triangles_culled",s.board["on.all.tris"]);r.metric("all_maps_triangles_unculled",s.board["off.all.tris"]);r.metric("all_maps_solids",s.board["on.all.pieces"]);
  r.metric("all_maps_index_ms",s.board["on.all.index"],"ms");r.metric("all_maps_clip_ms",s.board["on.all.clip"],"ms");r.metric("all_maps_pass_ms_culled",s.board["on.all.pass"],"ms");r.metric("all_maps_pass_ms_unculled",s.board["off.all.pass"],"ms");
  r.metric("solids",s.board["on.pieces"]);r.metric("faces_cut",s.board["on.cut"]);r.metric("faces_buried",s.board["on.buried"]);r.metric("pixels_changed",s.board["diff"]);
  r.check("the architecture was rebuilt both ways",on>0&&off>0,fx(on,0)+" / "+fx(off,0)+" triangles");
  if(s.board["shaped"]==0){r.check("no shaped structures: the same triangles both ways, no face cut",on==off&&s.board["on.cut"]==0&&s.board["on.buried"]==0,fx(on,0)+" / "+fx(off,0));
}
  // Buried faces go; a face cut by another solid is drawn in more, smaller triangles than it had, so the
  // count can rise a little where seams are many: the check is on what is left out.
  if(s.fixture->value("culling-fewer")=="1")r.check("culling leaves buried faces out and cuts the ones that reach into solids",s.board["on.buried"]>0&&s.board["on.cut"]>0,fx(s.board["on.buried"],0)+" faces left out, "+fx(s.board["on.cut"],0)+" cut");
  if(s.board["cull.geometry"]==1){
   const Geometry* g=&notes()[s.fixture->name];
   r.metric("geometry_faces",s.board["cull.faces"]);r.metric("geometry_area_before_m2",s.board["cull.before"],"m2");r.metric("geometry_area_after_m2",s.board["cull.after"],"m2");
   r.check("no kept face lies inside another solid",s.board["cull.inside"]==0,fx(s.board["cull.inside"],0)+(g&&!g->firstInside.empty()?" first: "+g->firstInside:""));
   r.check("no face that shows was removed",s.board["cull.lost"]==0,fx(s.board["cull.lost"],0)+(g&&!g->firstLost.empty()?" first: "+g->firstLost:""));
   r.check("no two kept faces lie flush on each other (no z-fighting)",s.board["cull.fights"]==0,fx(s.board["cull.fights"],0)+(g&&!g->firstFight.empty()?" first: "+g->firstFight:""));
   r.check("the building draws less surface culled",s.board["cull.after"]<s.board["cull.before"]-.5,fx(s.board["cull.before"],1)+" -> "+fx(s.board["cull.after"],1)+" m2");}},
 .teardown=[](Scene& s){if(s.renderer)s.renderer->setFaceCulling(true);}});
}
