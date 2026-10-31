#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(push_constant) uniform ShadowFace {vec4 lamp;vec4 forward;vec4 up;vec4 housing;vec4 turn;} p;
layout(location=0) in vec3 toPoint;
layout(location=1) in vec2 uv;
void main(){
 float distance=length(toPoint);
 // The lamp's own housing surrounds the emitter; it must not shadow the room.
 vec2 local=vec2(toPoint.x*p.turn.x-toPoint.y*p.turn.y,toPoint.x*p.turn.y+toPoint.y*p.turn.x);
 float z=toPoint.z+p.lamp.z;
 if(p.housing.x>0.0&&abs(local.x)<=p.housing.x+0.005&&abs(local.y)<=p.housing.y+0.005&&z>=p.housing.z-0.005&&z<=p.housing.w+0.005)discard;
 if(p.housing.x<=0.0&&distance<0.3)discard;
 // Grates, fences and foliage cut holes in the shadow like they do on screen.
 if(textureLod(colorMap,uv,0.0).a<0.5)discard;
 // Radial depth: one comparison works on every cube face.
 gl_FragDepth=clamp(distance/p.lamp.w,0.0,1.0);
}
