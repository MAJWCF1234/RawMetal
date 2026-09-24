#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
layout(push_constant) uniform Params { float underwater; float sceneDim; float damageFlash; float shotKick; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec3 highlight(vec2 p){
 vec3 sampleColor=texture(sceneImage,clamp(p,vec2(0.0),vec2(1.0))).rgb;
 return max(sampleColor-vec3(0.72),vec3(0.0));
}
void main(){
 vec2 centered=(uv-.5)*2.0;float radius=length(centered);float edge=clamp((radius-.48)/.52,0.0,1.0);edge=edge*edge*(3.0-2.0*edge);
 vec2 pixel=1.0/vec2(textureSize(sceneImage,0));vec2 shift=normalize(centered+vec2(1e-5))*pixel*2.0*edge*clamp(params.shotKick,0.0,1.0);
 vec3 scene;scene.r=texture(sceneImage,uv+shift).r;scene.g=texture(sceneImage,uv).g;scene.b=texture(sceneImage,uv-shift).b;
 // Only bright emitters bleed. Blur their highlights, never the base image or
 // the full-resolution HUD, so low-resolution textures stay visibly crisp.
 vec2 glowPixel=1.0/vec2(textureSize(sceneImage,0));
 vec3 bloom=highlight(uv+vec2(4.0,0.0)*glowPixel)+highlight(uv-vec2(4.0,0.0)*glowPixel)
           +highlight(uv+vec2(0.0,4.0)*glowPixel)+highlight(uv-vec2(0.0,4.0)*glowPixel);
 bloom+=0.5*(highlight(uv+vec2(11.0,7.0)*glowPixel)+highlight(uv-vec2(11.0,7.0)*glowPixel)
            +highlight(uv+vec2(11.0,-7.0)*glowPixel)+highlight(uv-vec2(11.0,-7.0)*glowPixel));
 scene+=min(bloom*0.10,vec3(0.12));scene*=params.sceneDim;
 scene*=1.0-0.06*smoothstep(0.45,1.3,radius);
 if(params.underwater>0.5)scene*=vec3(.4,.8,.9);
 float vignette=clamp(params.damageFlash,0.0,1.0)*edge*.78;scene=mix(scene,scene*vec3(.45,.07,.045),vignette);
 vec4 h=texture(hudImage,uv);color=vec4(mix(scene,h.rgb,h.a),1.0);
}
