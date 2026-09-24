#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
layout(push_constant) uniform Params { float underwater; float sceneDim; float damageFlash; float shotKick; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
void main(){
 vec2 centered=(uv-.5)*2.0;float radius=length(centered);float edge=clamp((radius-.48)/.52,0.0,1.0);edge=edge*edge*(3.0-2.0*edge);
 vec2 pixel=1.0/vec2(textureSize(sceneImage,0));vec2 shift=normalize(centered+vec2(1e-5))*pixel*2.0*edge*clamp(params.shotKick,0.0,1.0);
 vec3 scene;scene.r=texture(sceneImage,uv+shift).r;scene.g=texture(sceneImage,uv).g;scene.b=texture(sceneImage,uv-shift).b;scene*=params.sceneDim;
 if(params.underwater>0.5)scene*=vec3(.4,.8,.9);
 float vignette=clamp(params.damageFlash,0.0,1.0)*edge*.78;scene=mix(scene,scene*vec3(.45,.07,.045),vignette);
 vec4 h=texture(hudImage,uv);color=vec4(mix(scene,h.rgb,h.a),1.0);
}
