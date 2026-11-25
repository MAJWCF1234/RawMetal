#include "Win32Window.h"
#ifdef _WIN32
#include <windowsx.h>
#include <Xinput.h>
#include "../core/PackedResource.h"
namespace retro {
namespace {
// Resolve XInput at runtime so older Windows installs can use their available version.
bool readPad(XINPUT_GAMEPAD& pad){
 using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
 static const GetState get=[]()->GetState{
  for(const wchar_t* name:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll"})
   if(HMODULE module=LoadLibraryW(name))
    if(FARPROC proc=GetProcAddress(module,"XInputGetState"))
     return reinterpret_cast<GetState>(reinterpret_cast<void*>(proc));
  return nullptr;
 }();
 if(!get)return false;

 static DWORD lastScan=0;
 static DWORD slot=XUSER_MAX_COUNT;
 static bool scanned=false;
 XINPUT_STATE state{};
 if(slot<XUSER_MAX_COUNT){
  if(get(slot,&state)==ERROR_SUCCESS){pad=state.Gamepad;return true;}
  slot=XUSER_MAX_COUNT;
  scanned=false;
 }
 const DWORD now=GetTickCount();
 if(scanned&&now-lastScan<1000)return false;
 lastScan=now;
 scanned=true;
 for(DWORD candidate=0;candidate<XUSER_MAX_COUNT;++candidate){
  if(get(candidate,&state)!=ERROR_SUCCESS)continue;
  slot=candidate;
  pad=state.Gamepad;
  return true;
 }
 return false;
}

Vec2 padStick(SHORT x,SHORT y,float deadzone){
 const Vec2 stick{float(x),float(y)};
 const float magnitude=length(stick);
 if(magnitude<=deadzone)return {};
 const float strength=(std::min(magnitude,32767.f)-deadzone)/(32767.f-deadzone);
 return stick*(strength/magnitude);
}
}
RECT Win32Window::viewport(int clientWidth,int clientHeight){
 float scale=std::min(float(clientWidth)/DisplayWidth,float(clientHeight)/DisplayHeight);
 if(scale>=1)scale=std::floor(scale);
 int width=std::max(1,int(DisplayWidth*scale)),height=std::max(1,int(DisplayHeight*scale));
 int x=(clientWidth-width)/2,y=(clientHeight-height)/2;return {x,y,x+width,y+height};
}
LRESULT CALLBACK Win32Window::wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
 auto*self=reinterpret_cast<Win32Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(m==WM_LBUTTONDOWN&&self&&self->m_parent)SetFocus(h);
 if(m==WM_KILLFOCUS&&self&&self->m_parent)self->setMenu(true);
 if(m==WM_CHAR&&self){if(w<127&&w!='`'&&w!='~'&&self->m_textInput.size()<256)self->m_textInput+=char(w);return 0;}
 if(m==WM_MOUSEWHEEL&&self){self->m_scrollDelta+=GET_WHEEL_DELTA_WPARAM(w);return 0;}
 if(m==WM_CLOSE){DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
Win32Window::Win32Window(int w,int h,const wchar_t* title,HWND parent,bool visible):m_parent(parent){HINSTANCE in=GetModuleHandleW(nullptr);WNDCLASSEXW wc{sizeof(wc)};wc.lpfnWndProc=wndProc;wc.hInstance=in;wc.hIcon=LoadIconW(in,MAKEINTRESOURCEW(1));wc.hIconSm=LoadIconW(in,MAKEINTRESOURCEW(1));wc.lpszClassName=L"RetroQuakeCppWindow";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);wc.hbrBackground=static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));RegisterClassExW(&wc);POINT origin{0,0};HMONITOR monitor=MonitorFromPoint(origin,MONITOR_DEFAULTTOPRIMARY);MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);RECT bounds=info.rcMonitor;if(parent)GetClientRect(parent,&bounds);DWORD style=(parent?WS_CHILD|WS_CLIPSIBLINGS:WS_POPUP)|(visible?WS_VISIBLE:0);m_hwnd=CreateWindowExW(parent?WS_EX_NOACTIVATE:0,wc.lpszClassName,title,style,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,parent,nullptr,in,this);m_bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);m_bmi.bmiHeader.biWidth=w;m_bmi.bmiHeader.biHeight=-h;m_bmi.bmiHeader.biPlanes=1;m_bmi.bmiHeader.biBitCount=32;m_bmi.bmiHeader.biCompression=BI_RGB;if(!parent&&visible)ShowCursor(FALSE);else m_cursorVisible=true;}
void Win32Window::showLoading(float progress,const wchar_t* status){
 RECT rc{};GetClientRect(m_hwnd,&rc);const int w=rc.right,h=rc.bottom;if(w<=0||h<=0)return;
 HDC dc=GetDC(m_hwnd);HDC mem=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,w,h);HGDIOBJ old=SelectObject(mem,bitmap);
 HBRUSH ground=CreateSolidBrush(RGB(5,7,6));FillRect(mem,&rc,ground);DeleteObject(ground);SetBkMode(mem,TRANSPARENT);
 auto font=[&](int height,int weight,int spacing){HFONT f=CreateFontW(-height,0,0,0,weight,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Bahnschrift");SelectObject(mem,f);SetTextCharacterExtra(mem,spacing);return f;};
 auto centred=[&](const wchar_t* text,int y,COLORREF colour){SetTextColor(mem,colour);SIZE size{};GetTextExtentPoint32W(mem,text,lstrlenW(text),&size);TextOutW(mem,(w-size.cx)/2+(size.cx>0?GetTextCharacterExtra(mem)/2:0),y,text,lstrlenW(text));};
 // The promotional logo (resource 2184: width, height, then BGRA rows on the loading black).
 static const std::vector<unsigned char> logo=[]{try{return loadResource(2184);}catch(...){return std::vector<unsigned char>{};}}();
 int below=h*44/100;
 if(logo.size()>8){const int lw=int(logo[0]|logo[1]<<8|logo[2]<<16|logo[3]<<24),lh=int(logo[4]|logo[5]<<8|logo[6]<<16|logo[7]<<24);
  if(lw>0&&lh>0&&logo.size()>=8+size_t(lw)*lh*4){BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),lw,-lh,1,32,BI_RGB};
   const int dw=w*52/100,dh=dw*lh/lw,dx=(w-dw)/2,dy=h*40/100-dh/2;SetStretchBltMode(mem,HALFTONE);SetBrushOrgEx(mem,0,0,nullptr);
   StretchDIBits(mem,dx,dy,dw,dh,0,0,lw,lh,logo.data()+8,&info,DIB_RGB_COLORS,SRCCOPY);below=dy+dh+h/60;}}
 else{HFONT big=font(std::max(28,h/13),FW_BOLD,h/40);centred(L"DEPTHWORKS",h*34/100,RGB(59,178,103));DeleteObject(big);}
 HFONT statusFont=font(std::max(12,h/50),FW_NORMAL,std::max(2,h/300));centred(status,below,RGB(146,138,128));
 const int barW=w*30/100,barH=std::max(2,h/360),barX=(w-barW)/2,barY=below+h*7/100;
 RECT track{barX,barY,barX+barW,barY+barH},done{barX,barY,barX+int(barW*std::clamp(progress,0.f,1.f)),barY+barH};
 HBRUSH dim=CreateSolidBrush(RGB(28,34,31)),lit=CreateSolidBrush(RGB(59,178,103));FillRect(mem,&track,dim);FillRect(mem,&done,lit);DeleteObject(dim);DeleteObject(lit);
 BitBlt(dc,0,0,w,h,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(statusFont);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(m_hwnd,dc);
}
Win32Window::~Win32Window(){if(!m_cursorVisible)ShowCursor(TRUE);if(m_hwnd)DestroyWindow(m_hwnd);} 
bool Win32Window::pump(){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)m_quit=true;TranslateMessage(&msg);DispatchMessageW(&msg);}return !m_quit;}
void Win32Window::setMenu(bool open){
 bool free=open||!focused();if(free!=m_cursorVisible){ShowCursor(free?TRUE:FALSE);m_cursorVisible=free;}
 if(free||open!=m_menuOpen)m_mouseCaptured=false;m_menuOpen=open;
}
InputState Win32Window::input(bool menuOpen,float inventoryCursorDt){
 setMenu(menuOpen);InputState i{};i.textInput=std::move(m_textInput);m_textInput.clear();if(!focused()){i.textInput.clear();return i;}
 auto key=[](int value){return (GetAsyncKeyState(value)&0x8000)!=0;};
 i.forward=key('W');i.back=key('S');i.left=key('A');i.right=key('D');i.sprint=key(VK_SHIFT);i.jump=key(VK_SPACE);i.crouch=key('C')||key(VK_CONTROL);i.prone=key('Z');i.fire=key(VK_LBUTTON);i.reload=key('R');i.mute=key('M');i.music=key('N');i.use=key('E');i.flashlight=key('F');i.subdue=key('Q');i.physglove=key('G');
 i.escape=key(VK_ESCAPE);i.inventory=key('I');i.menuUp=key(VK_UP);i.menuDown=key(VK_DOWN);i.menuLeft=key(VK_LEFT);i.menuRight=key(VK_RIGHT);i.menuAccept=key(VK_RETURN);i.scoreboard=key(VK_TAB);i.chat=key('T');i.talk=m_talkKey>0&&key(m_talkKey);
 i.guard=key(VK_RBUTTON);i.rotateProp=key(VK_MBUTTON);
 XINPUT_GAMEPAD pad{};
const bool connected=readPad(pad);
 // Controller options. The tables are the menu's choices (index 3 of the look speeds is the stock feel).
 static constexpr float lookSpeeds[]={.4f,.6f,.8f,1.f,1.3f,1.6f,2.2f},deadScales[]={1.f,1.5f,2.f,2.6f},steadyTau[]={0.f,.08f,.18f,.35f},pressFilter[]={0.f,.12f,.25f,.45f};
 const float tickNow=float(GetTickCount64()%100000000ull)/1000.f;float tickDt=std::clamp(tickNow-m_padLastTick,0.f,.1f);m_padLastTick=tickNow;
 const PadOptions opt=m_pad;
 if(connected){
  if(opt.swapSticks){std::swap(pad.sThumbLX,pad.sThumbRX);std::swap(pad.sThumbLY,pad.sThumbRY);}
  if(opt.bumpers){   // fire and aim on the bumpers; use and the Physgloves move to the triggers
   const bool rb=(pad.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0,lb=(pad.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0;
   const bool rt=pad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD,lt=pad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
   pad.wButtons=WORD((pad.wButtons&~(XINPUT_GAMEPAD_RIGHT_SHOULDER|XINPUT_GAMEPAD_LEFT_SHOULDER))|(rt?XINPUT_GAMEPAD_RIGHT_SHOULDER:0)|(lt?XINPUT_GAMEPAD_LEFT_SHOULDER:0));
   pad.bRightTrigger=rb?255:0;pad.bLeftTrigger=lb?255:0;
  }
  if(opt.filter>0){   // a press counts once held long enough; a tremor's brush does not
   const float need=pressFilter[std::clamp(opt.filter,0,3)];
   static constexpr WORD bits[6]={XINPUT_GAMEPAD_A,XINPUT_GAMEPAD_B,XINPUT_GAMEPAD_X,XINPUT_GAMEPAD_Y,XINPUT_GAMEPAD_RIGHT_SHOULDER,XINPUT_GAMEPAD_LEFT_SHOULDER};
   for(int b=0;b<6;++b){const bool down=(pad.wButtons&bits[b])!=0;m_padHold[b]=down?m_padHold[b]+tickDt:0;if(down&&m_padHold[b]<need)pad.wButtons=WORD(pad.wButtons&~bits[b]);}
   {const bool rt=pad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;m_padHold[6]=rt?m_padHold[6]+tickDt:0;if(rt&&m_padHold[6]<need)pad.bRightTrigger=0;}
   {const bool lt=pad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;m_padHold[7]=lt?m_padHold[7]+tickDt:0;if(lt&&m_padHold[7]<need)pad.bLeftTrigger=0;}
  }
 }
 const WORD buttons=pad.wButtons;
 const WORD pressedButtons=buttons&~m_padButtons;
 m_padButtons=buttons;
 const bool triggerFire=pad.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
 const bool triggerAim=pad.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
const float deadScale=deadScales[std::clamp(opt.dead,0,3)];
 auto steadied=[&](Vec2 raw,Vec2& state){const float tau=steadyTau[std::clamp(opt.steady,0,3)];if(tau<=0)return state=raw;state=state+(raw-state)*(1.f-std::exp(-tickDt/tau));return state;};
 const Vec2 look=steadied(padStick(pad.sThumbRX,pad.sThumbRY,std::min(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE*deadScale,30000.f)),m_padLookSmooth);
 bool inventoryClick=false;
 if(menuOpen){
  // Actions held in a menu must be released before they can act in the world.
  m_padHeldInMenu=buttons;
  if(inventoryCursorDt<=0||!triggerFire)m_padFireHeldInMenu=triggerFire;
  inventoryClick=inventoryCursorDt>0&&triggerFire&&!m_padFireHeldInMenu;
  if(inventoryClick)m_padFireHeldInMenu=true;
  m_padAimHeldInMenu=triggerAim;
 }else{
  m_padHeldInMenu&=buttons;
  m_padFireHeldInMenu&=triggerFire;
  m_padAimHeldInMenu&=triggerAim;
 }
 if(menuOpen){m_aimOn=m_sprintOn=m_crouchOn=false;}
 if(connected){
  constexpr int menuDeadzone=16000;
  i.menuUp|=(buttons&XINPUT_GAMEPAD_DPAD_UP)||pad.sThumbLY>menuDeadzone;
  i.menuDown|=(buttons&XINPUT_GAMEPAD_DPAD_DOWN)||pad.sThumbLY<-menuDeadzone;
  i.menuLeft|=(buttons&XINPUT_GAMEPAD_DPAD_LEFT)||pad.sThumbLX<-menuDeadzone;
  i.menuRight|=(buttons&XINPUT_GAMEPAD_DPAD_RIGHT)||pad.sThumbLX>menuDeadzone;
  i.escape|=(buttons&XINPUT_GAMEPAD_START)||(menuOpen&&(buttons&XINPUT_GAMEPAD_B));
  const bool browsingScoreboard=(buttons&XINPUT_GAMEPAD_BACK)!=0;
  i.scoreboard|=browsingScoreboard;
  i.menuAccept|=(menuOpen||browsingScoreboard)&&(buttons&XINPUT_GAMEPAD_A);
  if(!menuOpen){
   const WORD gameplay=buttons&~m_padHeldInMenu;
   const bool aimRaw=triggerAim&&!m_padAimHeldInMenu;
   if(opt.toggleAim&&aimRaw&&!m_prevAimRaw)m_aimOn=!m_aimOn;
   m_prevAimRaw=aimRaw;
   const bool aiming=opt.toggleAim?m_aimOn:aimRaw;
   i.controllerMove=steadied(padStick(pad.sThumbLX,pad.sThumbLY,std::min(XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE*deadScale,30000.f)),m_padMoveSmooth);
   i.controllerLook=look*(length(look)*(aiming?.45f:1.f));
   i.controllerLook.x*=2.8f*lookSpeeds[std::clamp(opt.look,0,6)];
   i.controllerLook.y*=1.8f*lookSpeeds[std::clamp(opt.look,0,6)];
   if(opt.toggleSprint){if(pressedButtons&XINPUT_GAMEPAD_LEFT_THUMB)m_sprintOn=!m_sprintOn;if(lengthSq(i.controllerMove)<.01f)m_sprintOn=false;i.sprint|=m_sprintOn;}
   else i.sprint|=(gameplay&XINPUT_GAMEPAD_LEFT_THUMB)!=0;
   i.flashlight|=(gameplay&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;
   i.jump|=!browsingScoreboard&&(gameplay&XINPUT_GAMEPAD_A);
   if(opt.toggleCrouch){if(pressedButtons&XINPUT_GAMEPAD_B)m_crouchOn=!m_crouchOn;i.crouch|=m_crouchOn;}
   else i.crouch|=(gameplay&XINPUT_GAMEPAD_B)!=0;
   i.reload|=(gameplay&XINPUT_GAMEPAD_X)!=0;
   i.subdue|=(gameplay&XINPUT_GAMEPAD_Y)!=0;
   i.use|=(gameplay&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0;
   i.physglove|=(gameplay&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0;
   i.inventory|=!browsingScoreboard&&(gameplay&XINPUT_GAMEPAD_DPAD_DOWN);
   i.talk|=!browsingScoreboard&&(gameplay&XINPUT_GAMEPAD_DPAD_UP);
   i.fire|=triggerFire&&!m_padFireHeldInMenu;
   i.guard|=aiming;
   if(!browsingScoreboard){
    if(pressedButtons&XINPUT_GAMEPAD_DPAD_RIGHT)i.weaponScroll=1;
    if(pressedButtons&XINPUT_GAMEPAD_DPAD_LEFT)i.weaponScroll=-1;
   }
  }
 }
 for(int digit=1;digit<=5;++digit)if(key('0'+digit))i.choice=digit;
 i.console=key(VK_OEM_3);
 RECT rc{};GetClientRect(m_hwnd,&rc);POINT pointer{};GetCursorPos(&pointer);
 if(menuOpen){m_scrollDelta=0;ScreenToClient(m_hwnd,&pointer);auto view=viewport(rc.right,rc.bottom);
  if(i.fire&&GetCapture()!=m_hwnd)SetCapture(m_hwnd);else if(!i.fire&&GetCapture()==m_hwnd)ReleaseCapture();
  if(inventoryCursorDt>0&&lengthSq(look)>0){
   const float step=700.f*std::min(inventoryCursorDt,.05f);
   m_padCursorRemainder+=Vec2{look.x,-look.y}*step;
   const LONG dx=LONG(m_padCursorRemainder.x),dy=LONG(m_padCursorRemainder.y);
   m_padCursorRemainder=m_padCursorRemainder-Vec2{float(dx),float(dy)};
   if(dx||dy){
    pointer.x=std::clamp(pointer.x+dx,view.left,view.right-1);
    pointer.y=std::clamp(pointer.y+dy,view.top,view.bottom-1);
    POINT screen=pointer;ClientToScreen(m_hwnd,&screen);SetCursorPos(screen.x,screen.y);
   }
  }else m_padCursorRemainder={};
  i.fire|=inventoryClick;
  i.pointerX=(pointer.x-view.left)*DisplayWidth/std::max(1L,view.right-view.left);i.pointerY=(pointer.y-view.top)*DisplayHeight/std::max(1L,view.bottom-view.top);return i;
 }
 m_padCursorRemainder={};
 if(m_scrollDelta)i.weaponScroll=m_scrollDelta>0?1:-1;
 m_scrollDelta=0;
 if(GetCapture()==m_hwnd)ReleaseCapture();
 POINT center{rc.right/2,rc.bottom/2};ClientToScreen(m_hwnd,&center);
 if(m_mouseCaptured){i.mouseDx=float(pointer.x-center.x);i.mouseDy=float(pointer.y-center.y);}m_mouseCaptured=true;SetCursorPos(center.x,center.y);return i;
}
void Win32Window::present(const std::uint32_t*p,int w,int h){if(!m_hwnd)return;HDC dc=GetDC(m_hwnd);RECT r{};GetClientRect(m_hwnd,&r);auto view=viewport(r.right,r.bottom);
 RECT bars[]={{0,0,r.right,view.top},{0,view.bottom,r.right,r.bottom},{0,view.top,view.left,view.bottom},{view.right,view.top,r.right,view.bottom}};
 for(auto bar:bars)FillRect(dc,&bar,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
 SetStretchBltMode(dc,COLORONCOLOR);m_bmi.bmiHeader.biWidth=w;m_bmi.bmiHeader.biHeight=-h;StretchDIBits(dc,view.left,view.top,view.right-view.left,view.bottom-view.top,0,0,w,h,p,&m_bmi,DIB_RGB_COLORS,SRCCOPY);ReleaseDC(m_hwnd,dc);
}
void Win32Window::setCaption(const std::wstring&s){SetWindowTextW(m_hwnd,s.c_str());}
}
#endif


