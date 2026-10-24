#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
layout(push_constant) uniform Params { float underwater; float sceneDim; float damageFlash; float shotKick; float hdrScene; float vrMode; float alarm; float postBloom; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec3 highlight(vec2 p){
 vec3 sampleColor=texture(sceneImage,clamp(p,vec2(0.0),vec2(1.0))).rgb;
 return max(sampleColor-vec3(params.hdrScene>0.5?1.0:0.72),vec3(0.0));
}
vec3 filmic(vec3 c){c=max(c,vec3(0.0));return clamp((c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14),vec3(0.0),vec3(1.0));}
// AgX display transform (Troy Sobotka's AgX; minimal fit by Benjamin
// Wrensch, MIT licence, see THIRD_PARTY_NOTICES.md). Returns display-encoded
// RGB: highlights desaturate toward white instead of clipping to hue.
vec3 agx(vec3 c){
 const mat3 inset=mat3(0.842479062253094,0.0423282422610123,0.0423756549057051,0.0784335999999992,0.878468636469772,0.0784336,0.0792237451477643,0.0791661274605434,0.879142973793104);
 const mat3 outset=mat3(1.19687900512017,-0.0528968517574562,-0.0529716355144438,-0.0980208811401368,1.15190312990417,-0.0980434501171241,-0.0990297440797205,-0.0989611768448433,1.15107367264116);
 c=inset*max(c,vec3(1e-10));
 c=clamp((log2(c)+12.47393)/16.5,0.0,1.0);
 vec3 x2=c*c,x4=x2*x2;
 c=15.5*x4*x2-40.14*x4*c+31.96*x4-6.868*x2*c+0.4298*x2+0.1191*c-0.00232;
 // "Punchy" look: deeper shadows and fuller colour than the base curve.
 c=pow(max(c,vec3(0.0)),vec3(1.3));
 float luma=dot(c,vec3(0.2126,0.7152,0.0722));c=luma+1.25*(c-luma);
 return clamp(outset*c,0.0,1.0);
}
vec3 focusedScene(vec2 p){
 // Fixed-distance depth of field created a visible world-space focus shell
 // around the player. Keep the low-resolution scene crisp and let material,
 // atmosphere and bloom provide depth without exposing an optimization radius.
 return texture(sceneImage,p).rgb;
}
void main(){
 vec2 centered=(uv-.5)*2.0;float radius=length(centered);float edge=clamp((radius-.48)/.52,0.0,1.0);edge=edge*edge*(3.0-2.0*edge);
 vec2 pixel=1.0/vec2(textureSize(sceneImage,0));vec2 shift=normalize(centered+vec2(1e-5))*pixel*2.0*edge*clamp(params.shotKick,0.0,1.0);
 vec3 scene=focusedScene(uv);
 if(params.shotKick>0.001){scene.r=texture(sceneImage,uv+shift).r;scene.b=texture(sceneImage,uv-shift).b;}
 // Only bright emitters bleed. Blur their highlights, never the base image or
 // the full-resolution HUD, so low-resolution textures stay visibly crisp.
 vec2 glowPixel=1.0/vec2(textureSize(sceneImage,0));
 vec3 bloom=highlight(uv+vec2(2.0,0.0)*glowPixel)+highlight(uv-vec2(2.0,0.0)*glowPixel)
           +highlight(uv+vec2(0.0,2.0)*glowPixel)+highlight(uv-vec2(0.0,2.0)*glowPixel);
 bloom+=0.5*(highlight(uv+vec2(11.0,7.0)*glowPixel)+highlight(uv-vec2(11.0,7.0)*glowPixel)
            +highlight(uv+vec2(11.0,-7.0)*glowPixel)+highlight(uv-vec2(11.0,-7.0)*glowPixel));
 // The post pass already added a wider bloom.
 if(params.hdrScene>0.5&&params.postBloom<0.5)scene+=bloom*0.07;
 else scene+=min(bloom*0.10,vec3(0.12));
 scene*=params.sceneDim;
 scene*=1.0-0.06*smoothstep(0.45,1.3,radius);
 if(params.underwater>0.5)scene*=vec3(.4,.8,.9);
 // Alarm beacon: a red wash that peaks with each klaxon blast and decays.
 float alarm=clamp(params.alarm,0.0,1.0);scene=mix(scene,scene*vec3(1.45,.30,.24)+vec3(.035,0.0,0.0),alarm*.55);
 float vignette=clamp(params.damageFlash,0.0,1.0)*edge*.78;scene=mix(scene,scene*vec3(.45,.07,.045),vignette);
 // The HDR scene attachment stores linear radiance. Apply the same exposure
 // as the headless capture path before the display transfer.
 vec3 displayScene=params.hdrScene>0.5?agx(scene):scene;
 vec4 h=params.vrMode>0.5?vec4(0.0):texture(hudImage,uv);color=vec4(mix(displayScene,h.rgb,h.a),1.0);
}
