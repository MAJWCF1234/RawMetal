#pragma once
#ifdef _WIN32
#include <windows.h>
#include <cstdint>
#include <string>
#include "../game/Game.h"
namespace retro {
class Win32Window {
public:
 Win32Window(int w,int h,const wchar_t* title); ~Win32Window();
 bool focused()const{return GetForegroundWindow()==m_hwnd;}
 static RECT viewport(int clientWidth,int clientHeight);
 bool valid()const{return m_hwnd!=nullptr;} bool pump(); InputState input(bool menuOpen=false); void setMenu(bool open); void present(const std::uint32_t* pixels,int w,int h); void setCaption(const std::wstring& s);
private:
 std::string m_textInput;
 static LRESULT CALLBACK wndProc(HWND,UINT,WPARAM,LPARAM); HWND m_hwnd=nullptr; BITMAPINFO m_bmi{}; bool m_quit=false; bool m_mouseCaptured=false,m_menuOpen=false,m_cursorVisible=false; POINT m_center{};
};
}
#endif
