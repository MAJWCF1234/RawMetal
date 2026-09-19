#include "SoftwareRenderer.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace retro {
static Point3 unitNormal(Point3 n){float length=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);return length>.00001f?n*(1/length):Point3{0,0,1};}
void SoftwareRenderer::attachNormal(Texture& texture,int resource,bool greenUp){
 auto source=loadTexture(resource);
 if(source.width!=texture.width||source.height!=texture.height)throw std::runtime_error("Normal map dimensions do not match the color map");
 prepareDecal(texture,false);texture.normalLevels.clear();std::vector<Point3> normals;normals.reserve(source.pixels.size());
 for(auto pixel:source.pixels){float x=std::clamp((int((pixel>>16)&255)-128)/127.f,-1.f,1.f),y=std::clamp((int((pixel>>8)&255)-128)/127.f,-1.f,1.f),z=(pixel&255)/127.5f-1.f;
  // Authored green-up normals use -V because image sampling runs downwards.
  normals.push_back(unitNormal({x,greenUp?-y:y,z}));
 }
 texture.normalLevels.push_back(std::move(normals));int width=texture.width,height=texture.height;
 while(width>1||height>1){int nw=std::max(1,width/2),nh=std::max(1,height/2);std::vector<Point3> next(size_t(nw*nh));auto&previous=texture.normalLevels.back();
  for(int y=0;y<nh;++y)for(int x=0;x<nw;++x){Point3 sum{};for(int j=0;j<2;++j)for(int i=0;i<2;++i)sum=sum+previous[std::min(height-1,y*2+j)*width+std::min(width-1,x*2+i)];next[y*nw+x]=unitNormal(sum);}
  texture.normalLevels.push_back(std::move(next));width=nw;height=nh;
 }
}
Point3 SoftwareRenderer::sampleNormal(const Texture& texture,float u,float v,float lod){
 if(texture.normalLevels.empty())return {0,0,1};u-=std::floor(u);v-=std::floor(v);
 lod=std::clamp(lod,0.f,float(texture.normalLevels.size()-1));int level=int(lod);
 auto fetch=[&](int at){int width=std::max(1,texture.width>>at),height=std::max(1,texture.height>>at);return texture.normalLevels[at][std::min(height-1,int(v*height))*width+std::min(width-1,int(u*width))];};
 auto a=fetch(level);if(size_t(level+1)==texture.normalLevels.size())return a;float blend=lod-level;return a*(1-blend)+fetch(level+1)*blend;
}
bool SoftwareRenderer::testNormalMapping(){
 std::ofstream report("normal-mapping-test.txt");
 for(auto* texture:{&m_wall,&m_pressureWall,&m_bulkhead,&m_floor}){
  if(texture->normalLevels.size()!=texture->mips.size()+1)return false;
  for(auto n:texture->normalLevels[0])if(!std::isfinite(n.x+n.y+n.z)||std::fabs(n.x*n.x+n.y*n.y+n.z*n.z-1)>.001f)return false;
 }
 Texture material{16,16,std::vector<std::uint32_t>(256,0xff888888)};material.normalLevels={{}};
 for(int y=0;y<16;++y)for(int x=0;x<16;++x)material.normalLevels[0].push_back({x<8?.6f:-.6f,0,.8f});
 auto render=[&](Point3 direction){clear(0);std::fill(m_zbuffer.begin(),m_zbuffer.end(),9999.f);NormalLighting lighting;lighting.directions[0]=direction;lighting.weights[0]=1;
  triangle3D({{-1,-1,2},0,1},{{1,-1,2},1,1},{{1,1,2},1,0},material,1,&lighting);
  triangle3D({{-1,-1,2},0,1},{{1,1,2},1,0},{{-1,1,2},0,0},material,1,&lighting);return m_pixels;
 };
 auto left=render({-.8f,0,-.6f}),right=render({.8f,0,-.6f});size_t a=size_t((m_height/2)*m_width+m_width/2-60),b=a+120;
 int dl=int(left[a]&255)-int(right[a]&255),dr=int(left[b]&255)-int(right[b]&255);
 if(std::abs(dl)<10||std::abs(dr)<10||dl*dr>=0)return false;
 for(auto&n:material.normalLevels[0])n={0,0,1};left=render({-.8f,0,-.6f});right=render({.8f,0,-.6f});
 if(left!=right)return false;
 report<<"Authored matching maps, unit normal decoding, mip chains, opposed light directions producing opposed relief, neutral maps preserving base shading: PASS\n";return true;
}
}
