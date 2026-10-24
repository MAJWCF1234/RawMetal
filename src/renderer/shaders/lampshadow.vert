#version 450
// Lamp shadow maps: one 90-degree face of a lamp's cube, drawn from the
// static world vertex buffers. The face basis matches ShadowFace/ShadowUp in
// scene.frag and comes from the CPU per face.
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
layout(push_constant) uniform ShadowFace {vec4 lamp;vec4 forward;vec4 up;} p;
layout(location=0) out vec3 toPoint;
layout(location=1) out vec2 outUV;
void main(){
 vec3 d=position.xyz-p.lamp.xyz,right=cross(p.forward.xyz,p.up.xyz);
 float w=dot(d,p.forward.xyz);
 // z=w-0.02 clips at a 2 cm near plane; the fragment writes radial depth.
 gl_Position=vec4(dot(d,right),dot(d,p.up.xyz),w-0.02,w);
 toPoint=d;outUV=uv;
}
