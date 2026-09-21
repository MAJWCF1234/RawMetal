#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace retro {
bool SoftwareRenderer::validate3D(){
 if(!testNormalMapping())return false;
 {auto&lamp=m_facilityTextures.at("lamp_1_on");if(lamp.emission.size()!=lamp.pixels.size()||std::none_of(lamp.emission.begin(),lamp.emission.end(),[](auto p){return (p&255)>128;}))return false;
  Texture emissive{1,1,{0xff000000u}};emissive.emission={0xffffffffu};clear(0);std::fill(m_zbuffer.begin(),m_zbuffer.end(),9999.f);
  triangle3D({{-.3f,-.3f,1},0,0},{{.3f,-.3f,1},1,0},{{0,.3f,1},.5f,1},emissive,0);
  if((m_pixels[size_t((m_height/2)*m_width+m_width/2)]&0xffffffu)!=0xffffffu)return false;
 }
 // Standalone decals clamp at their edges, and distant detail averages instead of aliasing.
 if(m_hazard.mips.empty()||sample(m_hazard,1,1)!=m_hazard.pixels.back()||sample(m_hazard,-1,-1)!=m_hazard.pixels.front())return false;
 Texture checker{8,8,std::vector<std::uint32_t>(64)};for(int y=0;y<8;++y)for(int x=0;x<8;++x)checker.pixels[y*8+x]=(x+y)%2?0xffffffffu:0xff000000u;
 prepareDecal(checker);auto distant=sample(checker,.13f,.67f,3);if((distant&255)<126||(distant&255)>128)return false;
 for(const char*name:{"hand.R","hand.L"}){auto p=m_armsMesh.bonePosition(name);Point3 target=name[5]=='R'?Point3{-.025f,1.55f,.223f}:Point3{-.025f,1.60f,.49f};auto d=p-target;if(d.x*d.x+d.y*d.y+d.z*d.z>.0004f)return false;}
 if(m_armsMesh.bones<10||m_enemyMesh.triangles.size()<100||m_weaponMesh.triangles.size()<100)return false;
 const size_t junkTriangles[]={50,150,86,12,94,172};
 for(size_t i=0;i<6;++i)if(m_clutterMeshes[i].triangles.size()!=junkTriangles[i]||m_clutterTextures[i].pixels.empty())return false;
 for(int model=0;model<int(m_facilityMeshes.size());++model){auto&mesh=m_facilityMeshes[model];if(mesh.triangles.empty())return false;for(const auto&face:mesh.triangles)if(facilityTexture(model,face.part).pixels.empty())return false;}
 m_armsMesh.pose(0,0);auto before=m_armsMesh.triangles;m_armsMesh.pose(1,1);
 float delta=0;for(size_t i=0;i<before.size();++i)for(int j=0;j<3;++j){auto d=before[i].v[j].p-m_armsMesh.triangles[i].v[j].p;delta+=d.x*d.x+d.y*d.y+d.z*d.z;}
 if(delta<.0001f)return false;
 clear(0);std::fill(m_zbuffer.begin(),m_zbuffer.end(),9999.f);
 triangle3D({{-.3f,-.3f,1},0,0},{{.3f,-.3f,1},1,0},{{0,.3f,1},.5f,1},m_wall,1);
 auto index=size_t((m_height/2)*m_width+m_width/2);auto color=m_pixels[index];
 triangle3D({{-.6f,-.6f,2},0,0},{{.6f,-.6f,2},1,0},{{0,.6f,2},.5f,1},m_floor,1);
 if(std::fabs(m_zbuffer[index]-1)>1e-4f||m_pixels[index]!=color)return false;
 std::fill(m_zbuffer.begin(),m_zbuffer.end(),9999.f);
 triangle3D({{-.3f,-.1f,-.2f},0,0},{{.3f,-.1f,1},1,0},{{0,.3f,1},.5f,1},m_wall,1);
 size_t coverage=0;for(auto z:m_zbuffer){if(!std::isfinite(z)||z<.059f)return false;if(z<9999)++coverage;}
 return coverage>100;
}
void SoftwareRenderer::previewModel(int model,float angle){
 clear(0xff283038);std::fill(m_zbuffer.begin(),m_zbuffer.end(),9999.f);
 Mesh* meshes[]={&m_weaponMesh,&m_armsMesh,&m_enemyMesh,&m_waspMesh,&m_bruteMesh,&m_pumpMesh,&m_compressorMesh,&m_pipeMesh,&m_gateMesh,&m_clutterMeshes[0],&m_clutterMeshes[1],&m_clutterMeshes[2],&m_clutterMeshes[3],&m_clutterMeshes[4],&m_clutterMeshes[5]};
 Texture* textures[]={&m_weaponTexture,&m_arms,&m_enemyTexture,&m_waspTexture,&m_bruteTexture,&m_pumpTexture,&m_compressorTexture,&m_pipeTexture,&m_gateTexture,&m_clutterTextures[0],&m_clutterTextures[1],&m_clutterTextures[2],&m_clutterTextures[3],&m_clutterTextures[4],&m_clutterTextures[5]};
 auto&mesh=model>=15?m_facilityMeshes.at(model-15):*meshes[std::clamp(model,0,14)];auto&texture=*textures[std::clamp(model,0,14)];
 Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;
 float scale=2.0f/std::max({range.x,range.y,range.z});
 for(auto face:mesh.triangles){for(auto&v:face.v){auto p=(v.p-center)*scale;float x=p.x*std::cos(angle)+p.z*std::sin(angle),z=-p.x*std::sin(angle)+p.z*std::cos(angle);v.p={x,p.y*.94f-z*.34f,3.f+z*.94f+p.y*.34f};}triangle3D(face.v[0],face.v[1],face.v[2],model>=15?facilityTexture(model-15,face.part):texture,1.15f);}
}
static Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
Point3 SoftwareRenderer::cameraPoint(Point3 v,const Game& game)const{
 const auto&p=game.player();float yaw=p.angle,pitch=p.pitch/140.f;
 static thread_local float previousYaw=999,previousPitch=999,cy=1,sy=0,cp=1,sp=0;
 if(yaw!=previousYaw){cy=std::cos(yaw);sy=std::sin(yaw);previousYaw=yaw;}if(pitch!=previousPitch){cp=std::cos(pitch);sp=std::sin(pitch);previousPitch=pitch;}
 float x=v.x-p.pos.x,y=v.y-p.pos.y,z=v.z-(p.z+p.eye);
 float forward=x*cy+y*sy;
 return {-x*sy+y*cy,z*cp-forward*sp,forward*cp+z*sp};
}
void SoftwareRenderer::triangle3D(MeshVertex a,MeshVertex b,MeshVertex c,const Texture& texture,float light,const NormalLighting* normalLighting){
 bool normalActive=normalLighting&&!texture.normalLevels.empty();std::array<Point3,2> tangentLights{};float flatResponse=.65f;
 auto dot=[](Point3 p,Point3 q){return p.x*q.x+p.y*q.y+p.z*q.z;};
 auto unit=[&](Point3 v){return v*(1/std::sqrt(std::max(.000001f,dot(v,v))));};
 if(normalActive){
  auto e1=b.p-a.p,e2=c.p-a.p;float du1=b.u-a.u,dv1=b.v-a.v,du2=c.u-a.u,dv2=c.v-a.v,det=du1*dv2-du2*dv1;
  if(std::fabs(det)<.000001f)normalActive=false;
  else{
   auto tangent=unit((e1*dv2-e2*dv1)*(1/det)),bitangent=unit((e2*du1-e1*du2)*(1/det)),normal=unit(cross3(e1,e2));
   if(dot(normal,a.p)>0)normal=normal*-1;
   for(int i=0;i<2;++i){auto lightDirection=normalLighting->directions[i];tangentLights[i]={dot(tangent,lightDirection),dot(bitangent,lightDirection),dot(normal,lightDirection)};flatResponse+=normalLighting->weights[i]*std::max(0.f,tangentLights[i].z);}
  }
 }
 if(m_gpuFrame){m_gpu->submit(a,b,c,texture,light,tangentLights,normalActive?normalLighting->weights:std::array<float,2>{},flatResponse,normalActive,m_emissionScale);return;}
 // Clip in camera space before perspective division; preserve UVs at intersections.
 MeshVertex input[8]={a,b,c},output[8];int count=3;
 for(int plane=0;plane<5;++plane){
  auto distance=[&](MeshVertex v){switch(plane){case 0:return v.p.z-.06f;case 1:return v.p.z+v.p.x*1.3f;case 2:return v.p.z-v.p.x*1.3f;case 3:return v.p.z+v.p.y*2.2f;default:return v.p.z-v.p.y*2.2f;}};
  int next=0;
  for(int i=0;i<count;++i){auto v=input[i],w=input[(i+1)%count];float d=distance(v),e=distance(w);
   if(d>=0)output[next++]=v;
   if((d>=0)!=(e>=0)){float t=d/(d-e);output[next++]={v.p+(w.p-v.p)*t,v.u+(w.u-v.u)*t,v.v+(w.v-v.v)*t,v.light+(w.light-v.light)*t};}
  }
  count=next;if(count<3)return;std::copy(output,output+count,input);
 }
 struct P{float x,y,iz,u,v,light;};float focal=m_width*.65f;
 auto project=[&](MeshVertex v){float iz=1.f/v.p.z;return P{m_width*.5f+v.p.x*focal*iz,m_height*.5f-v.p.y*focal*iz,iz,v.u*iz,v.v*iz,v.light*iz};};
 for(int t=1;t<count-1;++t){P A=project(input[0]),B=project(input[t]),C=project(input[t+1]);
  float area=(B.x-A.x)*(C.y-A.y)-(B.y-A.y)*(C.x-A.x);if(std::fabs(area)<.001f)continue;
  float inverseArea=1.f/area;
  auto derivative=[&](float a,float b,float c){return Vec2{((b-a)*(C.y-A.y)-(c-a)*(B.y-A.y))*inverseArea,((c-a)*(B.x-A.x)-(b-a)*(C.x-A.x))*inverseArea};};
  auto depthDerivative=derivative(A.iz,B.iz,C.iz),uDerivative=derivative(A.u,B.u,C.u),vDerivative=derivative(A.v,B.v,C.v);
  float normalLod=0;
  if(normalActive){
   float zc=3.f/(A.iz+B.iz+C.iz),uc=(A.u+B.u+C.u)*zc/3,vc=(A.v+B.v+C.v)*zc/3;
   float ux=(uDerivative.x-uc*depthDerivative.x)*zc*texture.width,uy=(uDerivative.y-uc*depthDerivative.y)*zc*texture.width;
   float vx=(vDerivative.x-vc*depthDerivative.x)*zc*texture.height,vy=(vDerivative.y-vc*depthDerivative.y)*zc*texture.height;
   normalLod=std::max(0.f,.5f*std::log2(std::max({1.f,ux*ux+vx*vx,uy*uy+vy*vy})));
  }
  int left=std::max(0,int(std::floor(std::min({A.x,B.x,C.x})))),right=std::min(m_width-1,int(std::ceil(std::max({A.x,B.x,C.x}))));
  int top=std::max(0,int(std::floor(std::min({A.y,B.y,C.y})))),bottom=std::min(m_height-1,int(std::ceil(std::max({A.y,B.y,C.y}))));
  // Clip each scanline to the triangle before per-pixel depth/material work.
  // Conservative bounds retain the existing barycentric edge coverage rule.
  for(int y=top;y<=bottom;++y){float py=y+.5f,lo=float(right+1),hi=float(left-1);
   auto edge=[&](const P& p,const P& q){if(py<std::min(p.y,q.y)||py>std::max(p.y,q.y)||std::fabs(q.y-p.y)<.000001f)return;float x=p.x+(py-p.y)*(q.x-p.x)/(q.y-p.y);lo=std::min(lo,x);hi=std::max(hi,x);};
   edge(A,B);edge(B,C);edge(C,A);
   int rowLeft=std::max(left,int(std::floor(lo-.5f))-1),rowRight=std::min(right,int(std::ceil(hi-.5f))+1);
   for(int x=rowLeft;x<=rowRight;++x){float px=x+.5f;
   float u=((B.x-px)*(C.y-py)-(B.y-py)*(C.x-px))*inverseArea,v=((C.x-px)*(A.y-py)-(C.y-py)*(A.x-px))*inverseArea,w=1.f-u-v;
   if(u<0||v<0||w<0)continue;float iz=A.iz*u+B.iz*v+C.iz*w,z=1.f/iz;size_t index=size_t(y*m_width+x);
   if(z>=m_zbuffer[index])continue;
   float U=(A.u*u+B.u*v+C.u*w)*z,V=(A.v*u+B.v*v+C.v*w)*z,lod=0;
   if(normalActive)lod=normalLod;
   else if(!texture.mips.empty()){
    float ux=(uDerivative.x-U*depthDerivative.x)*z*texture.width,uy=(uDerivative.y-U*depthDerivative.y)*z*texture.width;
    float vx=(vDerivative.x-V*depthDerivative.x)*z*texture.height,vy=(vDerivative.y-V*depthDerivative.y)*z*texture.height;
    lod=std::max(0.f,.5f*std::log2(std::max({1.f,ux*ux+vx*vx,uy*uy+vy*vy})));
   }
   auto texel=sample(texture,U,V,lod);if((texel>>24)<128)continue;
   float vertexLight=(A.light*u+B.light*v+C.light*w)*z;
   if(normalActive){auto n=sampleNormal(texture,U,V,lod);float response=.65f;
    for(int i=0;i<2;++i)response+=normalLighting->weights[i]*std::max(0.f,dot(n,tangentLights[i]));
    vertexLight*=std::clamp(response/flatResponse,.6f,1.4f);
   }
   if(texture.additive){auto old=m_pixels[index];float alpha=float(texel>>24)/255.f;unsigned result=0xff000000u;
    for(int channel=0;channel<3;++channel){float tint=channel==0?.35f:channel==1?.72f:1.f;unsigned value=std::min(255u,unsigned((old>>(channel*8))&255)+unsigned(((texel>>(channel*8))&255)*alpha*light*tint));result|=value<<(channel*8);}put(x,y,result);
   }else{if(!texture.transparent)m_zbuffer[index]=z;auto color=shade(texel,light*vertexLight/(1.f+z*.018f));
    if(!texture.emission.empty()){
     int ex=std::min(texture.width-1,int((U-std::floor(U))*texture.width)),ey=std::min(texture.height-1,int((V-std::floor(V))*texture.height));auto glow=texture.emission[size_t(ey*texture.width+ex)];unsigned result=0xff000000u;
     for(int channel=0;channel<3;++channel){unsigned value=std::min(255u,unsigned((color>>(channel*8))&255)+unsigned(((glow>>(channel*8))&255)*1.6f*m_emissionScale));result|=value<<(channel*8);}color=result;
    }if(texture.transparent){auto old=m_pixels[index];float alpha=float(texel>>24)/255.f;unsigned result=0xff000000u;for(int channel=0;channel<3;++channel){int shift=channel*8;result|=unsigned(((color>>shift)&255)*alpha+((old>>shift)&255)*(1-alpha))<<shift;}color=result;}put(x,y,color);}
   }
  }
 }
}
void SoftwareRenderer::drawScene(const Game& game,bool clearDepth){
 if(clearDepth)std::fill(m_zbuffer.begin(),m_zbuffer.end(),std::numeric_limits<float>::infinity());
 const auto&w=game.world();
 auto&m_lightingCache=m_chunkLighting[w.level()];auto&m_lightingDoors=m_chunkLightingDoors[w.level()];
 // Refresh shadow caches at discrete door poses instead of rebuilding every frame.
 std::vector<float> doorState;for(auto&door:w.doors())doorState.push_back(std::floor(door.open*8.f)/8.f);
 if(doorState!=m_lightingDoors||m_lightingCache.size()>250000){m_lightingCache.clear();m_chunkNormalLighting[w.level()].clear();m_lightingDoors=doorState;}
 bool movingGeometry=false;
 int shadowBudget=m_shadowBudgetLimit;
 // Bin lights in 4 m cells; a stacked map must not scan every storey's lamps
 // for each surface sample. The radius test below remains the exact filter.
 auto&lightCells=m_chunkLightCells[w.level()];
 auto lightCell=[](Point3 p){int x=std::clamp(int(std::floor((p.x+4)/4)),0,7),y=std::clamp(int(std::floor((p.y+4)/4)),0,7),z=std::clamp(int(std::floor((p.z+12)/4)),0,7);return (z*8+y)*8+x;};
 if(lightCells.empty()||m_chunkLightCounts[w.level()]!=w.lights().size()){
 lightCells.assign(512,{});m_chunkLightCounts[w.level()]=w.lights().size();
 for(const auto& light:w.lights()){
  if(w.level()==3&&&light==&w.lights().back())continue;
  for(int z=0;z<8;++z)for(int y=0;y<8;++y)for(int x=0;x<8;++x){
   auto separation=[](float p,int cell,float origin){float lo=cell==0?-1000.f:origin+cell*4,hi=cell==7?1000.f:origin+(cell+1)*4;return std::max({lo-p,0.f,p-hi});};
   float dx=separation(light.position.x,x,-4),dy=separation(light.position.y,y,-4),dz=separation(light.z,z,-12);
   if(dx*dx+dy*dy+dz*dz<=30.01f)lightCells[(z*8+y)*8+x].push_back(size_t(&light-w.lights().data()));
  }
 }
 }
 // Merge authored deck runs into opaque rectangles. Only reject a complete
 // projected bounding box covered by one rectangle: shaft/stair openings stay
 // visible at every camera height, with no arbitrary floor-distance cutoff.
 std::vector<Structure> occluders;
 if(w.level()==3&&m_visibilityCulling)for(const auto&layer:w.layers())if(layer.thickness>0){
  size_t first=occluders.size();
  for(int y=0;y<24;++y)for(int x=0;x<24;){if(layer.rows[y][x]!='='){++x;continue;}int begin=x;while(x<24&&layer.rows[y][x]=='=')++x;
   bool joined=false;for(size_t i=first;i<occluders.size();++i){auto&o=occluders[i];if(o.x1==begin&&o.x2==x&&o.y2==y){o.y2=float(y+1);joined=true;break;}}
   if(!joined)occluders.push_back({float(begin),float(y),float(x),float(y+1),layer.elevation-layer.thickness,layer.elevation});
  }
 }
 Point3 eye{game.player().pos.x,game.player().pos.y,game.player().z+game.player().eye};
 auto hidden=[&](Point3 a,Point3 b){
  for(auto&o:occluders){float z=(o.bottom+o.top)*.5f;
   if(!((eye.z>o.top+.03f&&b.z<o.bottom-.03f)||(eye.z<o.bottom-.03f&&a.z>o.top+.03f)))continue;
   float t1=(z-eye.z)/(a.z-eye.z),t2=(z-eye.z)/(b.z-eye.z);
   float x1=eye.x+(a.x-eye.x)*t1,x2=eye.x+(b.x-eye.x)*t1,x3=eye.x+(a.x-eye.x)*t2,x4=eye.x+(b.x-eye.x)*t2;
   if(std::min({x1,x2,x3,x4})<o.x1+.005f||std::max({x1,x2,x3,x4})>o.x2-.005f)continue;
   float y1=eye.y+(a.y-eye.y)*t1,y2=eye.y+(b.y-eye.y)*t1,y3=eye.y+(a.y-eye.y)*t2,y4=eye.y+(b.y-eye.y)*t2;
   if(std::min({y1,y2,y3,y4})>=o.y1+.005f&&std::max({y1,y2,y3,y4})<=o.y2-.005f)return true;
  }return false;
 };
 auto outside=[](Point3 p){unsigned mask=0;if(p.z<.06f)mask|=1;if(p.z+p.x*1.3f<0)mask|=2;if(p.z-p.x*1.3f<0)mask|=4;if(p.z+p.y*2.2f<0)mask|=8;if(p.z-p.y*2.2f<0)mask|=16;return mask;};
 auto sphereVisible=[&](Point3 point,float radius){auto p=cameraPoint(point,game);return p.z+radius>.06f&&p.z+p.x*1.3f+radius*1.65f>0&&p.z-p.x*1.3f+radius*1.65f>0&&p.z+p.y*2.2f+radius*2.42f>0&&p.z-p.y*2.2f+radius*2.42f>0;};
 auto flashPitch=game.player().pitch/140.f,flashCp=std::cos(flashPitch),flashSp=std::sin(flashPitch);
 Point3 flashForward{std::cos(game.player().angle)*flashCp,std::sin(game.player().angle)*flashCp,flashSp};
 bool flashlightEnabled=game.flashlightOn();int flashlightRayBudget=4096;
 auto flashlightContribution=[&](Point3 point,Point3 normal){
  if(!flashlightEnabled)return 0.f;Point3 delta=point-eye;float d2=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;if(d2<.04f||d2>196.f)return 0.f;
  float distance=std::sqrt(d2),along=(delta.x*flashForward.x+delta.y*flashForward.y+delta.z*flashForward.z)/distance;if(along<=.80f)return 0.f;
  float cone=std::clamp((along-.80f)/.16f,0.f,1.f);cone=cone*cone*(3.f-2.f*cone);
  float nl=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z),facing=.65f;if(nl>.00001f)facing=.30f+.70f*std::fabs((normal.x*delta.x+normal.y*delta.y+normal.z*delta.z)/(nl*distance));
  // Never treat an untested receiver as visible when the ray budget runs out.
  if(flashlightRayBudget<=0)return 0.f;
  if(flashlightRayBudget>0){--flashlightRayBudget;float startT=0.f;
   if(eye.x<.01f&&delta.x>0)startT=std::max(startT,(.01f-eye.x)/delta.x);if(eye.x>23.99f&&delta.x<0)startT=std::max(startT,(23.99f-eye.x)/delta.x);
   if(eye.y<.01f&&delta.y>0)startT=std::max(startT,(.01f-eye.y)/delta.y);if(eye.y>23.99f&&delta.y<0)startT=std::max(startT,(23.99f-eye.y)/delta.y);
   startT=std::clamp(startT,0.f,.98f);auto start=eye+delta*startT;
   if(!w.rayClear({start.x,start.y},start.z,{point.x,point.y},point.z,true,false))return 0.f;
  }
  return cone*facing*1.9f/(1.f+d2*.020f);
 };
 auto illumination=[&](Point3 point,Point3 normal){
  float normalLength=std::sqrt(normal.x*normal.x+normal.y*normal.y+normal.z*normal.z);if(normalLength<.00001f)return .7f;normal=normal*(1/normalLength);
  if(movingGeometry){float base=(.82f+.12f*std::fabs(normal.z))*(.35f+.65f*w.liftLampPower());return std::clamp(base+flashlightContribution(point,normal),.24f,2.4f);}
  // Static fixture lighting is cached; the player-mounted flashlight is added
  // afterward because its cone moves every frame with yaw and pitch.
  auto positionBits=[](float value){return std::uint64_t(std::clamp(int(std::round(value*64))+2048,0,4095));};
  auto normalBits=[](float value){return std::uint64_t(std::clamp(int(std::round(value*15))+15,0,30));};
  auto key=positionBits(point.x)|(positionBits(point.y)<<12)|(positionBits(point.z)<<24)|(normalBits(normal.x)<<36)|(normalBits(normal.y)<<41)|(normalBits(normal.z)<<46);
  float brightness=0;auto cached=m_lightingCache.find(key);
  if(cached!=m_lightingCache.end())brightness=cached->second;
  else{
   brightness=.27f+.07f*std::fabs(normal.z);
   for(auto source:lightCells[lightCell(point)]){const auto&fixture=w.lights()[source];float x=fixture.position.x,y=fixture.position.y;
    if(std::fabs(x-point.x)>5.5f||std::fabs(y-point.y)>5.5f)continue;
    Point3 light{x,y,fixture.z},delta=light-point;float d2=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;if(d2>30||d2<.001f)continue;
    float distance=std::sqrt(d2),facing=std::fabs(normal.x*delta.x+normal.y*delta.y+normal.z*delta.z)/distance;
    float side=normal.x*delta.x+normal.y*delta.y+normal.z*delta.z>=0?1.f:-1.f;auto origin=point+normal*(side*.025f)+delta*(.025f/distance);
    float visibility=0;bool complete=true;
    for(float offset:{-.18f,0.f,.18f}){auto target=light+Point3{offset,0,0};auto ray=target-origin;int steps=std::max(1,int(std::ceil(distance/.18f)));bool blocked=false;
     for(int i=1;i<steps;++i){if(shadowBudget<=0){complete=false;break;}--shadowBudget;auto p=origin+ray*(float(i)/steps);if(!w.fits(p.x,p.y,p.z,.01f,false)||w.doorBlocks(p.x,p.y,p.z,.01f)){blocked=true;break;}}
     visibility+=blocked?.04f:1.f;
    }
    brightness+=(visibility/3.f)*(.12f+.88f*facing)*3.2f/(1+d2*.65f);if(!complete)break;
   }
   brightness=std::sqrt(std::clamp(brightness,.24f,1.4f));if(shadowBudget>0)m_lightingCache.emplace(key,brightness);
  }
  return std::clamp(brightness+flashlightContribution(point,normal),.24f,2.4f);
 };
 auto normalLightingAt=[&](Point3 center){
  auto bits=[](float value){return uint64_t(std::clamp(int(std::round(value*64))+2048,0,4095));};
  uint64_t key=bits(center.x)|(bits(center.y)<<12)|(bits(center.z)<<24);auto&cache=m_chunkNormalLighting[w.level()];
  auto found=cache.find(key);
  if(found==cache.end()){
   NormalLighting lights;
   for(auto source:lightCells[lightCell(center)]){const auto&fixture=w.lights()[source];
    auto delta=Point3{fixture.position.x,fixture.position.y,fixture.z}-center;float d2=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;
    if(d2>30||d2<.001f)continue;float weight=2.8f/(1+d2*.65f);
    auto start=center+delta*(.04f/std::sqrt(d2));
    if(shadowBudget>0){shadowBudget-=int(std::sqrt(d2)/.12f)+1;if(!w.rayClear({start.x,start.y},start.z,fixture.position,fixture.z,true,false))weight*=.04f;}
    if(weight<=lights.weights[1])continue;
    int slot=weight>lights.weights[0]?0:1;if(slot==0){lights.weights[1]=lights.weights[0];lights.directions[1]=lights.directions[0];}
    lights.weights[slot]=weight;lights.directions[slot]=delta*(1/std::sqrt(d2));
   }
   if(shadowBudget<=0){auto origin=cameraPoint(center,game);for(auto&direction:lights.directions)direction=cameraPoint(center+direction,game)-origin;return lights;}
   found=cache.emplace(key,lights).first;
  }
  auto result=found->second;auto origin=cameraPoint(center,game);
  for(auto&direction:result.directions)direction=cameraPoint(center+direction,game)-origin;
  return result;
 };
 bool objectLighting=false;float objectLight=1;
 auto tri=[&](MeshVertex a,MeshVertex b,MeshVertex c,const Texture&t,float light){
  if(std::max({a.p.z,b.p.z,c.p.z})<game.dormantBelow())return;
  auto A=cameraPoint(a.p,game),B=cameraPoint(b.p,game),C=cameraPoint(c.p,game);if(outside(A)&outside(B)&outside(C))return;
  if(objectLighting)light*=objectLight;
  else if(light<1.5f){auto normal=cross3(b.p-a.p,c.p-a.p);a.light=illumination(a.p,normal);b.light=illumination(b.p,normal);c.light=illumination(c.p,normal);}
  a.p=A;b.p=B;c.p=C;triangle3D(a,b,c,t,light);
 };
 auto quad=[&](Point3 a,Point3 b,Point3 c,Point3 d,const Texture&t,float light,Vec2 uvScale=Vec2{1,1},Vec2 uvOffset=Vec2{}){
  if(std::max({a.z,b.z,c.z,d.z})<game.dormantBelow())return;
  if(outside(cameraPoint(a,game))&outside(cameraPoint(b,game))&outside(cameraPoint(c,game))&outside(cameraPoint(d,game)))return;
  if(hidden({std::min({a.x,b.x,c.x,d.x}),std::min({a.y,b.y,c.y,d.y}),std::min({a.z,b.z,c.z,d.z})},{std::max({a.x,b.x,c.x,d.x}),std::max({a.y,b.y,c.y,d.y}),std::max({a.z,b.z,c.z,d.z})}))return;
  auto size=[](Point3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);};
  // Sample large architectural surfaces on a regular grid so light and shadow
  // gradients do not expose the two triangles of an entire wall or door.
  int columns=1,rows=1;
  if(light<1.5f&&(t.mips.empty()||!t.normalLevels.empty())){columns=std::clamp(int(std::ceil(size(b-a))),1,16);rows=std::clamp(int(std::ceil(size(d-a))),1,16);}
  auto normal=cross3(b-a,c-a);std::array<MeshVertex,289> vertices;
  for(int y=0;y<=rows;++y)for(int x=0;x<=columns;++x){float u=float(x)/columns,v=float(y)/rows;auto p=a+(b-a)*u+(d-a)*v;
   vertices[size_t(y*(columns+1)+x)]={cameraPoint(p,game),u*uvScale.x+uvOffset.x,(1-v)*uvScale.y+uvOffset.y,-1.f};
  }
  for(int y=0;y<rows;++y)for(int x=0;x<columns;++x){int i=y*(columns+1)+x;auto &A=vertices[i],&B=vertices[i+1],&C=vertices[i+columns+2],&D=vertices[i+columns+1];
   if(outside(A.p)&outside(B.p)&outside(C.p)&outside(D.p))continue;
   auto illuminate=[&](MeshVertex&v,int ix,int iy){if(v.light<0)v.light=light<1.5f?illumination(a+(b-a)*(float(ix)/columns)+(d-a)*(float(iy)/rows),normal):1.f;};
   illuminate(A,x,y);illuminate(B,x+1,y);illuminate(C,x+1,y+1);illuminate(D,x,y+1);
   NormalLighting lights;const NormalLighting* normalState=nullptr;
   if(!t.normalLevels.empty()&&!movingGeometry){lights=normalLightingAt(a+(b-a)*((x+.5f)/columns)+(d-a)*((y+.5f)/rows));normalState=&lights;}
   triangle3D(A,B,C,t,light,normalState);triangle3D(A,C,D,t,light,normalState);}
 };
 Texture lamp{1,1,{0xffd1f1dau}},amber{1,1,{0xffdf9849u}},blue{1,1,{0xff53aec4u}};
 Texture iron{1,1,{0xff343834u}},red{1,1,{0xffb84728u}};
 auto box=[&](Point3 a,Point3 b,const Texture&texture,float light){
  if(hidden(a,b))return;
  // Architectural repeats are measured in metres, never stretched over a deck.
  auto face=[&](Point3 A,Point3 B,Point3 C,Point3 D,float intensity){auto length3=[](Point3 p){return std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);};quad(A,B,C,D,texture,intensity,w.level()>=3?Vec2{length3(B-A)*(&texture==&m_pressureWall?.5f:1.f),length3(D-A)*(&texture==&m_pressureWall?1.f/3.f:1.f)}:Vec2{1,1});};
  if(eye.y<=a.y)face({a.x,a.y,a.z},{b.x,a.y,a.z},{b.x,a.y,b.z},{a.x,a.y,b.z},light);
  if(eye.y>=b.y)face({b.x,b.y,a.z},{a.x,b.y,a.z},{a.x,b.y,b.z},{b.x,b.y,b.z},light*.8f);
  if(eye.x<=a.x)face({a.x,b.y,a.z},{a.x,a.y,a.z},{a.x,a.y,b.z},{a.x,b.y,b.z},light*.85f);
  if(eye.x>=b.x)face({b.x,a.y,a.z},{b.x,b.y,a.z},{b.x,b.y,b.z},{b.x,a.y,b.z},light);
  if(eye.z>=b.z)face({a.x,a.y,b.z},{b.x,a.y,b.z},{b.x,b.y,b.z},{a.x,b.y,b.z},light*1.1f);
  if(eye.z<=a.z)face({a.x,b.y,a.z},{b.x,b.y,a.z},{b.x,a.y,a.z},{a.x,a.y,a.z},light*.65f);
 };
 // Low-sided, capped pipes retain a cylindrical silhouette without dense meshes.
 auto cylinder=[&](Point3 a,Point3 b,float radius,const Texture&texture){
  Point3 axis=b-a;float length=std::sqrt(axis.x*axis.x+axis.y*axis.y+axis.z*axis.z);if(length<.001f)return;
  axis=axis*(1.f/length);Point3 u=cross3(axis,std::fabs(axis.z)>.9f?Point3{0,1,0}:Point3{0,0,1});
  u=u*(1.f/std::sqrt(u.x*u.x+u.y*u.y+u.z*u.z));Point3 v=cross3(axis,u);
  if(!sphereVisible((a+b)*.5f,length*.5f+radius))return;
  if(hidden({std::min(a.x,b.x)-radius,std::min(a.y,b.y)-radius,std::min(a.z,b.z)-radius},{std::max(a.x,b.x)+radius,std::max(a.y,b.y)+radius,std::max(a.z,b.z)+radius}))return;
  for(int i=0;i<10;++i){float t=i*kTwoPi/10,n=(i+1)*kTwoPi/10;auto p=(u*std::cos(t)+v*std::sin(t))*radius,q=(u*std::cos(n)+v*std::sin(n))*radius;
   quad(a+p,a+q,b+q,b+p,texture,1.f,{radius*.63f,length});
   tri({a,.5f,.5f},{a+q,1,1},{a+p,0,0},texture,.8f);tri({b,.5f,.5f},{b+p,0,0},{b+q,1,1},texture,.8f);
  }
 };
 auto prop=[&](Mesh&mesh,const Texture&texture,float x,float y,float height,float yaw,float footprint=.94f,float base=-999.f){
  if(base==-999.f)base=w.floorHeight(x,y);if(base+height<game.dormantBelow())return;Point3 receiver{x,y,base+height*.5f};if(!sphereVisible(receiver,std::max(height,footprint)))return;
  if(hidden({x-footprint,y-footprint,base},{x+footprint,y+footprint,base+height}))return;
  objectLighting=true;objectLight=(illumination(receiver,{0,0,1})+illumination(receiver,{1,0,0}))*.5f;
  Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;
  float scale=std::min(height/std::max(.001f,range.y),footprint/std::max(range.x,range.z));
  float cosine=std::cos(yaw),sine=std::sin(yaw);
  for(auto face:mesh.triangles){for(auto&vertex:face.v){auto p=(vertex.p-center)*scale;vertex.p={x+p.x*cosine+p.z*sine,y-p.x*sine+p.z*cosine,base+p.y+range.y*scale*.5f};}
   auto n=cross3(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);float light=.72f+.3f*std::fabs(n.z)/std::max(.001f,std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z));tri(face.v[0],face.v[1],face.v[2],texture,light);
  }
  objectLighting=false;
 };
 auto facility=[&](int model,float x,float y,float base,float width,float depth,float height,float yaw){
  auto&mesh=m_facilityMeshes[model];Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;
  if(base+height<game.dormantBelow())return;Point3 receiver{x,y,base+height*.5f};if(!sphereVisible(receiver,std::max({width,depth,height})))return;
  float radius=std::max(width,depth);if(hidden({x-radius,y-radius,base},{x+radius,y+radius,base+height}))return;
  objectLighting=true;objectLight=(illumination(receiver,{0,0,1})+illumination(receiver,{1,0,0}))*.5f;
  float c=std::cos(yaw),s=std::sin(yaw);
  for(auto face:mesh.triangles){
   for(auto&v:face.v){auto p=v.p-center;p={p.x*width/std::max(.001f,range.x),p.y*height/std::max(.001f,range.y),p.z*depth/std::max(.001f,range.z)};v.p={x+p.x*c+p.z*s,y-p.x*s+p.z*c,base+p.y+height*.5f};}
   auto n=cross3(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);
   // Pack exports include thin panels and mixed winding. Keep both sides;
   // the shared depth buffer selects the visible exterior without opening holes.
   float light=.8f+.25f*std::fabs(n.z)/std::max(.001f,std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z));
   tri(face.v[0],face.v[1],face.v[2],facilityTexture(model,face.part),light);
  }
  objectLighting=false;
 };
 for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x){float X=float(x),Y=float(y),Z=w.ceilingHeight(X+.5f,Y+.5f);
  // Only resident chunks reach this renderer; reject off-screen modules early.
  if(w.tile(x,y)!='#'){
   // Half-metre floor patches expose real stair risers and the sides of raised decks.
   for(int sy=0;sy<2;++sy)for(int sx=0;sx<2;++sx){float ax=X+sx*.5f,ay=Y+sy*.5f,h=w.floorHeight(ax+.25f,ay+.25f);
    quad({ax,ay,h},{ax+.5f,ay,h},{ax+.5f,ay+.5f,h},{ax,ay+.5f,h},w.level()>=4?(w.metalFloor(x,y)?m_floor:m_pressureFloor):w.level()==3?m_concrete:w.level()==1?(h>0?m_pressureMetal:m_pressureFloor):(w.metalFloor(x,y)?m_floor:m_concrete),w.level()==1?.9f:w.level()==3?.95f:w.metalFloor(x,y)?.8f:.95f);
    float north=w.floorHeight(ax+.25f,ay-.25f),south=w.floorHeight(ax+.25f,ay+.75f),west=w.floorHeight(ax-.25f,ay+.25f),east=w.floorHeight(ax+.75f,ay+.25f);
    if(h>north)quad({ax,ay,north},{ax+.5f,ay,north},{ax+.5f,ay,h},{ax,ay,h},m_metal,.9f);
    if(h>south)quad({ax+.5f,ay+.5f,south},{ax,ay+.5f,south},{ax,ay+.5f,h},{ax+.5f,ay+.5f,h},m_metal,.9f);
    if(h>west)quad({ax,ay+.5f,west},{ax,ay,west},{ax,ay,h},{ax,ay+.5f,h},m_metal,.9f);
    if(h>east)quad({ax+.5f,ay,east},{ax+.5f,ay+.5f,east},{ax+.5f,ay+.5f,h},{ax+.5f,ay,h},m_metal,.9f);
   }
    quad({X,Y+1,Z},{X+1,Y+1,Z},{X+1,Y,Z},{X,Y,Z},m_facilityTextures.at("ceiling_1"),.6f);
   // Close ceiling height changes instead of exposing the void between sectors.
   float northCeiling=w.ceilingHeight(X+.5f,Y-.01f),westCeiling=w.ceilingHeight(X-.01f,Y+.5f);
   if(Z>northCeiling&&w.tile(x,y-1)!='#')quad({X,Y,northCeiling},{X+1,Y,northCeiling},{X+1,Y,Z},{X,Y,Z},m_metal,.7f);
   if(Z>westCeiling&&w.tile(x-1,y)!='#')quad({X,Y+1,westCeiling},{X,Y,westCeiling},{X,Y,Z},{X,Y+1,Z},m_metal,.7f);
   float southCeiling=w.ceilingHeight(X+.5f,Y+1.01f),eastCeiling=w.ceilingHeight(X+1.01f,Y+.5f);
   if(Z>southCeiling&&w.tile(x,y+1)!='#')quad({X+1,Y+1,southCeiling},{X,Y+1,southCeiling},{X,Y+1,Z},{X+1,Y+1,Z},m_metal,.7f);
   if(Z>eastCeiling&&w.tile(x+1,y)!='#')quad({X+1,Y,eastCeiling},{X+1,Y+1,eastCeiling},{X+1,Y+1,Z},{X+1,Y,Z},m_metal,.7f);
   if(w.level()<4&&y%4==0)box({X,Y+.12f,Z-.28f},{X+1,Y+.28f,Z-.03f},m_metal,.7f);
   if(w.level()<4&&(x==2||x==20)){box({X+.06f,Y,Z-.5f},{X+.17f,Y+1,Z-.39f},m_metal,.8f);box({X+.28f,Y,Z-.5f},{X+.36f,Y+1,Z-.42f},m_metal,.65f);}
   // Painted route edges and worn hazard stripes tie the loops together.
   // Route instruction paint removed; safety tape remains at machinery and doors.
   char tile=w.tile(x,y);
   if(tile=='C')prop(m_crateMesh,m_crateTexture,X+.5f,Y+.5f,.85f,(x%2)*1.5708f);
   if(tile=='B')prop(m_barrelMesh,m_barrelTexture,X+.5f,Y+.5f,1.1f,float(x));
   if(tile=='T'){
    // Six tall coolant vessels give the foundry a visible central landmark.
    for(int side=0;side<12;++side){float a=side*kPi/6,b=(side+1)*kPi/6;float ax=X+.5f+std::cos(a)*.47f,ay=Y+.5f+std::sin(a)*.47f,bx=X+.5f+std::cos(b)*.47f,by=Y+.5f+std::sin(b)*.47f;
     quad({ax,ay,.12f},{bx,by,.12f},{bx,by,2.5f},{ax,ay,2.5f},m_metal,.5f+.2f*std::fabs(std::cos(a)));
     quad({ax,ay,1.1f},{bx,by,1.1f},{bx,by,1.18f},{ax,ay,1.18f},m_routePaint,.8f);
     tri({{X+.5f,Y+.5f,2.62f},.5f,.5f},{{ax,ay,2.5f},0,0},{{bx,by,2.5f},1,0},m_metal,.7f);
    }
    box({X+.39f,Y+.39f,2.6f},{X+.61f,Y+.61f,Z},m_metal,.8f);
   }
  }else{
   float roof=Z;Z=w.wallHeight(x,y);
   if(Z<roof){quad({X,Y,Z},{X+1,Y,Z},{X+1,Y+1,Z},{X,Y+1,Z},m_metal,.8f);quad({X,Y+1,roof},{X+1,Y+1,roof},{X+1,Y,roof},{X,Y,roof},m_metal,.43f);}
   auto wall=[&](float ax,float ay,float bx,float by,float light){
    float dx=bx-ax,dy=by-ay,yaw=-std::atan2(dy,dx),cx=(ax+bx)*.5f,cy=(ay+by)*.5f;
    auto&material=w.level()==0?m_wall:m_pressureWall;
    float offset=(dx!=0?ax*dx:ay*dy)*.5f;
    float wallBase=w.level()>=3?-9.f:0.f;
    quad({ax,ay,wallBase},{bx,by,wallBase},{bx,by,Z},{ax,ay,Z},material,1.f,{.5f,(Z-wallBase)/3.f},{offset,0});
    if((x*3+y)%9==0&&Z>=2.7f&&w.wallSpaceFree({cx,cy},{dx,dy},.68f,.65f,1.33f))facility(3,cx-dy*.018f,cy+dx*.018f,.65f,.68f,.034f,.68f,yaw);
    if(w.level()<3&&(x+y)%4==0)facility(2,cx-dy*.055f,cy+dx*.055f,0,.15f,.16f,Z,yaw);
    (void)light;
   };
   if(w.tile(x-1,y)!='#')wall(X,Y,X,Y+1,.90f);
   if(w.tile(x+1,y)!='#')wall(X+1,Y+1,X+1,Y,.90f);
   if(w.tile(x,y-1)!='#')wall(X+1,Y,X,Y,.75f);
   if(w.tile(x,y+1)!='#')wall(X,Y+1,X+1,Y+1,.75f);
  }
 }
 for(auto&s:w.structures()){
  if(s.top<game.dormantBelow())continue;
  if(s.material==6)continue; // Collision only: supplied cabinet mesh is drawn below.
  if(!sphereVisible({(s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f,(s.bottom+s.top)*.5f},std::max({s.x2-s.x1,s.y2-s.y1,s.top-s.bottom})))continue;
  if(s.material==1){
   // Faceted containment jacket, with consistent metre-scale panels.
   for(int side=0;side<12;++side){float a=side*kPi/6,b=(side+1)*kPi/6;float ax=12+1.74f*std::cos(a),ay=20+1.74f*std::sin(a),bx=12+1.74f*std::cos(b),by=20+1.74f*std::sin(b);
    quad({ax,ay,s.bottom},{bx,by,s.bottom},{bx,by,s.top},{ax,ay,s.top},m_bulkhead,1.f,{.9f,5.6f});
    for(float z:{-8.6f,-6.5f,-4.f}){float A=12+1.82f*std::cos(a),B=20+1.82f*std::sin(a),C=12+1.82f*std::cos(b),D=20+1.82f*std::sin(b);quad({A,B,z},{C,D,z},{C,D,z+.16f},{A,B,z+.16f},m_pressureMetal,1.1f,{.94f,.16f});}
    tri({{12,20,s.top},.5f,.5f},{{ax,ay,s.top},0,0},{{bx,by,s.top},1,0},m_metal,1.f);
   }
   // Containment vessel spans both reactor maps. Luminous coolant channels
   // and heavy external bands make it readable from below and the balcony.
   for(float x:{11.55f,12.3f})for(float z:{-8.4f,-7.4f,-5.9f,-4.9f})box({x,18.2f,z},{x+.06f,18.28f,z+.65f},blue,1.2f);
   quad({12.4f,18.17f,-5.7f},{11.6f,18.17f,-5.7f},{11.6f,18.17f,-4.9f},{12.4f,18.17f,-4.9f},m_chemicalSign,1.1f);
  }
  else if(s.material==4){
   // Every tread uses the same concrete body and one inset anti-slip plate.
   box({s.x1,s.y1,s.bottom},{s.x2,s.y2,s.top},m_concrete,1.05f);
   quad({s.x1+.08f,s.y1+.035f,s.top+.003f},{s.x2-.08f,s.y1+.035f,s.top+.003f},{s.x2-.08f,s.y2-.035f,s.top+.003f},{s.x1+.08f,s.y2-.035f,s.top+.003f},m_pressureMetal,1.05f,{s.x2-s.x1-.16f,s.y2-s.y1-.07f});
  }
  else if(!s.rail)box({s.x1,s.y1,s.bottom},{s.x2,s.y2,s.top},s.material==7?m_bulkhead:s.material==2?m_panelMetal:s.material==3||(w.level()==3&&s.top-s.bottom>1.5f)?m_pressureWall:w.level()==3?m_bulkhead:m_floor,1.05f);
  else{if(s.top-s.bottom>1.5f){
    if(w.level()==3){
     // A safety cage must not become an opaque wall around every cab window.
     box({s.x1,s.y1,s.bottom},{s.x2,s.y2,s.bottom+.16f},m_panelMetal,.8f);
     box({s.x1,s.y1,s.bottom+.92f},{s.x2,s.y2,s.bottom+.96f},m_metal,.9f);
     float run=std::max(s.x2-s.x1,s.y2-s.y1);int bars=std::max(1,int(run/.4f));
     for(int i=1;i<=bars;++i){float t=float(i)/(bars+1),x=s.x1+(s.x2-s.x1)*t,y=s.y1+(s.y2-s.y1)*t;
      box({x-.012f,y-.012f,s.bottom+.16f},{x+.012f,y+.012f,s.top-.07f},m_metal,.85f);}
    }else box({s.x1+.012f,s.y1+.012f,s.bottom+.012f},{s.x2-.012f,s.y2-.012f,s.top-.08f},m_floor,.8f);
   }
   box({s.x1,s.y1,s.top-.07f},{s.x2,s.y2,s.top},m_panelMetal,.95f);
   box({s.x1,s.y1,s.bottom},{s.x1+.055f,s.y1+.055f,s.top-.07f},m_panelMetal,.9f);
   box({s.x2-.055f,s.y2-.055f,s.bottom},{s.x2,s.y2,s.top-.07f},m_panelMetal,.9f);}
 }
 // The supplied tape is a straight strip. Lay one continuous strip across each
 // opening, preserving its aspect ratio instead of repeating corner decals per tile.
 auto stripeBand=[&](float left,float right,float centerY,float offset=0.f){
  float halfWidth=(right-left)*m_hazard.height/m_hazard.width*.5f;
  float height=w.floorHeight((left+right)*.5f,centerY)+offset+.009f;
  quad({left,centerY-halfWidth,height},{right,centerY-halfWidth,height},{right,centerY+halfWidth,height},{left,centerY+halfWidth,height},m_hazard,.85f);
 };
 for(auto&door:w.doors())stripeBand(door.left,door.right,door.y,door.z);
 for(int y=1;y<World::Height-1;++y)for(int x=1;x<World::Width-1;){
  if(w.tile(x,y)!='G'){++x;continue;}
  int start=x;while(x<World::Width-1&&w.tile(x,y)=='G')++x;
  stripeBand(float(start),float(x),y-.18f);
 }
 // Door lintels and signs are geometry in the world, visible along both routes.
 for(auto&door:w.doors()){
  float x=(door.left+door.right)*.5f,y=door.y-.5f,half=(door.right-door.left)*.5f;
  float base=w.floorHeight(x,door.y)+door.z;
  box({x-half,y-.12f,base+2.5f},{x+half,y+1.12f,base+3.f},m_metal,.8f);
  float bottom=base+door.open*2.65f;
  box({door.left,door.y-.11f,bottom},{door.right,door.y+.11f,bottom+2.48f},m_bulkhead,1.f);
  float stripeHeight=(door.right-door.left)*m_hazard.height/m_hazard.width;
  quad({door.right,door.y-.115f,bottom+.20f},{door.left,door.y-.115f,bottom+.20f},{door.left,door.y-.115f,bottom+.20f+stripeHeight},{door.right,door.y-.115f,bottom+.20f+stripeHeight},m_hazard,.95f);
  quad({door.left,door.y+.115f,bottom+.20f},{door.right,door.y+.115f,bottom+.20f},{door.right,door.y+.115f,bottom+.20f+stripeHeight},{door.left,door.y+.115f,bottom+.20f+stripeHeight},m_hazard,.95f);
  for(float jamb:{door.left,door.right})box({jamb-.055f,y-.08f,base},{jamb+.055f,y+1.08f,base+2.75f},m_panelMetal,.85f);
  // Switch housings attach to the fixed wall on both sides, never to the moving leaf.
  for(float side:{-1.f,1.f}){float face=door.y+side*.535f,switchX=door.left-.25f;
   float h=base;
   box({switchX-.17f,face-.035f,h+.86f},{switchX+.17f,face+.035f,h+1.27f},m_panelMetal,.9f);
   if(side<0)quad({switchX+.14f,face-.04f,h+.89f},{switchX-.14f,face-.04f,h+.89f},{switchX-.14f,face-.04f,h+1.24f},{switchX+.14f,face-.04f,h+1.24f},m_terminalTexture,.85f);
   else quad({switchX-.14f,face+.04f,h+.89f},{switchX+.14f,face+.04f,h+.89f},{switchX+.14f,face+.04f,h+1.24f},{switchX-.14f,face+.04f,h+1.24f},m_terminalTexture,.85f);
   box({switchX-.025f,face+side*.042f-.005f,h+1.17f},{switchX+.025f,face+side*.042f+.005f,h+1.20f},door.opening?blue:amber,1.55f);
  }
  // Compact sector label bolted directly to the header.
  auto&front=door.transfer?(w.level()==0?m_transferSign:w.level()==1?m_gantrySign:w.level()==3?m_reactorSign:m_surfaceSign):w.level()==1?(y<10?m_pumpSign:m_controlSign):(y<10?m_processingSign:m_containmentSign);
  auto&back=door.transfer?front:w.level()==1?m_transferSign:(y<10?m_intakeSign:m_processingSign);
  quad({x+.7f,y-.125f,base+2.52f},{x-.7f,y-.125f,base+2.52f},{x-.7f,y-.125f,base+2.9575f},{x+.7f,y-.125f,base+2.9575f},front,.9f);
  quad({x-.7f,y+1.125f,base+2.52f},{x+.7f,y+1.125f,base+2.52f},{x+.7f,y+1.125f,base+2.9575f},{x-.7f,y+1.125f,base+2.9575f},back,.9f);
 }
 for(auto&p:w.props()){Mesh* meshes[]={&m_pumpMesh,&m_compressorMesh,&m_pipeMesh,&m_gateMesh};Texture* textures[]={&m_pumpTexture,&m_compressorTexture,&m_pipeTexture,&m_gateTexture};prop(*meshes[p.kind],*textures[p.kind],p.position.x,p.position.y,p.height,p.yaw,p.footprint,w.floorHeight(p.position.x,p.position.y)+p.base);}
 // Original square fixture proportions, with its top 2 cm below its support.
 // The light source sits 4 cm beneath the luminous underside.
 for(const auto&light:w.lights()){movingGeometry=w.level()==3&&&light==&w.lights().back();m_emissionScale=movingGeometry?w.liftLampPower():1.f;facility(4,light.position.x,light.position.y,light.z+.04f,.8f,.8f,.09f,0);}movingGeometry=false;m_emissionScale=1.f;
 for(const auto&fixture:w.fixtures())facility(fixture.model,fixture.position.x,fixture.position.y,w.floorHeight(fixture.position.x,fixture.position.y)+fixture.base,fixture.width,fixture.depth,fixture.height,fixture.yaw);
 for(auto&c:game.clutter()){
  auto&mesh=m_clutterMeshes[c.kind];auto center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;auto size=c.size();float scale=std::max({size[0],size[1],size[2]})/std::max({range.x,range.y,range.z});
  float mid=c.z+c.height()*.5f;if(!sphereVisible({c.pos.x,c.pos.y,mid},.4f))continue;
  objectLighting=true;objectLight=illumination({c.pos.x,c.pos.y,mid},{0,0,1});
  for(auto face:mesh.triangles){for(auto&v:face.v){auto p=(v.p-center)*scale;auto r=c.rotate(p.x,p.z,p.y);v.p={c.pos.x+r[0],c.pos.y+r[1],mid+r[2]};}tri(face.v[0],face.v[1],face.v[2],m_clutterTextures[c.kind],1.f);}objectLighting=false;
 }
 if(w.level()>=4){
  // Headers tie the service bays into a supported industrial interior.
  for(float y:{4.f,10.f,16.f,22.f}){float roof=w.ceilingHeight(12,y);
   box({1,y-.1f,roof-.18f},{23,y+.1f,roof},m_panelMetal,.9f);
  }
  if(w.level()==5)for(float x:{8.5f,15.5f}){
   cylinder({x,1.f,-6.25f},{x,22.5f,-6.25f},.16f,m_pipeTexture);
   for(float y:{4.f,10.f,16.f,22.f}){
    box({x-.025f,y-.035f,-6.45f},{x+.025f,y+.035f,-5.75f},iron,.9f);
    box({x-.21f,y-.06f,-6.45f},{x+.21f,y+.06f,-6.40f},iron,.9f);
   }
  }
  // Suspended return lines stay above the walking envelope, with visible hangers.
  for(float x:{3.f,20.5f}){
   cylinder({x,2.f,-6.55f},{x,22.f,-6.55f},.11f,m_pipeTexture);
   for(float y:{3.f,7.f,11.f,15.f,19.f,21.f}){
    float roof=w.ceilingHeight(x,y);
    box({x-.18f,y-.05f,-6.72f},{x+.18f,y+.05f,-6.66f},iron,.8f);
    for(float dx:{-.16f,.16f})box({x+dx-.025f,y-.025f,-6.7f},{x+dx+.025f,y+.025f,roof},iron,.8f);
   }
  }
 }
 if(w.level()==5){
  // The sealed end bulkhead has visible reinforcement and an unpowered lock.
  for(float x:{18.45f,20.45f})box({x,23.58f,-8.9f},{x+.10f,23.65f,-6.65f},m_panelMetal,.9f);
  box({19.38f,23.52f,-8.2f},{19.62f,23.65f,-7.82f},iron,.9f);
  quad({18.1f,23.64f,-8.83f},{20.9f,23.64f,-8.83f},{20.9f,23.64f,-8.64f},{18.1f,23.64f,-8.64f},m_hazard,.85f);
  cylinder({20.5f,18.2f,-9.f},{20.5f,18.2f,-6.55f},.11f,m_pipeTexture);
  box({20.28f,17.98f,-9.f},{20.72f,18.42f,-8.88f},iron,.9f);
  static Texture steam=[](){Texture t{32,32,std::vector<uint32_t>(1024)};t.clampEdges=true;
   for(int y=0;y<32;++y)for(int x=0;x<32;++x){float dx=(x-15.5f)/16,dy=(y-15.5f)/16,d=dx*dx+dy*dy;unsigned noise=unsigned(x+y*32+1)*747796405u+2891336453u;noise=((noise>>((noise>>28)+4))^noise)*277803737u;noise=((noise>>22)^noise)&255u;
    t.pixels[y*32+x]=d<1&&noise<190*(1-d)?0xff7f8e91u:0;}return t;}();
  Point3 side{-std::sin(game.player().angle),std::cos(game.player().angle),0};
  for(int i=0;i<7;++i){float age=std::fmod(game.elapsed()*.55f+i/7.f,1.f),radius=.07f+age*.2f;Point3 p{20.35f-age*.28f,18.2f,-7.8f+age*1.05f},up{0,0,radius};
   quad(p-side*radius-up,p+side*radius-up,p+side*radius+up,p-side*radius+up,steam,.9f);}
 }
 if(w.level()==0){
 box({1.f,1.37f,1.27f},{1.055f,3.63f,2.03f},m_panelMetal,.9f);
 quad({1.06f,3.6f,1.3f},{1.06f,1.4f,1.3f},{1.06f,1.4f,2.f},{1.06f,3.6f,2.f},m_intakeSign,1.1f);
 // The former end wall is now a chunk opening. Suspend its dispatch board
 // above head height with real steel straps and ceiling anchor plates.
 box({20.17f,22.94f,2.57f},{22.83f,23.f,3.43f},m_panelMetal,.9f);
 for(float hangerX:{20.45f,22.55f}){
  float ceiling=w.ceilingHeight(hangerX,22.97f);
  box({hangerX-.045f,22.925f,3.32f},{hangerX+.045f,23.015f,ceiling},m_metal,1.f);
  box({hangerX-.14f,22.83f,ceiling-.055f},{hangerX+.14f,23.11f,ceiling},m_panelMetal,.9f);
 }
 quad({22.8f,22.935f,2.60f},{20.2f,22.935f,2.60f},{20.2f,22.935f,3.40f},{22.8f,22.935f,3.40f},m_exitSign,1.1f);
 // Authored warning plates from the asset pack, mounted proud of the wall.
 auto warning=[&](float x,float y,const Texture&texture,float width){float height=width*texture.height/texture.width;
  box({x-width*.5f-.025f,y-.025f,.95f},{x+width*.5f+.025f,y+.025f,.95f+height+.05f},m_panelMetal,.8f);
  quad({x+width*.5f,y-.03f,.975f},{x-width*.5f,y-.03f,.975f},{x-width*.5f,y-.03f,.975f+height},{x+width*.5f,y-.03f,.975f+height},texture,1.f);
 };
 warning(7.5f,7.97f,m_machineSign,.85f);warning(16.5f,22.96f,m_confinedSign,.85f);
 // Chemical diamonds belong on the vessels and retain their square orientation.
 for(float x:{13.5f,14.5f}){
  box({x-.24f,12.94f,.88f},{x+.24f,13.005f,1.36f},m_panelMetal,.8f);
  quad({x-.21f,13.01f,.91f},{x+.21f,13.01f,.91f},{x+.21f,13.01f,1.33f},{x-.21f,13.01f,1.33f},m_chemicalSign,1.1f);
 }
 box({10,14,.66f},{11,15,3.2f},m_metal,.9f);
 quad({9.99f,14.15f,.83f},{9.99f,14.85f,.83f},{9.99f,14.85f,1.05f},{9.99f,14.15f,1.05f},m_serviceSign,1.1f);
 }
 if(w.level()==3){
  // Boarding deck: freight holding on the west, traction plant on the east.
  const auto& corpse=game.hazmat();
  if(corpse.initialized&&sphereVisible({corpse.p[0].x,corpse.p[0].y,corpse.p[0].z},2.1f)){
   quad({8.25f,6.7f,.009f},{9.55f,6.7f,.009f},{9.55f,7.9f,.009f},{8.25f,7.9f,.009f},m_blood,1.1f);
   quad({8.05f,7.7f,.010f},{8.43f,7.7f,.010f},{8.43f,8.13f,.010f},{8.05f,8.13f,.010f},m_blood,.9f);
   // Narrow contact smears lead into the final collapse, not an even halo.
   quad({9.00f,7.58f,.011f},{9.20f,7.49f,.011f},{9.72f,8.28f,.011f},{9.55f,8.39f,.011f},m_blood,.82f);
   quad({9.49f,8.21f,.012f},{9.62f,8.15f,.012f},{9.94f,8.62f,.012f},{9.83f,8.69f,.012f},m_blood,.72f);
   bool poseChanged=!m_hazmatPoseValid;
   for(int joint=0;joint<Ragdoll::Count&&!poseChanged;++joint){auto a=corpse.p[joint],b=m_hazmatPoseJoints[joint];poseChanged=a.x!=b.x||a.y!=b.y||a.z!=b.z;}
   if(poseChanged){corpse.skin(m_hazmatPose);m_hazmatPoseJoints=corpse.p;m_hazmatPoseValid=true;}
   const auto&posed=m_hazmatPose;auto&vertices=Ragdoll::asset().vertices;objectLighting=true;objectLight=illumination({corpse.p[1].x,corpse.p[1].y,corpse.p[1].z},{0,0,1});
   for(size_t i=0;i<vertices.size();i+=3){MeshVertex face[3];for(int j=0;j<3;++j){auto p=posed[i+j];face[j]={{p.x,p.y,p.z},vertices[i+j].u,vertices[i+j].v};}
    tri(face[0],face[1],face[2],m_hazmatTextures[vertices[i].material],.95f);
    // The blood follows the skinned chest surface, not a floating world card.
    bool patch=vertices[i].material==0;float front=0;for(int j=0;j<3;++j){auto p=vertices[i+j].p;patch&=p.x>-.40f&&p.x<.40f&&p.z>.72f&&p.z<1.42f;front+=p.y;}
    if(patch&&std::fabs(front)>.02f){auto n=cross3(face[1].p-face[0].p,face[2].p-face[0].p);auto a=vertices[i+1].p-vertices[i].p,b=vertices[i+2].p-vertices[i].p;float restY=a.z*b.x-a.x*b.z;float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);n=n*((restY*front<0?-1.f:1.f)*.004f/std::max(.00001f,len));
     for(int j=0;j<3;++j){face[j].p=face[j].p+n;auto p=vertices[i+j].p;face[j].u=p.x/.8f+.5f;face[j].v=1-(p.z-.72f)/.7f;}tri(face[0],face[1],face[2],m_blood,.95f);
    }
   }
   objectLighting=false;
  }
  // The central approach is kept clear; equipment is concentrated at cab windows.
  // This bay ends at the existing x=7 structural partition: keep every crate
  // fully west of it rather than allowing an attractive but impossible overlap.
  for(Vec2 cargo:{Vec2{4.45f,7.9f},Vec2{5.75f,9.25f}}){prop(m_crateMesh,m_crateTexture,cargo.x,cargo.y,.85f,0,1.05f,0);
   quad({cargo.x-.46f,cargo.y-.58f,.012f},{cargo.x+.46f,cargo.y-.58f,.012f},{cargo.x+.46f,cargo.y-.43f,.012f},{cargo.x-.46f,cargo.y-.43f,.012f},m_hazard,.9f);}
  prop(m_barrelMesh,m_barrelTexture,6.5f,13.4f,.95f,0,.65f,0);
  for(float y:{10.55f,11.45f})for(float z:{.13f,.77f})prop(m_crateMesh,m_crateTexture,6.2f,y,.30f,0,.34f,z);
  // Cable drum on bearing blocks; repeated narrow collars read as wound cable.
  cylinder({16.1f,11.3f,1.15f},{17.8f,11.3f,1.15f},.42f,m_pressureMetal);
  for(float x:{16.1f,17.7f})cylinder({x,11.3f,1.15f},{x+.10f,11.3f,1.15f},.56f,m_metal);
  for(int i=0;i<13;++i){float x=16.3f+i*.10f;cylinder({x,11.3f,1.15f},{x+.035f,11.3f,1.15f},.43f,iron);}
  for(float x:{16.25f,17.6f}){box({x-.13f,11.05f,0},{x+.13f,11.55f,1.1f},m_pressureMetal,.9f);box({x-.3f,10.8f,0},{x+.3f,11.8f,.12f},m_metal,.9f);}
  cylinder({16.2f,11.3f,1.55f},{14.9f,11.3f,2.45f},.025f,iron);
  box({15.1f,9.8f,2.35f},{19.f,10.1f,2.55f},m_panelMetal,.9f);
  // Ventilation trunk runs into the shaft rather than ending in an empty room.
  box({4.f,12.4f,2.15f},{8.85f,12.95f,2.55f},m_pressureMetal,.9f);
  for(float x:{4.2f,6.2f,8.2f})box({x,12.37f,2.12f},{x+.08f,12.98f,2.58f},m_metal,.8f);
  // Theatre-like shaft bays: readable machinery silhouettes within the window
  // sightline, backed by opaque walls instead of distant explorable rooms.
  for(float z:{-3.f,3.f,6.f,9.f}){
   for(float x:{8.16f,15.84f}){
    if(z==3)facility(6,x<12?8.5f:15.5f,11.65f,z+.12f,2.38f,.70f,1.26f,kPi*.5f);
    else if(z==6){for(float y:{10.7f,12.2f,13.7f})facility(8,x,y,z+.55f,.7856f,.136f,1.0656f,kPi*.5f);}
    else if(z==-3){for(float y:{10.8f,12.f,13.2f}){cylinder({x,y,z+.12f},{x,y,z+2.45f},.12f,m_pressureMetal);for(float h:{.4f,1.9f})cylinder({x,y,z+h},{x,y,z+h+.09f},.18f,m_metal);}}
    else {box({x-.06f,10.2f,z},{x+.06f,13.8f,z+2.35f},m_bulkhead,1.f);}
   }
   // Different color accents and heavy cross-members identify a passing level.
   auto&accent=z==-3?blue:z==6?red:amber;
   for(float x:{8.95f,14.95f}){box({x,9.2f,z+2.28f},{x+.10f,14.8f,z+2.42f},m_metal,.9f);box({x+.025f,10.3f,z+2.24f},{x+.075f,13.7f,z+2.27f},accent,1.65f);}
  }
  // Close guide rails and repeated height markers supply real parallax during
  // ascent and much faster upward optical flow during the scripted fall.
  for(float x:{9.45f,14.45f}){
   box({x,10.5f,-8.9f},{x+.10f,10.64f,12.f},m_metal,.9f);
   box({x,13.35f,-8.9f},{x+.10f,13.49f,12.f},m_metal,.9f);
   for(int band=-7;band<10;++band){float z=band*1.2f;box({x-.025f,13.30f,z},{x+.125f,13.34f,z+.10f},band<0?red:amber,1.65f);}
  }
  auto phase=w.liftPhase();float phaseTime=w.liftPhaseTime(),cab=w.liftHeight();
  bool broken=phase==World::LiftPhase::Falling||phase==World::LiftPhase::Caught||phase==World::LiftPhase::Crashed;
  bool scrape=phase==World::LiftPhase::Falling||(phase==World::LiftPhase::Caught&&phaseTime<.35f)||(phase==World::LiftPhase::Crashed&&phaseTime<.45f);
  movingGeometry=true;
  if(broken&&phase!=World::LiftPhase::Crashed){float sway=.12f*std::sin(phaseTime*11.f);Point3 a{14.25f,11.5f,cab+2.9f},b{14.25f+sway,11.5f,cab+1.6f},c{14.3f+sway*1.8f,11.5f,cab+.85f};
   quad(a,b,b+Point3{0,.035f,0},a+Point3{0,.035f,0},m_metal,.8f,{1,3});quad(b,c,c+Point3{0,.035f,0},b+Point3{0,.035f,0},m_metal,.8f,{1,2});
  }
  if(scrape)for(float x:{9.65f,14.35f})for(int i=0;i<12;++i){float life=std::fmod(phaseTime*2.7f+i*.137f,1.f),y=12.1f+i*.055f,z=cab+.65f+life*1.7f;
   quad({x,y,z},{x,y+.012f,z},{x,y+.06f,z+.10f+life*.15f},{x,y+.048f,z+.10f+life*.15f},i%3?amber:lamp,1.8f);
  }
  movingGeometry=false;
  // Purchased instrument cabinets flank the actual interactive computer.
  for(float x:{17.35f,18.85f})prop(m_consoleMesh,m_consoleTexture,x,19.25f,1.188f,kPi,.626f,-9.f);
  // Elevated coolant headers connect the west pump bay to the containment jacket.
  for(float z:{-6.6f,-3.65f}){
   box({3.8f,21.85f,z},{10.4f,22.03f,z+.18f},m_pressureMetal,.9f);
   box({10.22f,20.4f,z},{10.4f,22.03f,z+.18f},m_pressureMetal,.9f);
   for(float x:{4.f,6.f,8.f,10.f})box({x,21.82f,z-.025f},{x+.08f,22.06f,z+.205f},m_metal,1.f);
  }
  movingGeometry=true;
  float z=w.liftHeight();bool ready=w.liftPhase()==World::LiftPhase::Ready,crashed=w.liftPhase()==World::LiftPhase::Crashed;
  // Modular freight cab: structural frame, recessed kick panels, wide cage
  // windows and split doors. Detail is geometry using the purchased materials.
  box({10,10,z-.25f},{14,14,z},m_pressureMetal,1.05f);
  quad({10.3f,10.3f,z+.004f},{13.7f,10.3f,z+.004f},{13.7f,13.7f,z+.004f},{10.3f,13.7f,z+.004f},m_floor,1.f,{2,2});
  for(float y:{10.18f,13.62f})quad({10.2f,y,z+.01f},{13.8f,y,z+.01f},{13.8f,y+.16f,z+.01f},{10.2f,y+.16f,z+.01f},m_hazard,1.1f);
  box({10,10,z+2.6f},{14,14,z+2.8f},m_bulkhead,.9f);
  for(float y:{10.15f,13.65f})box({10.12f,y,z+2.42f},{13.88f,y+.2f,z+2.6f},m_metal,.85f);
  for(float x:{10.8f,12.85f})box({x,10.25f,z+2.50f},{x+.10f,13.75f,z+2.59f},m_metal,.85f);
  // Recessed utility panels and ventilation keep the cab from reading as a box.
  for(float y:{10.7f,12.6f}){box({10.125f,y,z+1.65f},{10.15f,y+.6f,z+2.08f},iron,.8f);for(int i=0;i<5;++i)box({10.15f,y+.04f,z+1.69f+i*.07f},{10.18f,y+.56f,z+1.71f+i*.07f},m_metal,.85f);}
  for(float x:{10.f,13.88f}){
   box({x,10,z},{x+.12f,14,z+.65f},m_pressureWall,.95f);
   box({x,10,z+2.12f},{x+.12f,14,z+2.6f},m_panelMetal,.9f);
   for(float y:{10.05f,11.95f,13.8f})box({x,y,z},{x+.12f,y+.15f,z+2.6f},m_metal,.8f);
   float rail=x<11?10.16f:13.78f;
   box({rail,10.2f,z+.82f},{rail+.06f,13.8f,z+.89f},m_metal,1.1f);
   for(float y:{10.6f,12.9f})box({x+.015f,y,z+.13f},{x+.1f,y+.45f,z+.47f},m_bulkhead,1.05f);
  }
  for(float y:{10.f,13.88f}){
   box({10,y,z},{11,y+.12f,z+2.6f},m_panelMetal,1.f);
   box({13,y,z},{14,y+.12f,z+2.6f},m_panelMetal,1.f);
   box({11,y,z+2.2f},{13,y+.12f,z+2.6f},m_panelMetal,1.f);
   float opening=y==10?(ready?1.f:w.liftPhase()==World::LiftPhase::Ascending?1-std::min(1.f,w.liftPhaseTime()/.65f):0.f):(crashed?std::clamp((w.liftPhaseTime()-3.f)/.65f,0.f,1.f):0.f);
   for(int leaf=0;leaf<2;++leaf){float left=11+leaf+(leaf?opening:-opening),right=left+1;
    box({left,y,z},{right,y+.12f,z+1.12f},m_bulkhead,1.f);
    box({left,y,z+1.82f},{right,y+.12f,z+2.2f},m_bulkhead,1.f);
    for(float post:{left,left+.47f,right-.06f})box({post,y,z+1.12f},{post+.06f,y+.12f,z+1.82f},m_metal,.8f);
    box({left,y,z+1.45f},{right,y+.12f,z+1.49f},m_metal,.8f);
   }
   // Load plate and threshold tracks stay on the fixed door frame.
   box({10.94f,y-.025f,z+.01f},{13.06f,y+.145f,z+.035f},m_metal,1.f);
  }
  quad({12.7f,13.865f,z+2.23f},{11.3f,13.865f,z+2.23f},{11.3f,13.865f,z+2.56f},{12.7f,13.865f,z+2.56f},m_liftSign,1.2f);
  // Header-mounted sign leaves the passing scenery window unobstructed.
  box({13.84f,10.88f,z+2.14f},{13.89f,12.12f,z+2.52f},m_panelMetal,.95f);
  quad({13.833f,10.95f,z+2.16f},{13.833f,12.05f,z+2.16f},{13.833f,12.05f,z+2.50f},{13.833f,10.95f,z+2.50f},m_liftDispatch,1.2f);
  bool alarm=w.liftPhase()==World::LiftPhase::Jammed||w.liftPhase()==World::LiftPhase::Falling||w.liftPhase()==World::LiftPhase::Caught;
  auto&signal=alarm&&int(game.elapsed()*2)%2?m_redPaint:amber;
  for(float x:{10.65f,13.25f})box({x,13.70f,z+2.27f},{x+.1f,13.82f,z+2.49f},signal,1.8f);
  for(float x:{10.15f,13.75f})box({x,10.3f,z+2.8f},{x+.045f,10.35f,crashed||w.liftPhase()==World::LiftPhase::Falling||w.liftPhase()==World::LiftPhase::Caught?z+3.2f:15.8f},m_metal,.7f);
  box({11.3f,11.6f,z+2.8f},{12.7f,12.4f,z+3.05f},m_pressureMetal,.8f);
  // The sealed surface gates stay in the shaft when the room plunges away.
  box({10,14.05f,9},{14,14.25f,12},m_bulkhead,1.f);
  quad({13.8f,14.04f,10.5f},{10.2f,14.04f,10.5f},{10.2f,14.04f,11.3f},{13.8f,14.04f,11.3f},m_surfaceSign,1.1f);
 }
 movingGeometry=false;
 for(auto&terminal:w.terminals()){movingGeometry=w.level()==3&&terminal.control;float x=terminal.position.x,y=terminal.position.y,h=w.floorHeight(x,y)+terminal.z;
  if(terminal.control){
   box({x-.27f,y-.18f,h},{x+.27f,y+.18f,h+.2f},m_panelMetal,.9f);
   facility(8,x,y,h+.2f,.36f,.54f,.75f,kPi*.5f);
  }else if(terminal.reactorAction>=2){
   // Mechanical gate valve: pipe, flanges, valve body, projecting stem and wheel.
   // No electrical switch housing is reused behind the handwheel.
   cylinder({x,y+.12f,h},{x,y+.12f,h+2.45f},.09f,m_pressureMetal);
   for(float z:{.12f,.78f,1.38f,2.32f})cylinder({x,y+.12f,h+z},{x,y+.12f,h+z+.08f},.15f,m_metal);
   cylinder({x,y+.12f,h+.92f},{x,y+.12f,h+1.23f},.17f,m_metal);
   cylinder({x,y+.12f,h+1.08f},{x,y-.19f,h+1.08f},.045f,m_metal);
   cylinder({x,y-.17f,h+1.08f},{x,y-.22f,h+1.08f},.07f,m_metal);
   for(int i=0;i<12;++i){float a=i*kPi/6,b=(i+1)*kPi/6;
    auto point=[&](float angle,float radius,float depth){return Point3{x+radius*std::cos(angle),y+depth,h+1.08f+radius*std::sin(angle)};};
    quad(point(a,.23f,-.23f),point(b,.23f,-.23f),point(b,.19f,-.23f),point(a,.19f,-.23f),m_metal,1.f);
    quad(point(a,.19f,-.19f),point(b,.19f,-.19f),point(b,.23f,-.19f),point(a,.23f,-.19f),m_metal,1.f);
    quad(point(a,.23f,-.19f),point(b,.23f,-.19f),point(b,.23f,-.23f),point(a,.23f,-.23f),m_metal,.9f);
    if(i%3==0)cylinder({x,y-.21f,h+1.08f},point(a,.20f,-.21f),.014f,m_metal);
   }
   // Small identification tag clamped to the pipe, below the mechanical assembly.
   box({x-.16f,y-.005f,h+.46f},{x+.16f,y+.02f,h+.60f},m_panelMetal,.9f);
   quad({x+.15f,y-.01f,h+.47f},{x-.15f,y-.01f,h+.47f},{x-.15f,y-.01f,h+.59f},{x+.15f,y-.01f,h+.59f},terminal.reactorAction==2?m_feedSign:m_returnSign,1.f);
  }else{
   // Personnel records belong on a computer, visually distinct from switchgear.
   box({x-.24f,y-.24f,h},{x+.24f,y+.24f,h+.405f},m_panelMetal,.9f);
   box({x-.27f,y-.27f,h+.405f},{x+.27f,y+.27f,h+.445f},m_panelMetal,1.f);
   facility(11,x,y,h+.445f,.386509f,.53235f,.50505f,kPi);
   if(terminal.reactorAction==1){box({x-.2f,y-.295f,h+.28f},{x+.2f,y-.24f,h+.39f},m_metal,1.f);box({x-.15f,y-.30f,h+.33f},{x+.11f,y-.296f,h+.345f},iron,1.f);box({x+.15f,y-.302f,h+.32f},{x+.17f,y-.295f,h+.34f},w.reactorStage()>=World::ReactorStage::DiskLoaded?blue:amber,1.8f);}
  }
 }
 // Extraction floor remains readable even before its gate unlocks.
 movingGeometry=false;
 if(w.level()==3&&w.reactorStage()==World::ReactorStage::NoDisk){auto p=World::reactorDiskPosition();prop(m_clutterMeshes[5],m_clutterTextures[5],p.x,p.y,.018f,0,.28f,World::ReactorDiskZ);}
 for(int edge=0;edge<3;++edge){float y=22.1f+edge*.25f,h=w.floorHeight(21.5f,y)+.01f;quad({21.1f,y,h},{21.9f,y,h},{21.9f,y+.12f,h},{21.1f,y+.12f,h},game.enemiesRemaining()==0?m_routePaint:m_redPaint,1.f);}
 for(const auto&e:game.enemies()){
  if(e.bodyTop()<game.dormantBelow())continue;
  if(!e.visible())continue;
  Point3 receiver{e.pos.x,e.pos.y,e.z+.85f};if(!sphereVisible(receiver,1.8f))continue;
  objectLighting=true;objectLight=(illumination(receiver,{0,0,1})+illumination(receiver,{1,0,0}))*.5f;
  bool wasp=e.kind==Enemy::Kind::Wasp,warden=e.kind==Enemy::Kind::Warden,brute=e.kind==Enemy::Kind::Brute||warden;
  auto&mesh=warden?m_wardenMesh:wasp?m_waspMesh:brute?m_bruteMesh:m_enemyMesh;
  auto&texture=warden?m_wardenTexture:wasp?m_waspTexture:brute?m_bruteTexture:m_enemyTexture;
  if(warden){int clip=0;float phase=std::fmod(game.elapsed()/2.5f+e.home.x*.1f,1.f);
   if(!e.alive){clip=4;phase=std::min(1.f,e.deathTime/1.15f);}
   else if(e.windup>0){clip=2;phase=(1-std::clamp(e.windup/.55f,0.f,1.f))*.4f;}
   else if(e.strike>0){clip=2;phase=.4f+(1-e.strike)*.6f;}
   else if(e.painFlash>0){clip=3;phase=1-e.painFlash;}
   else if(e.moving){clip=1;phase=std::fmod(e.gait/(2*kPi),1.f);}
   mesh.poseCreature(clip,phase);
  }
  Point3 center=(mesh.minimum+mesh.maximum)*.5f,range=mesh.maximum-mesh.minimum;
  float scale=warden?1.8f/std::max(.01f,range.y):(wasp?1.35f:brute?2.25f:1.5f)/std::max({range.x,range.y,range.z});
  float angle=e.heading,collapse=e.alive?0.f:std::min(1.f,e.deathTime/.65f);
  float shrink=e.alive?1.f:1.f-std::clamp((e.deathTime-1.25f)/1.15f,0.f,1.f);
  float wind=e.windup>0?std::sin(e.windup*5.f)*.12f:0;
  for(auto face:mesh.triangles){
   for(auto&v:face.v){Point3 p=(v.p-center)*scale;
    float localY=p.z;
    float height=p.y+range.y*scale*.5f;
    if(e.alive&&!warden){
     if(wasp){height+=.60f+.065f*std::sin(game.elapsed()*4.f+e.pos.x);if(face.part==1)height+=std::sin(game.elapsed()*36.f)*std::fabs(p.x)*.9f;}
     else if(brute){float sway=e.moving?std::sin(e.gait)*.07f:0;p.x+=sway*height;height+=std::fabs(sway)*.18f;}
     else if(std::fabs(p.x)>.21f){
      // Alternate leg groups rotate around their body attachment; feet lift only while walking.
      float phase=e.gait+(p.x<0?3.14159f:0.f)+std::floor((p.z+.7f)*4.f)*3.14159f;
      float stride=e.moving?std::sin(phase)*.16f:0;
      localY+=stride*(std::fabs(p.x)-.18f);height+=std::max(0.f,e.moving?std::cos(phase)*.10f:0.f);
     }
     height+=wind*(1-std::min(1.f,std::fabs(p.x)));localY+=e.strike*.22f;
     localY-=e.painFlash*.035f;
    }else if(!e.alive&&!warden){height*=1-.88f*collapse;p.x+=std::sin(collapse*3.14f)*.1f*(p.x<0?-1.f:1.f);}
    p.x*=shrink;localY*=shrink;height*=shrink;
    v.p={e.pos.x+localY*std::cos(angle)-p.x*std::sin(angle),e.pos.y+localY*std::sin(angle)+p.x*std::cos(angle),e.z+height+.015f};
   }
   Point3 n=cross3(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
   float light=.72f+.35f*std::fabs(n.z)/std::max(.001f,len)+e.painFlash*.22f;
   tri(face.v[0],face.v[1],face.v[2],wasp&&face.part==1?m_wingTexture:texture,e.alive?light:.65f);
  }
  objectLighting=false;
 }
 // Recognizable authored supplies, with their original UVs and world depth.
 for(auto&p:game.pickups())if(p.active){
  bool health=p.kind==Pickup::Kind::Health;
  prop(health?m_medkitMesh:m_shellsMesh,health?m_medkitTexture:m_shellsTexture,p.pos.x,p.pos.y,health?.4f:.36f,-.3f,health?.65f:.48f);
 }
 // Translucent animated water, drawn after opaque geometry on both backends.
 if(w.level()==5){
  const auto&water=m_water;
  auto wave=[&](float x,float y){float t=game.elapsed();float h=-9.055f+.008f*std::sin(x*5+t*1.8f)*std::cos(y*7-t);
   auto p=game.player();float d=length(Vec2{x,y}-p.pos);if(w.waterSurface(p.pos.x,p.pos.y)>-100&&p.z<-9.02f)h+=.004f*std::sin(d*18-t*7)*std::exp(-d*2);
   return Point3{x,y,h};};
  for(float cy:{9.f,15.f})for(float cx:{8.f,13.f})for(int j=0;j<4;++j)for(int i=0;i<12;++i){float x=cx+i*.25f,y=cy-.5f+j*.25f;
   quad(wave(x,y),wave(x+.25f,y),wave(x+.25f,y+.25f),wave(x,y+.25f),water,1.15f,{.5f,.5f},{x+game.elapsed()*.035f,y-game.elapsed()*.02f});}
 }
}
void SoftwareRenderer::prepareViewModel(const Game& game){
 const auto&motion=game.weaponMotion();
 if(game.unarmed()){bool jab=game.punchAge()<.48f&&!game.guarding();if(!m_armsMesh.poseAction(jab?(game.punchLeft()?"jab.L":"jab.R"):"guard_idle",jab?game.punchAge()/.48f:std::fmod(game.elapsed()*.5f,1.f)))throw std::runtime_error("Missing authored unarmed animation");return;}
 auto center=(m_weaponMesh.minimum+m_weaponMesh.maximum)*.5f,range=m_weaponMesh.maximum-m_weaponMesh.minimum;float scale=1.15f/std::max({range.x,range.y,range.z});
 auto local=[&](Point3 source){auto p=(source-center)*scale;return Point3{p.x+.15f,p.y-.155f,-p.z+.82f};};
 const Point3 pivot{.15f,-.155f,.52f};
 auto animated=[&](Point3 p){p=p-pivot;float y=p.y*std::cos(motion.pitch)+p.z*std::sin(motion.pitch),z=-p.y*std::sin(motion.pitch)+p.z*std::cos(motion.pitch);return Point3{p.x*std::cos(motion.yaw)+z*std::sin(motion.yaw),y+motion.bob,-p.x*std::sin(motion.yaw)+z*std::cos(motion.yaw)-motion.back}+pivot;};
 auto armCenter=(m_armsMesh.minimum+m_armsMesh.maximum)*.5f;
 auto toRig=[&](Point3 view){auto p=(view-Point3{.15f,-.285f,.52f})*(1.f/1.15f);p.x=-p.x;return p+armCenter;};
 auto right=animated(local({.02f,.70f,1.10f})+Point3{.045f,-.105f,-.07f}),left=animated(local({.02f,1.10f,-1.15f})+Point3{-.085f,-.055f,-.03f});
 m_armsMesh.poseAttached(toRig(right),toRig(left),motion.elbow,-motion.pitch,-motion.yaw,game.elapsed()*2.f,game.weaponKick());
}
void SoftwareRenderer::drawViewModel(const Game& game){
 if(m_gpuFrame)m_gpu->clearDepth();
 if(game.holdingClutter()&&!m_inspectRig)return;
 std::fill(m_zbuffer.begin(),m_zbuffer.end(),std::numeric_limits<float>::infinity());
 const auto&motion=game.weaponMotion();
 Point3 gunCenter=(m_weaponMesh.minimum+m_weaponMesh.maximum)*.5f;
 Point3 gunRange=m_weaponMesh.maximum-m_weaponMesh.minimum;
 float gunScale=1.15f/std::max({gunRange.x,gunRange.y,gunRange.z});
 auto gunLocal=[&](Point3 source){auto p=(source-gunCenter)*gunScale;return Point3{p.x+.15f,p.y-.155f,-p.z+.82f};};
 const Point3 pivot{.15f,-.155f,.52f};
 auto animated=[&](Point3 p){
  p=p-pivot;
  float y=p.y*std::cos(motion.pitch)+p.z*std::sin(motion.pitch),z=-p.y*std::sin(motion.pitch)+p.z*std::cos(motion.pitch);
  return Point3{p.x*std::cos(motion.yaw)+z*std::sin(motion.yaw),y+motion.bob,-p.x*std::sin(motion.yaw)+z*std::cos(motion.yaw)-motion.back}+pivot;
 };
 // Sockets are authored in the imported gun's coordinates. Palm offsets put the
 // curled fingers around the grip while keeping the wrists outside the stock.
 const Point3 triggerSocket{.02f,.70f,1.10f},supportSocket{.02f,1.10f,-1.15f};
  auto rightWrist=animated(gunLocal(triggerSocket)+Point3{.045f,-.105f,-.07f});
  auto leftWrist=animated(gunLocal(supportSocket)+Point3{-.085f,-.055f,-.03f});
 bool fists=game.unarmed();
 Point3 armCenter=(m_armsMesh.minimum+m_armsMesh.maximum)*.5f,armOffset=fists?Point3{0,-.22f,.30f}:Point3{.15f,-.285f,.52f};
 auto fromRig=[&](Point3 rig){auto p=(rig-armCenter)*1.15f;p.x=-p.x;return p+armOffset;};
 if(!m_poseReady)prepareViewModel(game);
 auto assembly=[&](Point3 p){float roll=motion.roll;float x=p.x*std::cos(roll)-p.y*std::sin(roll),y=p.x*std::sin(roll)+p.y*std::cos(roll);
  return Point3{x,y+(fists?(game.guarding()?.08f:-.04f)+motion.bob:-game.holster()*.85f),p.z};};
 m_gripError=0;
 for(int side=0;side<2&&!fists;++side){auto p=fromRig(m_armsMesh.bonePosition(side?"hand.L":"hand.R"));
  auto d=p-(side?leftWrist:rightWrist);m_gripError=std::max(m_gripError,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));
 }
 auto draw=[&](Mesh&mesh,const Texture&texture,bool arms){
  for(auto face:mesh.triangles){
   if(!arms&&face.part==3&&(game.shotAge()<.09f||game.shotAge()>.45f))continue;
   for(auto&v:face.v){
    if(arms)v.p=fromRig(v.p);
    else{
     auto local=gunLocal(v.p);
     if(face.part==2)local.z-=motion.bolt;
     if(face.part==3){float t=game.shotAge()-.09f;local=local+Point3{t*1.8f,t*.85f-t*t*3.5f,-t*.3f};}
     v.p=animated(local);
    }
   }
   for(auto&v:face.v){v.p=assembly(v.p);if(m_inspectRig)v.p=inspectionPoint(v.p);}
   Point3 n=cross3(face.v[1].p-face.v[0].p,face.v[2].p-face.v[0].p);float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
   float light=.65f+.45f*std::fabs(n.y)/std::max(.001f,len)+(game.shotAge()<.08f?.35f*(1-game.shotAge()/.08f):0);
   triangle3D(face.v[0],face.v[1],face.v[2],texture,light);
  }
 };
 draw(m_armsMesh,m_arms,true);if(!fists)draw(m_weaponMesh,m_weaponTexture,false);
 // The source arms end in open shoulder rings. Continue those rings into dark
 // sleeves behind the camera, closing the model rather than exposing its interior.
 Texture sleeve{1,1,{0xff302c27u}};
 for(const auto&ring:m_armsMesh.openRings){
  Point3 center{};for(auto v:ring)center=center+fromRig(v.p);center=center*(1.f/float(ring.size()));
  // Only upper-arm openings are extended; small mesh boundaries are left intact.
  float radius=0;for(auto v:ring){auto d=fromRig(v.p)-center;radius=std::max(radius,std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z));}if(radius<.045f)continue;
  auto sleeveEnd=[&](Point3 p){return p+Point3{(p.x<.15f?-.08f:.08f),-.14f,-.7f};};
  auto drawSleeve=[&](Point3 a,Point3 b,Point3 c){a=assembly(a);b=assembly(b);c=assembly(c);if(m_inspectRig){a=inspectionPoint(a);b=inspectionPoint(b);c=inspectionPoint(c);}triangle3D({a,0,0},{b,0,0},{c,0,0},sleeve,.8f);};
  for(size_t i=0;i<ring.size();++i){auto a=fromRig(ring[i].p),b=fromRig(ring[(i+1)%ring.size()].p),A=sleeveEnd(a),B=sleeveEnd(b);
   if(fists)drawSleeve(center,a,b);
   else{drawSleeve(a,b,B);drawSleeve(a,B,A);drawSleeve(sleeveEnd(center),A,B);}
  }
 }
 if(game.shotAge()<.10f&&!fists&&!m_inspectRig){
  Point3 tip{};int count=0;
  for(auto&t:m_weaponMesh.triangles)for(auto&v:t.v)if(v.p.z<m_weaponMesh.minimum.z+gunRange.z*.015f){tip=tip+v.p;++count;}
  if(count){auto center=assembly(animated(gunLocal(tip*(1.f/count))));float age=game.shotAge(),size=.13f+std::sin(std::min(1.f,age/.07f)*kPi)*.15f;
   float rotation=std::floor(age*60)*1.7f;Point3 right{std::cos(rotation)*size,std::sin(rotation)*size,0},up{-std::sin(rotation)*size,std::cos(rotation)*size,0};
   float intensity=2.4f*std::max(0.f,1-age/.10f);auto a=center-right-up,b=center+right-up,c=center+right+up,d=center-right+up;
   triangle3D({a,0,1},{b,1,1},{c,1,0},m_muzzleFlash,intensity);triangle3D({a,0,1},{c,1,0},{d,0,0},m_muzzleFlash,intensity);
  }
 }
}
Point3 SoftwareRenderer::inspectionPoint(Point3 p)const{
 p=p-Point3{.12f,-.20f,.83f};float x=p.x*std::cos(m_inspectYaw)+p.z*std::sin(m_inspectYaw),z=-p.x*std::sin(m_inspectYaw)+p.z*std::cos(m_inspectYaw);
 return {x,p.y*std::cos(m_inspectPitch)-z*std::sin(m_inspectPitch),1.3f+z*std::cos(m_inspectPitch)+p.y*std::sin(m_inspectPitch)};
}
void SoftwareRenderer::inspectRig(const Game&game,float yaw,float pitch){
 clear(0xff242529);m_inspectRig=true;m_inspectYaw=yaw;m_inspectPitch=pitch;
 drawViewModel(game);
 text(10,10,"RAWMETAL / RIG INSPECTION",0xffeadcbb);
 text(10,m_height-12,game.weaponKick()>.1f?"RECOIL POSE":"IDLE POSE",0xffbdad8d);
 m_inspectRig=false;
}
}

