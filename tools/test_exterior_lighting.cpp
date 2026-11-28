#include "../src/core/Math.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include "../src/renderer/ExteriorLighting.h"
using namespace retro;
float sample(Point3 p){
 std::array<Point3,6> axes={{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
 std::array<float,6> b={.1f,.8f,1.f,.3f,.6f,.2f};
 auto w=exteriorLightingWeights(axes,p);float v=0;for(int i=0;i<6;i++)v+=w[i]*b[i];return v;

}
int main(){
 float jump=std::fabs(sample({1,.99999f,0})-sample({.99999f,1,0}));
 std::printf("Probe boundary brightness jump %.8f\n",jump);
 if(jump>.0001f)return 1;
 float previous=sample({1,0,0}),worst=0;
 for(int i=1;i<=36000;i++){float a=i*kTwoPi/36000;float v=sample({std::cos(a),std::sin(a),0});worst=std::max(worst,std::fabs(v-previous));previous=v;}
 std::printf("Full rotation maximum per-step change %.8f\n",worst);
 if(worst>.001f)return 2;
 if(std::fabs(sample({1,0,0})-.1f)>.00001f||!std::isfinite(sample({0,0,0})))return 3;
 return 0;
}
