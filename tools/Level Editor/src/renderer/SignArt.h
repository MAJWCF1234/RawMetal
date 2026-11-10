#pragma once
// Sign artwork: the lettered plates a SIGN record hangs in the world, drawn by
// one rasteriser that both the engine (SoftwareRenderer::makeSign, Scene3D)
// and the campaign builder's Sign designer use, so the designer's preview is
// the very pixels the game puts on the wall. Header-only: the stand-alone
// builder (RawMetalEditor.exe) includes it without the renderer.
//
// A design is a style (the plate it is painted on), optional symbol, optional
// picture (a SIGN_ART record), the words and two colours. Style 0 with no
// symbol, ink or picture is the facility's original enamel plate, drawn
// pixel for pixel as it always was (512 x 144) so existing maps do not change.
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace retro::signart {

// ---- Type: installed Windows faces rasterised once per size into coverage.
struct Glyph{int advance=0;std::vector<std::uint8_t> alpha;};
struct Face{int width=0,height=0;std::array<Glyph,96> glyphs;};
// 0-3 are the UI's faces; 4 is a hand (chalk and marker); 5 a heavy stencil-like face.
inline constexpr int FaceCount=6;
inline const Face& face(int which,int size){
 static std::map<int,Face> faces;static std::mutex guard;std::lock_guard lock(guard);
 which=std::clamp(which,0,FaceCount-1);
 if(auto found=faces.find(which*1024+size);found!=faces.end())return found->second;
 auto& out=faces[which*1024+size];
 static const wchar_t* names[FaceCount]={L"Bahnschrift SemiBold SemiConden",L"Bahnschrift Light SemiCondensed",L"Impact",L"Bahnschrift SemiCondensed",L"Ink Free",L"Bahnschrift Bold Condensed"};
 HDC dc=CreateCompatibleDC(nullptr);
 HFONT font=CreateFontW(-size,0,0,0,which==5?FW_BOLD:FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_TT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,names[which]);
 auto oldFont=SelectObject(dc,font);TEXTMETRICW metrics{};GetTextMetricsW(dc,&metrics);
 out.height=metrics.tmHeight;out.width=size*2+8;
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=out.width;info.bmiHeader.biHeight=-out.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
 void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto oldBitmap=SelectObject(dc,bitmap);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
 for(int c=32;c<128;++c){
  std::memset(bits,0,size_t(out.width)*out.height*4);wchar_t ch=wchar_t(c);
  SIZE extent{};GetTextExtentPoint32W(dc,&ch,1,&extent);TextOutW(dc,4,0,&ch,1);GdiFlush();
  auto& glyph=out.glyphs[size_t(c-32)];glyph.advance=extent.cx;glyph.alpha.resize(size_t(out.width)*out.height);
  auto* pixels=static_cast<const std::uint32_t*>(bits);for(size_t i=0;i<glyph.alpha.size();++i)glyph.alpha[i]=std::uint8_t((pixels[i]>>8)&255);
 }
 SelectObject(dc,oldBitmap);SelectObject(dc,oldFont);DeleteObject(bitmap);DeleteObject(font);DeleteDC(dc);
 return out;
}
inline unsigned glyphIndex(char c){unsigned v=(unsigned char)c;return v<32||v>127?'?'-32:v-32;}
inline int textWidth(std::string_view s,int which,int size,int tracking){auto&f=face(which,size);int w=0;for(char c:s)w+=f.glyphs[glyphIndex(c)].advance+tracking;return w;}

// ---- The design.
enum Style{Plate,Hazard,Notice,Stencil,Route,Chalk,Picture,StyleCount};
enum Icon{NoIcon,Warning,Electric,ArrowLeft,ArrowRight,ArrowUp,ArrowDown,NoEntry,Radiation,Fire,Exit,IconCount};
inline constexpr const char* styleNames[StyleCount]={"Enamel plate","Hazard","Notice","Stencil (painted on)","Route","Chalkboard","Picture"};
inline constexpr const char* iconNames[IconCount]={"No symbol","Warning","Electric","Arrow left","Arrow right","Arrow up","Arrow down","No entry","Radiation","Fire","Exit"};
// Pictures: ARGB, at most 256 on a side and 65536 pixels (SIGN_ART records).
inline constexpr int ArtMaxSide=256,ArtMaxPixels=65536;
struct Image{int width=0,height=0;std::vector<std::uint32_t> pixels;};
struct Design{
 std::string title,subtitle;
 std::uint32_t accent=0xffca994du; // the safety colour: band, header, route field, stencil paint
 int style=Plate,icon=NoIcon;
 std::uint32_t ink=0;               // lettering colour; 0 is the style's own
 const Image* art=nullptr;          // a picture (Picture style) or a logo in the symbol slot
};
struct Raster{int width=0,height=0;std::vector<std::uint32_t> pixels;bool transparent=false;};
inline bool legacy(const Design& d){return d.style==Plate&&d.icon==NoIcon&&!d.ink&&!d.art;}
// Texture size for a sign this many metres wide per metre high.
inline void rasterSize(const Design& d,float aspect,int& w,int& h){
 if(legacy(d)||!(aspect>0)){w=512;h=144;return;}
 if(aspect>=1){w=512;h=std::clamp(int(std::lround(512/aspect)),48,512);}
 else{h=512;w=std::clamp(int(std::lround(512*aspect)),48,512);}
}

namespace detail {
inline std::uint32_t rgb(int r,int g,int b){return 0xff000000u|(std::uint32_t(r)<<16)|(std::uint32_t(g)<<8)|std::uint32_t(b);}
inline float channel(std::uint32_t c,int shift){return float((c>>shift)&255);}
inline std::uint32_t mix(std::uint32_t a,std::uint32_t b,float t){t=std::clamp(t,0.f,1.f);
 return rgb(int(channel(a,16)+(channel(b,16)-channel(a,16))*t),int(channel(a,8)+(channel(b,8)-channel(a,8))*t),int(channel(a,0)+(channel(b,0)-channel(a,0))*t));}
inline float hash(int x,int y,unsigned seed){unsigned h=unsigned(x)*374761393u+unsigned(y)*668265263u+seed*2246822519u;h=(h^(h>>13))*1274126177u;return float((h^(h>>16))&0xffffu)/65535.f;}
inline float noise(float x,float y,unsigned seed){int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;fx=fx*fx*(3-2*fx);fy=fy*fy*(3-2*fy);
 float a=hash(ix,iy,seed),b=hash(ix+1,iy,seed),c=hash(ix,iy+1,seed),d=hash(ix+1,iy+1,seed);return a+(b-a)*fx+(c-a)*fy+(a-b-c+d)*fx*fy;}
inline float luma(std::uint32_t c){return (channel(c,16)*.299f+channel(c,8)*.587f+channel(c,0)*.114f)/255.f;}
struct Pt{float x,y;};
inline bool inside(const Pt* p,int n,float x,float y){bool in=false;for(int i=0,j=n-1;i<n;j=i++)if((p[i].y>y)!=(p[j].y>y)&&x<(p[j].x-p[i].x)*(y-p[i].y)/(p[j].y-p[i].y)+p[i].x)in=!in;return in;}
inline bool circle(float x,float y,float cx,float cy,float r){return (x-cx)*(x-cx)+(y-cy)*(y-cy)<=r*r;}
// One sample of a symbol in its unit box (v down). Colours: 0 nothing, 1 the
// symbol's ground (sign yellow, red disc...), 2 the mark itself.
enum Paint{Clear,Ground,Mark};
struct IconSample{Paint paint=Clear;std::uint32_t colour=0;};
inline IconSample iconAt(int icon,float u,float v,std::uint32_t markColour){
 const std::uint32_t yellow=rgb(242,188,36),black=rgb(18,17,16),red=rgb(196,34,28),white=rgb(240,236,226),green=rgb(28,128,72);
 auto triangle=[&](float inset){Pt t[3]={{.5f,.06f+inset*1.6f},{.96f-inset*1.4f,.9f-inset*.8f},{.04f+inset*1.4f,.9f-inset*.8f}};return inside(t,3,u,v);};
 switch(icon){
 case Warning:case Electric:case Fire:{
  if(!triangle(0))return {};
  if(!triangle(.075f))return {Mark,black};
  bool mark=false;
  if(icon==Warning){Pt bar[4]={{.46f,.32f},{.54f,.32f},{.525f,.64f},{.475f,.64f}};mark=inside(bar,4,u,v)||circle(u,v,.5f,.74f,.045f);}
  else if(icon==Electric){Pt bolt[6]={{.56f,.28f},{.40f,.56f},{.50f,.56f},{.43f,.82f},{.62f,.50f},{.51f,.50f}};mark=inside(bolt,6,u,v);}
  else{Pt tip[3]={{.5f,.30f},{.62f,.62f},{.38f,.62f}};mark=circle(u,v,.5f,.66f,.13f)||inside(tip,3,u,v);
   if(mark&&(circle(u,v,.5f,.71f,.06f)))mark=false;}
  return mark?IconSample{Mark,black}:IconSample{Ground,yellow};}
 case ArrowLeft:case ArrowRight:case ArrowUp:case ArrowDown:{
  float x=u,y=v;if(icon==ArrowLeft)x=1-u;if(icon==ArrowUp)x=1-v,y=u;if(icon==ArrowDown)x=v,y=u;
  Pt arrow[7]={{.08f,.40f},{.52f,.40f},{.52f,.16f},{.94f,.5f},{.52f,.84f},{.52f,.60f},{.08f,.60f}};
  return inside(arrow,7,x,y)?IconSample{Mark,markColour}:IconSample{};}
 case NoEntry:
  if(!circle(u,v,.5f,.5f,.46f))return {};
  if(u>.2f&&u<.8f&&v>.42f&&v<.58f)return {Mark,white};
  return {Ground,red};
 case Radiation:{
  if(!circle(u,v,.5f,.5f,.46f))return {};
  float dx=u-.5f,dy=v-.5f,r=std::sqrt(dx*dx+dy*dy);
  if(r<.07f)return {Mark,black};
  if(r>.11f&&r<.38f){float a=std::atan2(dy,dx)*180.f/3.14159265f+90.f;a=std::fmod(a+720.f,120.f);if(a<60.f)return {Mark,black};}
  return {Ground,yellow};}
 case Exit:{
  if(u<.04f||u>.96f||v<.1f||v>.9f)return {};
  bool door=(u>.14f&&u<.48f&&v>.2f&&v<.82f)&&!(u>.19f&&u<.43f&&v>.26f&&v<.82f);
  Pt arrow[7]={{.5f,.46f},{.7f,.46f},{.7f,.34f},{.9f,.51f},{.7f,.68f},{.7f,.56f},{.5f,.56f}};
  return door||inside(arrow,7,u,v)?IconSample{Mark,white}:IconSample{Ground,green};}
 default:return {};
 }
}
}

// Draws a symbol (or a picture fitted into it) into a square box of the raster.
// recolour: draw every mark and ground in one colour (stencil paint, chalk).
inline void drawSymbol(Raster& r,int icon,const Image* art,int x0,int y0,int size,std::uint32_t markColour,bool recolour,float wear,unsigned seed){
 using namespace detail;
 if(size<4)return;
 for(int y=std::max(0,y0);y<std::min(r.height,y0+size);++y)for(int x=std::max(0,x0);x<std::min(r.width,x0+size);++x){
  float acc[3]{},cover=0;
  if(art&&art->width>0&&art->height>0){
   // Contain-fit, centred, bilinear-free nearest with 2x2 coverage.
   float scale=std::min(size/float(art->width),size/float(art->height)),w=art->width*scale,h=art->height*scale,ox=x0+(size-w)*.5f,oy=y0+(size-h)*.5f;
   for(int j=0;j<2;++j)for(int i=0;i<2;++i){float sx=(x+.25f+.5f*i-ox)/scale,sy=(y+.25f+.5f*j-oy)/scale;if(sx<0||sy<0||sx>=art->width||sy>=art->height)continue;
    auto p=art->pixels[size_t(int(sy))*art->width+int(sx)];float a=float(p>>24)/255.f;if(recolour)p=markColour;
    acc[0]+=channel(p,16)*a;acc[1]+=channel(p,8)*a;acc[2]+=channel(p,0)*a;cover+=a;}
  }else{
   for(int j=0;j<4;++j)for(int i=0;i<4;++i){auto s=iconAt(icon,(x-x0+.125f+.25f*i)/size,(y-y0+.125f+.25f*j)/size,markColour);if(s.paint==Clear)continue;
    // Painted in one colour: the symbol's dominant shape only (a disc, a door; a triangle's marks).
    if(recolour&&s.paint!=(icon==NoEntry||icon==Radiation||icon==Exit?Ground:Mark))continue;
    auto c=recolour?markColour:s.colour;acc[0]+=channel(c,16);acc[1]+=channel(c,8);acc[2]+=channel(c,0);cover+=1;}
   cover/=16;for(float& a:acc)a/=16;
  }
  if(art&&art->width>0){cover/=4;for(float& a:acc)a/=4;}
  if(cover<=0)continue;
  std::uint32_t c=rgb(int(std::min(255.f,acc[0]/cover)),int(std::min(255.f,acc[1]/cover)),int(std::min(255.f,acc[2]/cover)));
  float a=cover*(1.f-std::max(0.f,noise(x*.3f,y*.3f,seed+29)-wear)*3.f);a=std::clamp(a,0.f,1.f);
  auto& d=r.pixels[size_t(y)*r.width+x];
  if(r.transparent){float below=float(d>>24)/255.f*(1-a),out=a+below;
   auto ch=[&](int sh){return std::uint32_t(std::clamp((channel(c,sh)*a+channel(d,sh)*below)/std::max(out,1e-4f),0.f,255.f));};
   d=(std::uint32_t(out*255.f+.5f)<<24)|(ch(16)<<16)|(ch(8)<<8)|ch(0);}
  else d=mix(d,c,a);
 }
}

inline Raster draw(const Design& d,float aspect){
 using namespace detail;
 int W=512,H=144;rasterSize(d,aspect,W,H);
 Raster sign{W,H,std::vector<std::uint32_t>(size_t(W)*H)};
 const char* title=d.title.c_str();const char* subtitle=d.subtitle.c_str();
 const std::uint32_t accent=d.accent|0xff000000u;
 unsigned seed=0;for(const char* c=title;*c;++c)seed=seed*31u+unsigned(char(*c));
 // Lettering blended over the ground; worn where the enamel chipped.
 auto letter=[&](const char* text,int faceIndex,int size,int x,int y,std::uint32_t colour,int tracking,float wear,float grain){
  const auto& f=face(faceIndex,size);int pen=x;
  for(const char* c=text;*c;++c){const auto& g=f.glyphs[glyphIndex(*c)];
   for(int yy=0;yy<f.height;++yy)for(int xx=0;xx<f.width;++xx){int px=pen-4+xx,py=y+yy;if(px<0||py<0||px>=W||py>=H)continue;
    float a=g.alpha[size_t(yy)*f.width+xx]/255.f;if(a<=0)continue;
    a*=1.f-std::max(0.f,noise(px*.3f,py*.3f,seed+13)-wear)*3.f;
    if(grain>0)a*=1.f-grain*hash(px,py,seed+41);
    auto& p=sign.pixels[size_t(py)*W+px];
    if(sign.transparent){a=std::clamp(a,0.f,1.f);float below=float(p>>24)/255.f*(1-a),out=a+below;
     auto ch=[&](int sh){return std::uint32_t(std::clamp((channel(colour,sh)*a+channel(p,sh)*below)/std::max(out,1e-4f),0.f,255.f));};
     p=(std::uint32_t(out*255.f+.5f)<<24)|(ch(16)<<16)|(ch(8)<<8)|ch(0);}
    else p=mix(p,colour,a);}
   pen+=g.advance+tracking;}
 };
 // Largest even size (from start down to least) whose width fits.
 // Measured once at 64 px and scaled, then checked, so only a size or two is ever rasterised.
 auto fit=[&](const char* text,int faceIndex,int start,int least,int width,int step){
  start=std::min(start,128);int n=int(std::strlen(text)),w64=textWidth(text,faceIndex,64,0);int s=start;
  if(w64>0&&w64*s/64+2*n>width)s=std::max(least,(width-2*n)*64/w64);if(step==2)s&=~1;
  while(s>least&&textWidth(text,faceIndex,s,2)>width)s-=step;return std::max(s,least);};
 if(legacy(d)){
  // The original enamelled graphite plate: bevelled rim, corner rivets, a
  // safety-colour band, auto-fitted condensed lettering, chipping and grime.
  const std::uint32_t plate=rgb(36,39,42),plateHi=rgb(52,56,60),rim=rgb(18,19,21),bevel=rgb(88,92,96);
  const int band=30;
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){
   int edge=std::min({x,y,W-1-x,H-1-y});
   float grain=noise(x*.9f,y*.06f,seed)*.06f+noise(x*.05f,y*.05f,seed+7)*.08f;
   std::uint32_t c=mix(plate,plateHi,(1.f-y/float(H))*.55f+grain);
   if(x>=6&&x<6+band&&edge>=6)c=mix(accent,rgb(0,0,0),.08f+grain);
   if(x==6+band||x==7+band)c=rim;
   if(edge<6)c=edge<2?rim:edge<4?mix(bevel,plate,y/float(H)):plateHi;
   float grime=std::clamp((y/float(H)-.6f)*2.2f,0.f,1.f)*noise(x*.08f,y*.2f,seed+3);
   c=mix(c,rgb(20,17,14),grime*.45f);
   float chip=noise(x*.22f,y*.22f,seed+11);
   if(edge<14&&chip>.86f-edge*.004f)c=mix(rgb(96,70,48),rgb(60,44,32),noise(x*.5f,y*.5f,seed+5));
   sign.pixels[size_t(y)*W+x]=c;
  }
  const int textLeft=6+band+22,textWidthMax=W-textLeft-22;
  int titleSize=64;while(titleSize>24&&textWidth(title,0,titleSize,2)>textWidthMax)titleSize-=2;
  int subtitleSize=26;while(subtitleSize>14&&textWidth(subtitle,0,subtitleSize,2)>textWidthMax)subtitleSize-=1;
  const auto& titleFace=face(0,titleSize);
  int titleY=std::max(8,(H-titleFace.height-face(0,subtitleSize).height)/2-6);
  letter(title,0,titleSize,textLeft,titleY,rgb(236,232,222),2,.8f,0);
  letter(subtitle,0,subtitleSize,textLeft+2,titleY+titleFace.height-4,mix(accent,rgb(236,232,222),.35f),2,.8f,0);
  for(int rx:{14+band,W-14})for(int ry:{14,H-15})for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){
   float dd=std::sqrt(float(dx*dx+dy*dy));if(dd>4.2f)continue;
   sign.pixels[size_t(ry+dy)*W+rx+dx]=mix(rgb(120,124,128),rgb(30,32,35),(dx+dy+6)/12.f*(dd/4.2f)*1.4f);
  }
  return sign;
 }
 // ---- Designed signs. Everything scales with the short side.
 const int S=std::min(W,H);const float k=S/144.f;
 auto px=[&](float v){return std::max(1,int(std::lround(v*k)));};
 const bool tall=H>W;
 auto rivets=[&](int inset,int radius){for(int rx:{inset,W-1-inset})for(int ry:{inset,H-1-inset})for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
  float dd=std::sqrt(float(dx*dx+dy*dy));if(dd>radius+.2f)continue;int X=rx+dx,Y=ry+dy;if(X<0||Y<0||X>=W||Y>=H)continue;
  sign.pixels[size_t(Y)*W+X]=mix(rgb(150,152,150),rgb(30,32,35),(dx+dy+radius*1.5f)/(radius*3.f)*(dd/(radius+.2f))*1.4f);}};
 // Enamel ground with a rim, grime and chips (every metal plate style).
 auto enamel=[&](std::uint32_t lo,std::uint32_t hi,int rimWidth,float chipping){
  const std::uint32_t rim=rgb(18,19,21);
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){int edge=std::min({x,y,W-1-x,H-1-y});
   float grain=noise(x*.9f/k,y*.06f/k,seed)*.06f+noise(x*.05f/k,y*.05f/k,seed+7)*.08f;
   std::uint32_t c=mix(lo,hi,(1.f-y/float(H))*.55f+grain);
   if(edge<rimWidth)c=edge<rimWidth/3?rim:mix(hi,rgb(255,255,255),.08f);
   float grime=std::clamp((y/float(H)-.6f)*2.2f,0.f,1.f)*noise(x*.08f/k,y*.2f/k,seed+3);c=mix(c,rgb(20,17,14),grime*.4f);
   float chip=noise(x*.22f/k,y*.22f/k,seed+11);
   if(edge<px(14)&&chip>(.9f-edge*.004f/k)/chipping)c=mix(rgb(96,70,48),rgb(60,44,32),noise(x*.5f,y*.5f,seed+5));
   sign.pixels[size_t(y)*W+x]=c;}};
 const std::uint32_t paper=rgb(234,229,216),dark=rgb(22,21,20),white=rgb(238,234,224);
 int contentL=px(16),contentR=W-px(16),contentT=px(12),contentB=H-px(12);
 std::uint32_t titleInk=white,subInk=mix(accent,white,.35f);int titleFace=0,subFace=0;float wear=.8f,grain=0;
 bool symbolRight=d.icon==ArrowRight;
 std::uint32_t markColour=white;bool recolour=false;
 switch(d.style){
 case Hazard:{
  enamel(rgb(226,170,30),rgb(246,196,52),px(5),1.f);
  // Black and yellow chevron border.
  int b=px(13);for(int y=0;y<H;++y)for(int x=0;x<W;++x){int edge=std::min({x,y,W-1-x,H-1-y});if(edge<px(5)||edge>=b)continue;
   if(((x+y)/px(16))%2==0)sign.pixels[size_t(y)*W+x]=mix(rgb(22,20,18),sign.pixels[size_t(y)*W+x],.12f*noise(x*.2f,y*.2f,seed+2));}
  contentL=b+px(10);contentR=W-b-px(10);contentT=b+px(6);contentB=H-b-px(6);
  titleInk=dark;subInk=rgb(40,36,30);titleFace=0;markColour=dark;break;}
 case Notice:{
  enamel(rgb(222,218,206),paper,px(5),.8f);
  int header=tall?int(H*.22f):int(H*.46f);
  for(int y=px(5);y<header;++y)for(int x=px(5);x<W-px(5);++x){auto& p=sign.pixels[size_t(y)*W+x];p=mix(accent,mix(p,dark,.3f),.08f+noise(x*.9f/k,y*.06f/k,seed)*.08f);}
  for(int x=px(5);x<W-px(5);++x)for(int t=0;t<px(2);++t)sign.pixels[size_t(header+t)*W+x]=dark;
  // The title sits in the header; the small words in the body.
  int tw=W-px(32);int ts=fit(title,0,int(header*.78f),12,tw,2);const auto& tf=face(0,ts);
  letter(title,0,ts,(W-textWidth(title,0,ts,2))/2,std::max(px(4),(header-tf.height)/2+px(2)),d.ink?d.ink|0xff000000u:white,2,.82f,0);
  contentT=header+px(10);titleInk=0;subInk=d.ink?d.ink|0xff000000u:dark;markColour=dark;
  rivets(px(14),px(4));break;}
 case Stencil:{
  sign.transparent=true;std::fill(sign.pixels.begin(),sign.pixels.end(),accent&0x00ffffffu);
  titleFace=5;subFace=5;titleInk=d.ink?d.ink|0xff000000u:accent;subInk=titleInk;wear=.62f;grain=.25f;markColour=titleInk;recolour=true;
  contentL=px(6);contentR=W-px(6);contentT=px(6);contentB=H-px(6);break;}
 case Route:{
  enamel(mix(accent,rgb(0,0,0),.2f),accent,px(5),.6f);
  int line=px(9);for(int y=0;y<H;++y)for(int x=0;x<W;++x){int edge=std::min({x,y,W-1-x,H-1-y});if(edge>=line&&edge<line+px(3))sign.pixels[size_t(y)*W+x]=mix(white,sign.pixels[size_t(y)*W+x],.15f);}
  contentL=line+px(14);contentR=W-line-px(14);titleInk=d.ink?d.ink|0xff000000u:white;subInk=mix(titleInk,accent,.25f);markColour=titleInk;break;}
 case Chalk:{
  int frame=px(11);
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){int edge=std::min({x,y,W-1-x,H-1-y});std::uint32_t c;
   if(edge<frame){float g=noise(x*(y<frame||y>=H-frame?.05f:1.5f)/k,y*(y<frame||y>=H-frame?1.5f:.05f)/k,seed+9);c=mix(rgb(78,52,30),rgb(126,88,52),g);if(edge<2)c=rgb(40,26,16);}
   else{float smudge=noise(x*.02f/k,y*.04f/k,seed+17);c=mix(rgb(30,40,36),rgb(52,64,58),smudge*.7f+hash(x,y,seed)*.06f);}
   sign.pixels[size_t(y)*W+x]=c;}
  contentL=frame+px(12);contentR=W-frame-px(12);contentT=frame+px(4);contentB=H-frame-px(4);
  titleFace=4;subFace=4;titleInk=d.ink?d.ink|0xff000000u:rgb(232,232,224);subInk=mix(titleInk,accent,.45f);wear=.7f;grain=.35f;markColour=titleInk;recolour=true;break;}
 case Picture:{
  enamel(rgb(30,32,34),rgb(44,46,50),px(4),.7f);
  int inset=px(6),caption=*title||*subtitle?int(H*(tall?.16f:.26f)):0;
  int ax=inset,ay=inset,aw=W-inset*2,ah=H-inset*2-caption;
  if(d.art&&d.art->width>0&&d.art->height>0){
   // Cover-fit: the picture fills its window, trimmed rather than letterboxed.
   float scale=std::max(aw/float(d.art->width),ah/float(d.art->height));float ox=ax+(aw-d.art->width*scale)*.5f,oy=ay+(ah-d.art->height*scale)*.5f;
   for(int y=ay;y<ay+ah;++y)for(int x=ax;x<ax+aw;++x){
    float sx=(x+.5f-ox)/scale-.5f,sy=(y+.5f-oy)/scale-.5f;int x0=std::clamp(int(std::floor(sx)),0,d.art->width-1),y0=std::clamp(int(std::floor(sy)),0,d.art->height-1);
    int x1=std::min(x0+1,d.art->width-1),y1=std::min(y0+1,d.art->height-1);float fx=std::clamp(sx-x0,0.f,1.f),fy=std::clamp(sy-y0,0.f,1.f);
    auto at=[&](int X,int Y){return d.art->pixels[size_t(Y)*d.art->width+X];};
    auto top=mix(at(x0,y0),at(x1,y0),fx),bottom=mix(at(x0,y1),at(x1,y1),fx);auto c=mix(top,bottom,fy);
    float a=channel(at(x0,y0),24)/255.f;auto& p=sign.pixels[size_t(y)*W+x];
    // Printed onto the plate: a little of the enamel's wear shows through.
    float wearMark=std::clamp((noise(x*.1f/k,y*.1f/k,seed+31)-.7f)*2.f,0.f,.35f);
    p=mix(p,mix(c,rgb(30,28,26),wearMark),a);}
  }else for(int y=ay;y<ay+ah;++y)for(int x=ax;x<ax+aw;++x)sign.pixels[size_t(y)*W+x]=((x/px(12)+y/px(12))&1)?rgb(70,72,76):rgb(58,60,64);
  contentT=H-inset-caption;contentB=H-inset;contentL=inset+px(10);contentR=W-inset-px(10);titleInk=d.ink?d.ink|0xff000000u:white;subInk=mix(titleInk,accent,.4f);
  rivets(px(8),px(3));
  if(caption<=0)return sign;
  break;}
 default:{ // Enamel plate with extras (a symbol, a picture or its own ink): the original look.
  const int band=px(30);
  enamel(rgb(36,39,42),rgb(52,56,60),px(6),1.f);
  for(int y=px(6);y<H-px(6);++y)for(int x=px(6);x<px(6)+band;++x){auto& p=sign.pixels[size_t(y)*W+x];p=mix(accent,rgb(0,0,0),.08f+noise(x*.9f/k,y*.06f/k,seed)*.06f);}
  for(int y=px(6);y<H-px(6);++y)for(int t=0;t<px(2);++t)sign.pixels[size_t(y)*W+px(6)+band+t]=rgb(18,19,21);
  contentL=px(6)+band+px(16);titleInk=d.ink?d.ink|0xff000000u:white;subInk=mix(accent,white,.35f);markColour=white;
  rivets(px(14),px(4));
  break;}
 }
 // Symbol slot: a square at the left (right for a right arrow), or on top of a tall sign.
 const bool symbol=(d.icon!=NoIcon||(d.art&&d.style!=Picture));
 if(symbol){
  if(tall){int size=std::min(contentR-contentL,(contentB-contentT)/2);int x=(W-size)/2;drawSymbol(sign,d.icon,d.art,x,contentT,size,markColour,recolour,wear,seed);contentT+=size+px(6);}
  else{int size=std::min(contentB-contentT,(contentR-contentL)/3);int y=contentT+(contentB-contentT-size)/2;
   if(symbolRight){drawSymbol(sign,d.icon,d.art,contentR-size,y,size,markColour,recolour,wear,seed);contentR-=size+px(12);}
   else{drawSymbol(sign,d.icon,d.art,contentL,y,size,markColour,recolour,wear,seed);contentL+=size+px(12);}}
 }
 // Words: the title as large as fits, the small words beneath.
 const int width=std::max(8,contentR-contentL),height=std::max(8,contentB-contentT);
 const bool hasTitle=titleInk&&*title,hasSub=*subtitle;
 int titleSize=hasTitle?fit(title,titleFace,std::max(12,int(height*(hasSub?.62f:.8f))),10,width,2):0;
 int subSize=hasSub?fit(subtitle,subFace,std::max(10,int(height*(hasTitle?.24f:.5f))),8,width,1):0;
 int th=hasTitle?face(titleFace,titleSize).height:0,sh=hasSub?face(subFace,subSize).height:0;
 int block=th+sh-(hasTitle&&hasSub?px(4):0),y=contentT+std::max(0,(height-block)/2);
 bool centred=d.style==Notice||d.style==Stencil||d.style==Chalk||d.style==Picture||tall;
 if(hasTitle){int x=centred?contentL+(width-textWidth(title,titleFace,titleSize,2))/2:contentL;letter(title,titleFace,titleSize,x,y,titleInk,2,wear,grain);y+=th-px(4);}
 if(hasSub){int x=centred?contentL+(width-textWidth(subtitle,subFace,subSize,2))/2:contentL+2;letter(subtitle,subFace,subSize,x,y,subInk,2,wear,grain);}
 return sign;
}

// ---- Pictures in campaign files: SIGN_ART|name|width|height|base64 of RGBA bytes.
inline std::string base64(const std::vector<std::uint32_t>& argb){
 static const char* table="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 std::vector<unsigned char> bytes;bytes.reserve(argb.size()*4);
 for(auto p:argb){bytes.push_back((p>>16)&255);bytes.push_back((p>>8)&255);bytes.push_back(p&255);bytes.push_back((p>>24)&255);}
 std::string out;out.reserve((bytes.size()+2)/3*4);
 for(size_t i=0;i<bytes.size();i+=3){unsigned v=unsigned(bytes[i])<<16;if(i+1<bytes.size())v|=unsigned(bytes[i+1])<<8;if(i+2<bytes.size())v|=bytes[i+2];
  out.push_back(table[(v>>18)&63]);out.push_back(table[(v>>12)&63]);out.push_back(i+1<bytes.size()?table[(v>>6)&63]:'=');out.push_back(i+2<bytes.size()?table[v&63]:'=');}
 return out;
}
inline bool unbase64(std::string_view text,int width,int height,Image& out){
 if(width<1||height<1||width>ArtMaxSide||height>ArtMaxSide||width*height>ArtMaxPixels)return false;
 auto value=[](char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
 std::vector<unsigned char> bytes;bytes.reserve(text.size()*3/4);unsigned acc=0;int bits=0;
 for(char c:text){if(c=='='||c=='\r')break;int v=value(c);if(v<0)return false;acc=(acc<<6)|unsigned(v);bits+=6;if(bits>=8){bits-=8;bytes.push_back((acc>>bits)&255);}}
 if(bytes.size()!=size_t(width)*height*4)return false;
 out.width=width;out.height=height;out.pixels.resize(size_t(width)*height);
 for(size_t i=0;i<out.pixels.size();++i)out.pixels[i]=(std::uint32_t(bytes[i*4+3])<<24)|(std::uint32_t(bytes[i*4])<<16)|(std::uint32_t(bytes[i*4+1])<<8)|bytes[i*4+2];
 return true;
}
// Shrinks a picture (box filter) until it fits the SIGN_ART limits.
inline Image fitArt(const Image& in){
 if(in.width<=ArtMaxSide&&in.height<=ArtMaxSide&&in.width*in.height<=ArtMaxPixels)return in;
 float scale=std::min({ArtMaxSide/float(in.width),ArtMaxSide/float(in.height),std::sqrt(ArtMaxPixels/float(in.width*in.height))});
 Image out;out.width=std::max(1,int(in.width*scale));out.height=std::max(1,int(in.height*scale));out.pixels.resize(size_t(out.width)*out.height);
 for(int y=0;y<out.height;++y)for(int x=0;x<out.width;++x){
  int x0=int(x/scale),x1=std::max(x0+1,int((x+1)/scale)),y0=int(y/scale),y1=std::max(y0+1,int((y+1)/scale));double s[4]{};int n=0;
  for(int yy=y0;yy<std::min(y1,in.height);++yy)for(int xx=x0;xx<std::min(x1,in.width);++xx){auto p=in.pixels[size_t(yy)*in.width+xx];for(int c=0;c<4;++c)s[c]+=(p>>(c*8))&255;++n;}
  std::uint32_t p=0;for(int c=0;c<4;++c)p|=std::uint32_t(s[c]/std::max(1,n)+.5)<<(c*8);out.pixels[size_t(y)*out.width+x]=p;}
 return out;
}

} // namespace retro::signart
