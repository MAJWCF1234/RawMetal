#include "SoftwareRenderer.h"
#include "../core/PackedResource.h"
#include "../core/Math.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <stdexcept>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "../ThirdParty/stb/stb_image.h"

namespace retro {
namespace {
constexpr float FOV = 75.0f * kPi / 180.0f;
std::uint32_t rgb(int r,int g,int b){ return 0xff000000u | (std::uint32_t(r)<<16) | (std::uint32_t(g)<<8) | std::uint32_t(b); }

const std::array<std::array<unsigned char,5>, 46> glyphs = {{
{{0x7,0x5,0x5,0x5,0x7}},{{0x2,0x6,0x2,0x2,0x7}},{{0x7,0x1,0x7,0x4,0x7}},{{0x7,0x1,0x7,0x1,0x7}},{{0x5,0x5,0x7,0x1,0x1}},
{{0x7,0x4,0x7,0x1,0x7}},{{0x7,0x4,0x7,0x5,0x7}},{{0x7,0x1,0x2,0x2,0x2}},{{0x7,0x5,0x7,0x5,0x7}},{{0x7,0x5,0x7,0x1,0x7}},
{{0x2,0x5,0x7,0x5,0x5}},{{0x6,0x5,0x6,0x5,0x6}},{{0x3,0x4,0x4,0x4,0x3}},{{0x6,0x5,0x5,0x5,0x6}},{{0x7,0x4,0x6,0x4,0x7}},{{0x7,0x4,0x6,0x4,0x4}},
{{0x3,0x4,0x5,0x5,0x3}},{{0x5,0x5,0x7,0x5,0x5}},{{0x7,0x2,0x2,0x2,0x7}},{{0x1,0x1,0x1,0x5,0x2}},{{0x5,0x5,0x6,0x5,0x5}},{{0x4,0x4,0x4,0x4,0x7}},
{{0x5,0x7,0x7,0x5,0x5}},{{0x5,0x7,0x7,0x7,0x5}},{{0x2,0x5,0x5,0x5,0x2}},{{0x6,0x5,0x6,0x4,0x4}},{{0x2,0x5,0x5,0x3,0x1}},{{0x6,0x5,0x6,0x5,0x5}},
{{0x3,0x4,0x2,0x1,0x6}},{{0x7,0x2,0x2,0x2,0x2}},{{0x5,0x5,0x5,0x5,0x7}},{{0x5,0x5,0x5,0x5,0x2}},{{0x5,0x5,0x7,0x7,0x5}},{{0x5,0x5,0x2,0x5,0x5}},
{{0x5,0x5,0x2,0x2,0x2}},{{0x7,0x1,0x2,0x4,0x7}},{{0x0,0x0,0x0,0x0}},{{0x0,0x2,0x0,0x2,0x0}},
{{0x1,0x1,0x2,0x4,0x4}},{{0x0,0x0,0x7,0x0,0x0}},{{0x0,0x0,0x0,0x0,0x2}},{{0x4,0x2,0x1,0x2,0x4}},
{{0x1,0x2,0x4,0x2,0x1}},{{0x5,0x1,0x2,0x4,0x5}},{{0x1,0x2,0x2,0x2,0x1}},{{0x4,0x2,0x2,0x2,0x4}}
}};
int gi(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='Z')return 10+c-'A'; if(c==' ')return 36;const char* extra="/-.><%()";auto found=std::strchr(extra,c);return found?38+int(found-extra):37; }
}

SoftwareRenderer::SoftwareRenderer(int w,int h):m_width(w),m_height(h),m_pixels(size_t(w*h)),m_depth(size_t(w),9999.f),m_zbuffer(size_t(w*h),9999.f){m_wall=loadTexture(101);m_floor=loadTexture(102);m_metal=loadTexture(103);m_arms=loadTexture(106);m_weaponTexture=loadTexture(112);m_enemyTexture=loadTexture(113);m_waspTexture=loadTexture(115);m_bruteTexture=loadTexture(117);m_wingTexture=loadTexture(118);
 const char* materialNames[]={"wall_6","wall_7","wall_8","wall_5","floor_1","ceiling_1","vent_1","lamp_1_on","door_1","generator_1","metal_4","metal_3","metal_6","wall_box_2","stairs_1"};
 for(int i=0;i<15;++i)m_facilityTextures.emplace(materialNames[i],loadTexture(172+i));
 m_facilityTextures.emplace("pc_1",loadTexture(192));m_facilityTextures.emplace("keyboard_1",loadTexture(193));
 {auto emission=loadTexture(194);auto&lamp=m_facilityTextures.at("lamp_1_on");if(emission.width!=lamp.width||emission.height!=lamp.height)throw std::runtime_error("Lamp emission dimensions mismatch");lamp.emission=std::move(emission.pixels);}
 m_barrelTexture=loadTexture(122);m_crateTexture=loadTexture(124);m_concrete=loadTexture(125);m_bulkhead=loadTexture(126);
 for(int i=0;i<6;++i)m_clutterTextures[i]=loadTexture(152+i*2);
 m_medkitTexture=loadTexture(134);m_shellsTexture=loadTexture(136);
 m_terminalTexture=loadTexture(137);m_cautionSign=loadTexture(138);prepareDecal(m_cautionSign);
 m_muzzleFlash=loadTexture(139);m_muzzleFlash.additive=true;prepareDecal(m_muzzleFlash);
 m_pumpTexture=loadTexture(141);m_compressorTexture=loadTexture(143);m_pipeTexture=loadTexture(145);m_gateTexture=loadTexture(147);
 m_pressureWall=loadTexture(148);m_pressureFloor=loadTexture(149);m_pressureMetal=loadTexture(150);
 attachNormal(m_wall,187);attachNormal(m_pressureWall,188);attachNormal(m_bulkhead,189);attachNormal(m_floor,190);
 m_hazard=loadTexture(127);m_chemicalSign=loadTexture(128);m_machineSign=loadTexture(129);m_confinedSign=loadTexture(130);m_signRust=loadTexture(131);m_panelMetal=loadTexture(132);
 for(auto*decal:{&m_hazard,&m_chemicalSign,&m_machineSign,&m_confinedSign})prepareDecal(*decal);
 m_routePaint=makePaint(0xffb99348u);m_redPaint=makePaint(0xff954732u);
 m_intakeSign=makeSign("01 / INTAKE","FREIGHT ACCESS",0xffca994du);m_processingSign=makeSign("02 / FOUNDRY","KEEP CLEAR",0xffd9984cu);
 m_containmentSign=makeSign("03 / CONTAINMENT","BIOHAZARD",0xffb45c42u);m_exitSign=makeSign("DISPATCH","AUTHORIZED PERSONNEL",0xff83baa7u);
 m_serviceSign=makeSign("SERVICE 04","LOW CLEARANCE",0xffca994du);
 m_transferSign=makeSign("PRESSURE WORKS","TRANSFER / 02",0xffa7a766u);m_pumpSign=makeSign("PUMP HALL","HIGH PRESSURE",0xffa7a766u);
 m_controlSign=makeSign("CONTROL GALLERY","SWITCHGEAR",0xffa7a766u);m_surfaceSign=makeSign("SURFACE LIFT","EXTRACTION",0xffa7a766u);m_gantrySign=makeSign("TURBINE GANTRY","TRANSFER / 03",0xffa7a766u);
}
SoftwareRenderer::Texture SoftwareRenderer::makeSign(const char* title,const char* subtitle,std::uint32_t accent){
 Texture sign{256,80,std::vector<std::uint32_t>(256*80)};
 for(int y=0;y<80;++y)for(int x=0;x<256;++x){auto rust=sample(m_signRust,x/256.f,y/80.f),metal=sample(m_panelMetal,x/256.f,y/80.f);int edge=std::min({x,y,255-x,79-y});
  unsigned wear=(rust>>16)&255;float light=.65f+wear/180.f;
  auto color=edge<4?shade(rust,1.1f):shade(metal,light);
  if(edge>=5&&edge<8)color=wear>77?rust:shade(accent,.55f+wear/230.f);
  if(edge<13&&wear>83)color=rust;
  sign.pixels[y*256+x]=color;
 }
 auto label=[&](int x,int y,const char* value,int scale,std::uint32_t color){for(;*value;++value,x+=4*scale){auto glyph=glyphs[size_t(gi(*value))];for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(glyph[yy]&(1<<(2-xx)))for(int j=0;j<scale;++j)for(int i=0;i<scale;++i){int px=x+xx*scale+i,py=y+yy*scale+j;if(px>=244)continue;auto rust=sample(m_signRust,px/256.f,py/80.f);unsigned wear=(rust>>16)&255;
   if(wear<106)sign.pixels[py*256+px]=shade(color,.64f+wear/220.f);
  }}};
 label(18,21,title,3,accent);label(18,51,subtitle,2,0xffbcb393u);
 for(int x:{11,244})for(int y:{11,68})for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx)sign.pixels[(y+dy)*256+x+dx]=dy==0?0xff171713u:dy<0?0xff929180u:0xff3b3a31u;
 prepareDecal(sign);return sign;
}
SoftwareRenderer::Texture SoftwareRenderer::makePaint(std::uint32_t color){
 Texture paint{64,64,std::vector<std::uint32_t>(64*64)};
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){auto rust=sample(m_signRust,x/64.f,y/64.f);unsigned wear=(rust>>16)&255;auto p=shade(color,.58f+wear/200.f);paint.pixels[y*64+x]=(p&0xffffffu)|(wear>97?0u:0xff000000u);}
 prepareDecal(paint,false);return paint;
}
void SoftwareRenderer::prepareDecal(Texture&texture,bool clampEdges){
 texture.clampEdges=clampEdges;texture.mips.clear();int width=texture.width,height=texture.height;auto pixels=texture.pixels;
 while(width>1||height>1){int nextWidth=std::max(1,width/2),nextHeight=std::max(1,height/2);std::vector<std::uint32_t> next(size_t(nextWidth*nextHeight));
  for(int y=0;y<nextHeight;++y)for(int x=0;x<nextWidth;++x){unsigned channels[4]{};for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto p=pixels[std::min(height-1,y*2+j)*width+std::min(width-1,x*2+i)];for(int c=0;c<4;++c)channels[c]+=(p>>(c*8))&255;}
   next[y*nextWidth+x]=(channels[0]/4)|((channels[1]/4)<<8)|((channels[2]/4)<<16)|((channels[3]/4)<<24);
  }texture.mips.push_back(next);pixels=std::move(next);width=nextWidth;height=nextHeight;
 }
}
SoftwareRenderer::Texture SoftwareRenderer::loadTexture(int id){
 auto resource=loadResource(id);
 Texture t; int channels=0;
 bool raw=resource.size()>=12&&!std::memcmp(resource.data(),"RMT1",4);
 if(raw){std::memcpy(&t.width,resource.data()+4,4);std::memcpy(&t.height,resource.data()+8,4);
  if(t.width<=0||t.height<=0||uint64_t(t.width)*t.height*4!=resource.size()-12)throw std::runtime_error("Invalid packed texture dimensions");}
 auto pixels=raw?resource.data()+12:stbi_load_from_memory(resource.data(),int(resource.size()),&t.width,&t.height,&channels,4);
 if(!pixels)throw std::runtime_error("Invalid embedded texture");
 t.pixels.resize(size_t(t.width)*size_t(t.height));
 for(size_t i=0;i<t.pixels.size();++i)t.pixels[i]=(std::uint32_t(pixels[i*4+3])<<24)|(rgb(pixels[i*4],pixels[i*4+1],pixels[i*4+2])&0xffffffu);
 if(!raw)stbi_image_free(pixels); return t;
}
std::uint32_t SoftwareRenderer::sample(const Texture&t,float u,float v,float lod){
 if(t.clampEdges){u=std::clamp(u,0.f,1.f);v=std::clamp(v,0.f,1.f);}else{u-=std::floor(u);v-=std::floor(v);}
 if(t.mips.empty())return t.pixels[size_t(std::min(t.height-1,int(v*t.height))*t.width+std::min(t.width-1,int(u*t.width)))];
 lod=std::clamp(lod,0.f,float(t.mips.size()));int level=int(lod);
 auto fetch=[&](int index){int width=std::max(1,t.width>>index),height=std::max(1,t.height>>index);auto&pixels=index?t.mips[index-1]:t.pixels;return pixels[size_t(std::min(height-1,int(v*height))*width+std::min(width-1,int(u*width)))];};
 auto a=fetch(level);if(size_t(level)==t.mips.size())return a;auto b=fetch(level+1);float blend=lod-level;std::uint32_t result=0;
 for(int channel=0;channel<4;++channel){int shift=channel*8;auto value=unsigned(((a>>shift)&255)*(1-blend)+((b>>shift)&255)*blend);result|=value<<shift;}return result;
}
void SoftwareRenderer::clear(std::uint32_t c){std::fill(m_pixels.begin(),m_pixels.end(),c);}
void SoftwareRenderer::put(int x,int y,std::uint32_t c){if(x>=0&&y>=0&&x<m_width&&y<m_height)m_pixels[size_t(y*m_width+x)]=c;}
void SoftwareRenderer::rect(int x,int y,int w,int h,std::uint32_t c){for(int yy=std::max(0,y);yy<std::min(m_height,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(m_width,x+w);++xx)put(xx,yy,c);}
std::uint32_t SoftwareRenderer::shade(std::uint32_t c,float s)const{int r=int(((c>>16)&255)*s),g=int(((c>>8)&255)*s),b=int((c&255)*s);return rgb(std::clamp(r,0,255),std::clamp(g,0,255),std::clamp(b,0,255));}
void SoftwareRenderer::text(int x,int y,const char* s,std::uint32_t c,int sc){for(;*s;++s,x+=4*sc){auto g=glyphs[size_t(gi(char(std::toupper((unsigned char)*s))))];for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(g[size_t(yy)]&(1<<(2-xx)))rect(x+xx*sc,y+yy*sc,sc,sc,c);}}


void SoftwareRenderer::wornPanel(int x,int y,int width,int height,bool recess,bool materialPanel){
 rect(x+2,y+3,width,height,rgb(5,4,3));
 for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
  unsigned grain=unsigned((xx+x)*7349)^unsigned((yy+y)*19391);grain=(grain^(grain>>7))*1597;
  auto c=materialPanel?sample(recess?m_panelMetal:m_signRust,(xx+x)/256.f,(yy+y)/180.f):sample(m_metal,(xx+x)/91.f,(yy+y)/67.f);
  float light=materialPanel?(recess?1.15f:.85f):(recess?.27f:.54f);light+=(grain%11)*.007f;
  int edge=std::min({xx,yy,width-1-xx,height-1-yy});
  if(edge==0)c=rgb(24,17,12),light=1;
  else if(edge==1)c=yy<height/2?rgb(113,96,71):rgb(27,22,17),light=1;
  else if(edge<4&&grain%9==0)c=rgb(126,81,42),light=.9f;
  put(x+xx,y+yy,shade(c,light));
 }
 if(width>40&&height>20){for(int dx:{5,width-6})for(int dy:{5,height-6}){
  rect(x+dx-2,y+dy-2,5,5,rgb(17,13,9));rect(x+dx-1,y+dy-1,3,3,rgb(96,86,69));
  rect(x+dx-1,y+dy,3,1,rgb(31,25,20));put(x+dx-1,y+dy-1,rgb(159,140,101));
 }}
}
void SoftwareRenderer::drawHud(const Game& game){
 const auto&p=game.player();
 const auto paper=rgb(222,206,164),muted=rgb(159,139,105),amber=rgb(210,145,54),red=rgb(180,55,36);
 const int sector=int(p.pos.y)/8;
 wornPanel(8,8,176,29);rect(17,12,151,12,rgb(24,18,13));
 text(19,14,game.level()==2?(p.z>2.5f?"08 UPPER GANTRY":"07 TURBINE HALL"):game.level()==1?(p.pos.y<7?"04 RECEIVING":p.pos.y<17?"05 PUMP HALL":"06 CONTROL"):(sector==0?"01  INTAKE":sector==1?"02  FOUNDRY":"03 CONTAINMENT"),paper,2);
 char b[80];std::snprintf(b,sizeof(b),"%d CONTACTS REMAIN",game.enemiesRemaining());text(17,27,b,muted);
 // Compact map reveals nearby contacts and a fixed extraction marker.
 const int mx=m_width-57,my=8;
 wornPanel(mx-5,my-5,58,58);rect(mx-1,my-1,50,50,rgb(19,18,10));
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(game.world().solid(x+.5f,y+.5f))rect(mx+x*2,my+y*2,2,2,game.world().tile(x,y)=='#'?rgb(85,78,45):rgb(58,52,35));
 for(auto&e:game.enemies())if(e.alive&&lengthSq(e.pos-p.pos)<36)rect(mx+int(e.pos.x*2),my+int(e.pos.y*2),2,2,red);
 auto exit=game.world().exitPoint();rect(mx+int(exit.x*2),my+int(exit.y*2),2,2,game.enemiesRemaining()==0?rgb(225,183,70):muted);
 rect(mx+int(p.pos.x*2)-1,my+int(p.pos.y*2)-1,3,3,amber);
 rect(mx+int((p.pos.x+std::cos(p.angle)*2)*2),my+int((p.pos.y+std::sin(p.angle)*2)*2),1,1,paper);
 wornPanel(0,m_height-39,m_width,39);
 for(int i=0;i<m_width;i+=9)rect(i,m_height-39,4,2,(i/9)%2?rgb(31,24,15):rgb(157,110,42));
 wornPanel(7,m_height-34,114,30,true);wornPanel(130,m_height-34,104,30,true);wornPanel(240,m_height-34,111,30,true);wornPanel(359,m_height-34,m_width-365,30,true);
 text(12,m_height-29,"HEALTH",muted);std::snprintf(b,sizeof(b),"%03d",int(p.health));text(12,m_height-20,b,p.health<30?red:paper,3);
 for(int i=0;i<10;++i){rect(62+i*5,m_height-16,4,11,rgb(10,7,5));if(p.health>i*10){rect(63+i*5,m_height-15,2,8,rgb(158,57,33));put(63+i*5,m_height-15,rgb(215,115,54));}}
 text(139,m_height-29,game.unarmed()?"UNARMED":"12 GA / SHELLS",muted);std::snprintf(b,sizeof(b),"%02d",p.ammo);text(139,m_height-20,game.unarmed()?(game.guarding()?"GUARD":"FISTS"):b,amber,game.unarmed()?2:3);
 text(247,m_height-29,"PURGE",muted);std::snprintf(b,sizeof(b),"%02d / %02d",game.kills(),int(game.enemies().size()));text(247,m_height-19,b,paper,2);
 text(m_width-103,m_height-27,"RAWMETAL",paper,2);text(m_width-103,m_height-12,"R  RESTART",muted);
 auto cross=game.hitFlash()>0?red:paper;int cx=m_width/2,cy=m_height/2;
 rect(cx-6,cy,3,1,cross);rect(cx+4,cy,3,1,cross);rect(cx,cy-6,1,3,cross);rect(cx,cy+4,1,3,cross);
 if(auto target=game.targetEnemy()){
  int x=m_width/2-65,y=m_height-60;
  wornPanel(x-4,y-4,138,20,true);text(x,y,target->name(),paper);
  rect(x,y+8,130,5,rgb(10,7,5));rect(x+1,y+9,int(128*std::max(0.f,target->hp)/target->maxHp),3,target->windup>0?amber:red);
  if(target->windup>0)text(cx-22,cy-22,"INCOMING",amber);
 }
 if(game.enemiesRemaining()==0){wornPanel(cx-70,42,140,14,true);text(cx-62,47,"TRANSFER INTERLOCK RELEASED",amber);}
 if(game.dead()||game.won()){wornPanel(cx-100,cy-26,200,51);text(cx-68,cy-15,game.won()?"SECTOR CLEARED":"SIGNAL LOST",game.won()?amber:red,2);text(cx-50,cy+7,"R TO RESTART",paper);}
 if(game.damageFlash()>0){auto tint=rgb(150,37,25);rect(0,0,m_width,2,tint);rect(0,0,2,m_height,tint);rect(m_width-2,0,2,m_height,tint);}
 if(game.audioMuted())text(10,43,"AUDIO MUTED / M",muted);
 else if(!game.musicEnabled())text(10,43,"MUSIC OFF / N",muted);
 if(game.pickupNoticeTime()>0){wornPanel(cx-88,m_height-81,176,16,true);text(cx-79,m_height-76,game.pickupNotice().c_str(),paper);}
 else if(auto pickup=game.nearbyPickup()){
  const char* message=pickup->kind==Pickup::Kind::Ammo?"12 GA / 16 SHELLS":p.health>=100?"FIRST AID / HEALTH FULL":"FIRST AID / 35 HEALTH";
  wornPanel(cx-88,m_height-81,176,16,true);text(cx-79,m_height-76,message,amber);
 }
 if(auto hint=game.interactionHint()){wornPanel(cx-85,m_height-104,170,16,true);text(cx-75,m_height-99,hint,paper);}
 if(game.logTime()>0&&game.activeLog()>=0){auto&log=game.world().terminals()[size_t(game.activeLog())];
  wornPanel(cx-155,48,310,68,false,true);text(cx-141,59,log.title,amber,2);text(cx-141,84,log.line1,paper);text(cx-141,98,log.line2,paper);
 }
}

void SoftwareRenderer::drawSettings(const Game& game){
 for(auto&pixel:m_pixels)pixel=shade(pixel,.25f);
 constexpr int x=MenuLayout::X,y=MenuLayout::Y;
 const auto paper=rgb(222,206,164),amber=rgb(210,145,54),muted=rgb(159,139,105);
 wornPanel(x,y,MenuLayout::Width,MenuLayout::Height,false,true);
 text(x+15,y+12,"RAWMETAL / SETTINGS",paper,2);text(x+15,y+29,"PAUSED",amber);
 const char* labels[]={"RESUME","MASTER VOLUME","MUSIC VOLUME","EFFECTS VOLUME","MOUSE SENSITIVITY","INVERT MOUSE Y","QUIT GAME"};
 auto&settings=game.settings();
 for(int row=0;row<MenuLayout::Rows;++row){int top=MenuLayout::RowTop+row*MenuLayout::RowHeight;bool selected=row==game.menuSelection();
  wornPanel(x+12,top,MenuLayout::Width-24,19,true,true);
  if(selected){rect(x+13,top+2,2,15,amber);rect(x+17,top+2,MenuLayout::Width-35,1,rgb(101,72,32));}
  text(x+23,top+7,labels[row],selected?paper:muted);
  if(row>=1&&row<=4){float value=row==1?settings.master:row==2?settings.music:row==3?settings.effects:settings.sensitivity;
   float normalized=row==4?(value-.2f)/2.8f:value;
   rect(MenuLayout::SliderX-3,top+7,MenuLayout::SliderWidth+6,5,rgb(8,7,5));
   rect(MenuLayout::SliderX,top+8,MenuLayout::SliderWidth,1,rgb(116,97,67));
   for(int tick=0;tick<=10;++tick)rect(MenuLayout::SliderX+tick*MenuLayout::SliderWidth/10,top+13,1,tick%5==0?3:2,muted);
   int knob=MenuLayout::SliderX+int(normalized*MenuLayout::SliderWidth);rect(knob-5,top+2,11,16,rgb(13,12,9));rect(knob-4,top+3,9,13,selected?amber:muted);rect(knob-4,top+3,9,2,paper);rect(knob+4,top+5,1,11,rgb(70,51,31));
   for(int grip=-2;grip<=2;grip+=2)rect(knob+grip,top+7,1,5,rgb(53,47,34));
   char valueText[16];if(row==4)std::snprintf(valueText,sizeof(valueText),"%.1fX",value);else std::snprintf(valueText,sizeof(valueText),"%d",int(value*100+.5f));text(MenuLayout::SliderX+83,top+7,valueText,paper);
  }
  if(row==5)text(MenuLayout::SliderX,top+7,settings.invertMouse?"ON":"OFF",paper);
 }
 text(x+15,y+201,"DRAG HANDLES / ARROWS / ENTER",muted);text(x+15,y+211,"ESC TO RESUME",amber);
}
void SoftwareRenderer::drawInventory(const Game& game){
 for(auto&pixel:m_pixels)pixel=shade(pixel,.22f);
 const auto paper=rgb(222,206,164),amber=rgb(210,145,54),muted=rgb(159,139,105);
 int x=48,y=28,w=m_width-96,h=m_height-56;wornPanel(x,y,w,h,false,true);
 text(x+16,y+12,"FIELD INVENTORY",paper,2);text(x+w-112,y+15,"I / CLOSE",muted);
 auto section=[&](int sx,int sy,int sw,int sh,const char* title){wornPanel(sx,sy,sw,sh,true,true);text(sx+8,sy+8,title,amber);};
 section(x+14,y+36,260,72,"PRIMARY WEAPON");section(x+14,y+116,260,72,"SECONDARY / MELEE");section(x+14,y+196,260,44,"EQUIPMENT");
 section(x+286,y+36,w-300,h-50,"STORAGE");
 text(x+28,y+62,game.unarmed()?"EMPTY / FISTS":"12 GA SHOTGUN",paper,2);text(x+28,y+86,game.unarmed()?"NO WEAPON EQUIPPED":"SHELLS",muted);char b[24];std::snprintf(b,sizeof(b),"%02d",game.player().ammo);text(x+215,y+84,b,amber,2);
 text(x+28,y+142,"UTILITY BLADE",paper);text(x+28,y+162,"RIGHT CLICK: GUARD",muted);
 text(x+28,y+216,"FIELD RIG / 6 SLOTS",paper);
 int gx=x+302,gy=y+66;for(int row=0;row<5;++row)for(int col=0;col<6;++col){int cw=34,ch=29;wornPanel(gx+col*cw,gy+row*ch,cw-3,ch-3,true);if(row==0&&col==0){text(gx+col*cw+6,gy+row*ch+8,"AM",amber);text(gx+col*cw+7,gy+row*ch+18,"16",paper);}if(row==0&&col==1){text(gx+col*cw+6,gy+row*ch+8,"MED",amber);text(gx+col*cw+7,gy+row*ch+18,"35",paper);}}
 text(x+302,y+h-25,"SLOTS 06 / 30",muted);text(x+16,y+h-18,"I TO RETURN TO THE SECTOR",amber);
}
void SoftwareRenderer::render(const Game& game){clear(rgb(12,16,18));drawScene(game);for(int level=0;level<Game::ChunkCount;++level)if(level!=game.level()&&game.chunkResident(level)){auto neighbor=game.chunkView(level);drawScene(neighbor,false);}drawViewModel(game);drawHud(game);if(game.paused())drawSettings(game);else if(game.inventoryOpen())drawInventory(game);}
}







