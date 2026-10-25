#version 450
// Sun cascade: orthographic depth from the static world vertex buffers.
// origin/right/up/forward come from the CPU cascade fit; the w components are
// inverse extents, so the same push constants reproduce the lookup exactly.
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
// w carries the Painterly Horizon LOD marker (negative): small ground plants
// (crossfade under 20 m) cast no sun shadow; trees and real geometry do.
layout(location=2) in vec4 worldNormal;
layout(push_constant) uniform Cascade {vec4 origin;vec4 right;vec4 up;vec4 forward;} p;
layout(location=0) out vec2 outUV;
void main(){
 vec3 d=position.xyz-p.origin.xyz;
 gl_Position=vec4(dot(d,p.right.xyz)*p.right.w,dot(d,p.up.xyz)*p.up.w,dot(d,p.forward.xyz)*p.forward.w*0.5+0.5,1.0);
 outUV=uv;
 if(worldNormal.w<0.0&&worldNormal.w>-20.0)gl_Position=vec4(0.0,0.0,-2.0,1.0);
}
