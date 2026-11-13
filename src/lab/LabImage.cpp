#include "lab/LabImage.h"
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace retro::lab {
bool savePng(const std::string& path,const Image& image){
 if(image.width<=0||image.height<=0)return false;
 static const bool com=SUCCEEDED(CoInitializeEx(nullptr,COINIT_MULTITHREADED))||true;(void)com;
 IWICImagingFactory* factory=nullptr;IWICStream* stream=nullptr;IWICBitmapEncoder* encoder=nullptr;IWICBitmapFrameEncode* frame=nullptr;
 bool ok=SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
 const std::wstring wide=std::filesystem::path(path).wstring();
 ok=ok&&SUCCEEDED(factory->CreateStream(&stream))&&SUCCEEDED(stream->InitializeFromFilename(wide.c_str(),GENERIC_WRITE));
 ok=ok&&SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))&&SUCCEEDED(encoder->Initialize(stream,WICBitmapEncoderNoCache));
 ok=ok&&SUCCEEDED(encoder->CreateNewFrame(&frame,nullptr))&&SUCCEEDED(frame->Initialize(nullptr))&&SUCCEEDED(frame->SetSize(UINT(image.width),UINT(image.height)));
 WICPixelFormatGUID format=GUID_WICPixelFormat24bppBGR;ok=ok&&SUCCEEDED(frame->SetPixelFormat(&format))&&format==GUID_WICPixelFormat24bppBGR;
 if(ok){const UINT stride=UINT((image.width*3+3)&~3);std::vector<BYTE> rows(size_t(stride)*image.height);
  for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){auto p=image.pixels[size_t(y)*image.width+x];BYTE* o=&rows[size_t(y)*stride+x*3];o[0]=BYTE(p);o[1]=BYTE(p>>8);o[2]=BYTE(p>>16);}
  ok=SUCCEEDED(frame->WritePixels(UINT(image.height),stride,UINT(rows.size()),rows.data()))&&SUCCEEDED(frame->Commit())&&SUCCEEDED(encoder->Commit());}
 if(frame)frame->Release();if(encoder)encoder->Release();if(stream)stream->Release();if(factory)factory->Release();
 return ok;
}
void drawLine(Image& image,float x0,float y0,float x1,float y1,std::uint32_t rgb){
 const float dx=x1-x0,dy=y1-y0;const int steps=std::clamp(int(std::max(std::fabs(dx),std::fabs(dy)))+1,1,8192);
 for(int i=0;i<=steps;++i){const float t=float(i)/steps;image.put(int(std::lround(x0+dx*t)),int(std::lround(y0+dy*t)),rgb);}
}
namespace {
// 5x7 glyphs, rows top to bottom, bit 4 the left column.
struct Glyph {char c;unsigned char rows[7];};
constexpr Glyph glyphs[]={
 {'0',{0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},{'1',{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},{'2',{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
 {'3',{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}},{'4',{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},{'5',{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
 {'6',{0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},{'7',{0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},{'8',{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
 {'9',{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},{'A',{0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},{'B',{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
 {'C',{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},{'D',{0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}},{'E',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
 {'F',{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},{'G',{0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}},{'H',{0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
 {'I',{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},{'J',{0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},{'K',{0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
 {'L',{0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},{'M',{0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},{'N',{0x11,0x11,0x19,0x15,0x13,0x11,0x11}},
 {'O',{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},{'P',{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},{'Q',{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
 {'R',{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},{'S',{0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},{'T',{0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
 {'U',{0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},{'V',{0x11,0x11,0x11,0x11,0x11,0x0A,0x04}},{'W',{0x11,0x11,0x11,0x15,0x15,0x15,0x0A}},
 {'X',{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},{'Y',{0x11,0x11,0x11,0x0A,0x04,0x04,0x04}},{'Z',{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
 {'.',{0,0,0,0,0,0x0C,0x0C}},{',',{0,0,0,0,0x0C,0x04,0x08}},{':',{0,0x0C,0x0C,0,0x0C,0x0C,0}},{'-',{0,0,0,0x1F,0,0,0}},
 {'+',{0,0x04,0x04,0x1F,0x04,0x04,0}},{'/',{0x01,0x01,0x02,0x04,0x08,0x10,0x10}},{'=',{0,0,0x1F,0,0x1F,0,0}},{'%',{0x18,0x19,0x02,0x04,0x08,0x13,0x03}},
 {'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}},{')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},{'_',{0,0,0,0,0,0,0x1F}},{'\'',{0x04,0x04,0x08,0,0,0,0}},
 {'!',{0x04,0x04,0x04,0x04,0x04,0,0x04}},{'?',{0x0E,0x11,0x01,0x02,0x04,0,0x04}},{'#',{0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}},
 {'<',{0x02,0x04,0x08,0x10,0x08,0x04,0x02}},{'>',{0x08,0x04,0x02,0x01,0x02,0x04,0x08}},{'[',{0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}},
 {']',{0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}},{'*',{0,0x04,0x15,0x0E,0x15,0x04,0}},{'@',{0x0E,0x11,0x17,0x15,0x17,0x10,0x0E}}};
const unsigned char* glyph(char c){if(c>='a'&&c<='z')c=char(c-32);for(const auto& g:glyphs)if(g.c==c)return g.rows;static const unsigned char box[7]={0x1F,0x11,0x11,0x11,0x11,0x11,0x1F};return box;}
}
int textWidth(const std::string& text,int scale){return int(text.size())*6*scale;}
void drawText(Image& image,int x,int y,const std::string& text,std::uint32_t rgb,int scale,bool shadow){
 for(int pass=shadow?0:1;pass<2;++pass){const int o=pass?0:scale;const std::uint32_t colour=pass?rgb:0;int cx=x+o;
  for(char c:text){if(c!=' '){const auto* rows=glyph(c);for(int r=0;r<7;++r)for(int b=0;b<5;++b)if(rows[r]&(0x10>>b))for(int sy=0;sy<scale;++sy)for(int sx=0;sx<scale;++sx)image.put(cx+b*scale+sx,y+o+r*scale+sy,colour);}cx+=6*scale;}}
}
void drawOverlay(Image& image,const Overlay& overlay,const std::function<Projected(Point3)>& project){
 auto screen=[&](Point3 p,bool world){if(!world)return Projected{p.x,p.y,1,true};return project(p);};
 for(const auto& l:overlay.lines){
  if(!l.world){drawLine(image,l.a.x,l.a.y,l.b.x,l.b.y,l.rgb);continue;}
  // Clip against the near plane in steps: a line half behind the eye keeps its visible half.
  Projected a=project(l.a),b=project(l.b);
  if(!a.visible&&!b.visible)continue;
  if(a.visible&&b.visible){drawLine(image,a.x,a.y,b.x,b.y,l.rgb);continue;}
  Point3 from=a.visible?l.a:l.b,to=a.visible?l.b:l.a;for(int i=0;i<12;++i){const Point3 mid=(from+to)*.5f;if(project(mid).visible)from=mid;else to=mid;}
  const Projected s=a.visible?a:b,e=project(from);if(e.visible)drawLine(image,s.x,s.y,e.x,e.y,l.rgb);}
 for(const auto& d:overlay.dots){const auto p=screen(d.p,d.world);if(!p.visible)continue;for(int y=-d.size;y<=d.size;++y)for(int x=-d.size;x<=d.size;++x)if(x*x+y*y<=d.size*d.size)image.put(int(p.x)+x,int(p.y)+y,d.rgb);}
 for(const auto& l:overlay.labels){const auto p=screen(l.p,l.world);if(!p.visible)continue;drawText(image,int(p.x)+(l.world?4:0),int(p.y)-(l.world?4:0),l.text,l.rgb);}
}
Image scaled(const Image& image,int width,int height){
 Image out(width,height);if(image.width<=0||image.height<=0)return out;
 for(int y=0;y<height;++y)for(int x=0;x<width;++x){
  const int x0=x*image.width/width,x1=std::max(x0+1,(x+1)*image.width/width),y0=y*image.height/height,y1=std::max(y0+1,(y+1)*image.height/height);
  unsigned r=0,g=0,b=0,n=0;for(int sy=y0;sy<y1;++sy)for(int sx=x0;sx<x1;++sx){auto p=image.pixels[size_t(sy)*image.width+sx];r+=(p>>16)&255;g+=(p>>8)&255;b+=p&255;++n;}
  out.at(x,y)=((r/n)<<16)|((g/n)<<8)|(b/n);}
 return out;
}
Image contactSheet(const std::vector<std::pair<std::string,const Image*>>& tiles,int columns,int tileWidth){
 if(tiles.empty())return Image(1,1);columns=std::max(1,std::min(columns,int(tiles.size())));
 const Image& first=*tiles[0].second;const int tileHeight=std::max(1,tileWidth*first.height/std::max(1,first.width)),caption=12,pad=4;
 const int rows=(int(tiles.size())+columns-1)/columns;Image sheet(pad+columns*(tileWidth+pad),pad+rows*(tileHeight+caption+pad),0x202020);
 for(size_t i=0;i<tiles.size();++i){const int cx=pad+int(i%columns)*(tileWidth+pad),cy=pad+int(i/columns)*(tileHeight+caption+pad);
  const Image t=scaled(*tiles[i].second,tileWidth,tileHeight);for(int y=0;y<tileHeight;++y)for(int x=0;x<tileWidth;++x)sheet.put(cx+x,cy+caption+y,t.pixels[size_t(y)*tileWidth+x]);
  std::string label=tiles[i].first;while(textWidth(label)>tileWidth&&label.size()>3)label.pop_back();drawText(sheet,cx,cy+2,label,0xe0e0e0,1,false);}
 return sheet;
}
std::uint64_t hashPixels(const std::uint32_t* pixels,size_t count){std::uint64_t h=1469598103934665603ull;for(size_t i=0;i<count;++i){h^=pixels[i]&0xffffffu;h*=1099511628211ull;}return h;}
Difference difference(const Image& a,const Image& b,int threshold){
 Difference d;if(a.width!=b.width||a.height!=b.height||a.pixels.empty())return {255,1};double sum=0;size_t moved=0;
 for(size_t i=0;i<a.pixels.size();++i){const auto p=a.pixels[i],q=b.pixels[i];int m=0;for(int s=0;s<24;s+=8){const int c=std::abs(int((p>>s)&255)-int((q>>s)&255));sum+=c;m=std::max(m,c);}moved+=m>threshold;}
 d.mean=sum/(3.0*a.pixels.size());d.changed=double(moved)/a.pixels.size();return d;
}
}
