#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
layout(push_constant) uniform Params { float underwater; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){vec4 s=texture(sceneImage,uv);vec4 h=texture(hudImage,uv);vec3 scene=s.rgb;if(params.underwater>0.5)scene*=vec3(.4,.8,.9);color=vec4(mix(scene,h.rgb,h.a),1.0);}
