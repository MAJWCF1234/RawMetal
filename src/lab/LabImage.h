#pragma once
#include "lab/Lab.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace retro::lab {
struct Image {
 int width=0,height=0;std::vector<std::uint32_t> pixels; // 0x??RRGGBB
 Image()=default;Image(int w,int h,std::uint32_t fill=0):width(w),height(h),pixels(size_t(w)*size_t(h),fill){}
 static Image of(const std::uint32_t* p,int w,int h){Image i(w,h);std::copy(p,p+size_t(w)*h,i.pixels.begin());return i;}
 std::uint32_t& at(int x,int y){return pixels[size_t(y)*width+x];}
 void put(int x,int y,std::uint32_t rgb){if(x>=0&&y>=0&&x<width&&y<height)at(x,y)=rgb;}
};
bool savePng(const std::string& path,const Image& image);
void drawLine(Image& image,float x0,float y0,float x1,float y1,std::uint32_t rgb);
void drawText(Image& image,int x,int y,const std::string& text,std::uint32_t rgb,int scale=1,bool shadow=true);
int textWidth(const std::string& text,int scale=1);
// The overlay drawn with a projector (world point -> screen); lines clipped at the near plane.
void drawOverlay(Image& image,const Overlay& overlay,const std::function<Projected(Point3)>& project);
Image scaled(const Image& image,int width,int height); // box filter
// A grid of captioned tiles.
Image contactSheet(const std::vector<std::pair<std::string,const Image*>>& tiles,int columns,int tileWidth);
std::uint64_t hashPixels(const std::uint32_t* pixels,size_t count);
// Mean absolute channel difference (0..255) and the share of pixels moved by more than `threshold`.
struct Difference {double mean=0,changed=0;};
Difference difference(const Image& a,const Image& b,int threshold=24);
}
