#include "Win32Window.h"
#ifdef _WIN32
#include <windowsx.h>
namespace retro {
RECT Win32Window::viewport(int clientWidth,int clientHeight){
 float scale=std::min(float(clientWidth)/DisplayWidth,float(clientHeight)/DisplayHeight);
 if(scale>=1)scale=std::floor(scale);
 int width=std::max(1,int(DisplayWidth*scale)),height=std::max(1,int(DisplayHeight*scale));
 int x=(clientWidth-width)/2,y=(clientHeight-height)/2;return {x,y,x+width,y+height};
}
LRESULT CALLBACK Win32Window::wndProc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_NCCREATE)SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
 auto*self=reinterpret_cast<Win32Window*>(GetWindowLongPtrW(h,GWLP_USERDATA));
 if(m==WM_CHAR&&self){if(w<127&&w!='`'&&w!='~'&&self->m_textInput.size()<256)self->m_textInput+=char(w);return 0;}
 if(m==WM_MOUSEWHEEL&&self){self->m_scrollDelta+=GET_WHEEL_DELTA_WPARAM(w);return 0;}
 if(m==WM_CLOSE){DestroyWindow(h);return 0;}if(m==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(h,m,w,l);
}
Win32Window::Win32Window(int w,int h,const wchar_t* title){HINSTANCE in=GetModuleHandleW(nullptr);WNDCLASSW wc{};wc.lpfnWndProc=wndProc;wc.hInstance=in;wc.lpszClassName=L"RetroQuakeCppWindow";wc.hCursor=LoadCursor(nullptr,IDC_CROSS);RegisterClassW(&wc);RECT r{0,0,w*2,h*2};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);int width=r.right-r.left,height=r.bottom-r.top,x=CW_USEDEFAULT,y=CW_USEDEFAULT;RECT work{};if(SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0)){width=std::min(width,int(work.right-work.left));height=std::min(height,int(work.bottom-work.top));x=work.left+(work.right-work.left-width)/2;y=work.top+(work.bottom-work.top-height)/2;}m_hwnd=CreateWindowExW(0,wc.lpszClassName,title,WS_OVERLAPPEDWINDOW|WS_VISIBLE,x,y,width,height,nullptr,nullptr,in,this);m_bmi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);m_bmi.bmiHeader.biWidth=w;m_bmi.bmiHeader.biHeight=-h;m_bmi.bmiHeader.biPlanes=1;m_bmi.bmiHeader.biBitCount=32;m_bmi.bmiHeader.biCompression=BI_RGB;ShowCursor(FALSE);}
Win32Window::~Win32Window(){if(!m_cursorVisible)ShowCursor(TRUE);if(m_hwnd)DestroyWindow(m_hwnd);} 
bool Win32Window::pump(){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT)m_quit=true;TranslateMessage(&msg);DispatchMessageW(&msg);}return !m_quit;}
void Win32Window::setMenu(bool open){
 bool free=open||!focused();if(free!=m_cursorVisible){ShowCursor(free?TRUE:FALSE);m_cursorVisible=free;}
 if(free||open!=m_menuOpen)m_mouseCaptured=false;m_menuOpen=open;
}
InputState Win32Window::input(bool menuOpen){
 setMenu(menuOpen);InputState i{};i.textInput=std::move(m_textInput);m_textInput.clear();if(!focused()){i.textInput.clear();return i;}
 auto key=[](int value){return (GetAsyncKeyState(value)&0x8000)!=0;};
 i.forward=key('W');i.back=key('S');i.left=key('A');i.right=key('D');i.sprint=key(VK_SHIFT);i.jump=key(VK_SPACE);i.crouch=key('C')||key(VK_CONTROL);i.fire=key(VK_LBUTTON);i.reload=key('R');i.mute=key('M');i.music=key('N');i.use=key('E');i.flashlight=key('F');
 i.escape=key(VK_ESCAPE);i.inventory=key('I');i.menuUp=key(VK_UP);i.menuDown=key(VK_DOWN);i.menuLeft=key(VK_LEFT);i.menuRight=key(VK_RIGHT);i.menuAccept=key(VK_RETURN);
 i.guard=key(VK_RBUTTON);
 i.console=key(VK_OEM_3);
 RECT rc{};GetClientRect(m_hwnd,&rc);POINT pointer{};GetCursorPos(&pointer);
 if(menuOpen){m_scrollDelta=0;ScreenToClient(m_hwnd,&pointer);auto view=viewport(rc.right,rc.bottom);
  if(i.fire&&GetCapture()!=m_hwnd)SetCapture(m_hwnd);else if(!i.fire&&GetCapture()==m_hwnd)ReleaseCapture();
  i.pointerX=(pointer.x-view.left)*DisplayWidth/std::max(1L,view.right-view.left);i.pointerY=(pointer.y-view.top)*DisplayHeight/std::max(1L,view.bottom-view.top);return i;
 }
 i.weaponScroll=m_scrollDelta>0?1:m_scrollDelta<0?-1:0;m_scrollDelta=0;
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


