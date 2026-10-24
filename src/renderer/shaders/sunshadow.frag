#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(location=0) in vec2 uv;
void main(){
 // Grates, fences and foliage cut holes in the shadow like they do on screen.
 if(textureLod(colorMap,uv,0.0).a<0.5)discard;
}
