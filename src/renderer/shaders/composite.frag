#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
layout(set=0,binding=2) uniform sampler2D sceneDepth;
layout(push_constant) uniform Params { float underwater; float sceneDim; float damageFlash; float shotKick; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec3 highlight(vec2 p){
 vec3 sampleColor=texture(sceneImage,clamp(p,vec2(0.0),vec2(1.0))).rgb;
 return max(sampleColor-vec3(0.72),vec3(0.0));
}
float viewDistance(float depth){return 0.06/max(1.0-depth,0.00001);}
vec3 focusedScene(vec2 p){
 vec3 sharp=texture(sceneImage,p).rgb;
 float centerDepth=texture(sceneDepth,p).r;
 if(centerDepth>=0.99999)return sharp;
 float distance=viewDistance(centerDepth);
 // Bliss's 2.10 cm / f4.8 camera settings produce a small circle of
 // confusion. Keep nearby surfaces and the weapon crisp during combat.
 const float focalLength=0.021;
 const float aperture=2.10/4.8;
 const float focusDistance=4.0;
 float coc=abs(aperture*focalLength*(distance-focusDistance)
              /(distance*(focusDistance-focalLength)));
 // The reference pack's camera operates on a different world scale. Calibrate
 // the circle of confusion in screen pixels so the far room reads as focused
 // through a lens while near combat detail stays clear.
 float radius=min(coc*float(textureSize(sceneImage,0).x)*1.75,7.0);
 radius*=smoothstep(focusDistance-0.3,focusDistance+0.7,distance);
 if(radius<0.35)return sharp;
 vec2 pixel=1.0/vec2(textureSize(sceneImage,0));
 const vec2 taps[8]=vec2[8](vec2(1,0),vec2(.707,.707),vec2(0,1),vec2(-.707,.707),
                            vec2(-1,0),vec2(-.707,-.707),vec2(0,-1),vec2(.707,-.707));
 vec3 sum=sharp*2.0;float weight=2.0;
 for(int i=0;i<8;++i){
  vec2 q=clamp(p+taps[i]*pixel*radius,vec2(0),vec2(1));
  float sampleDistance=viewDistance(texture(sceneDepth,q).r);
  // Do not smear a nearby silhouette or viewmodel across a distant wall.
  if(sampleDistance<distance*0.82)continue;
  sum+=texture(sceneImage,q).rgb;weight+=1.0;
 }
 return sum/weight;
}
void main(){
 vec2 centered=(uv-.5)*2.0;float radius=length(centered);float edge=clamp((radius-.48)/.52,0.0,1.0);edge=edge*edge*(3.0-2.0*edge);
 vec2 pixel=1.0/vec2(textureSize(sceneImage,0));vec2 shift=normalize(centered+vec2(1e-5))*pixel*2.0*edge*clamp(params.shotKick,0.0,1.0);
 vec3 scene=focusedScene(uv);
 if(params.shotKick>0.001){scene.r=texture(sceneImage,uv+shift).r;scene.b=texture(sceneImage,uv-shift).b;}
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
