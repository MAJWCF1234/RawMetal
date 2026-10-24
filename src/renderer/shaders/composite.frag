#version 450
layout(set=0,binding=0) uniform sampler2D sceneImage;
layout(set=0,binding=1) uniform sampler2D hudImage;
// Exposure stage output: b holds the eased focus distance.
layout(set=0,binding=2) uniform sampler2D exposureImage;
layout(push_constant) uniform Params { float underwater; float sceneDim; float damageFlash; float shotKick; float hdrScene; float vrMode; float alarm; float postBloom;
 float lensAperture; float filmTime; float lensPad; float aspect; } params;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
vec3 highlight(vec2 p){
 vec3 sampleColor=texture(sceneImage,clamp(p,vec2(0.0),vec2(1.0))).rgb;
 return max(sampleColor-vec3(params.hdrScene>0.5?1.0:0.72),vec3(0.0));
}
vec3 filmic(vec3 c){c=max(c,vec3(0.0));return clamp((c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14),vec3(0.0),vec3(1.0));}
// Display transform derived here from first principles (no third-party
// curve): inset toward luminance, log2 encoding over -10..+6.5 stops around
// middle grey, a pivot that centres grey, a symmetric S curve for contrast,
// a power that lands grey at display 0.46, then an outset to restore colour.
vec3 displayCurve(vec3 c){
 const float Inset=0.18,MinStops=-10.0,Range=16.5,Contrast=1.7,Outset=1.22;
 const float Pivot=log(0.5)/log(10.0/16.5),Grey=log(0.46)/log(0.5);
 float l=dot(max(c,vec3(0.0)),vec3(0.2126,0.7152,0.0722));
 c=mix(max(c,vec3(0.0)),vec3(l),Inset);
 vec3 t=clamp((log2(max(c,vec3(1e-10))/0.18)-MinStops)/Range,0.0,1.0);
 t=pow(t,vec3(Pivot));
 vec3 a=pow(t,vec3(Contrast)),b=pow(1.0-t,vec3(Contrast));
 vec3 d=pow(a/(a+b),vec3(Grey));
 float dl=dot(d,vec3(0.2126,0.7152,0.0722));
 return clamp(dl+Outset*(d-dl),0.0,1.0);
}
// Cinematic lens, at first person. A thin lens focused at distance f images
// a point at depth d as a disc of diameter proportional to |1/f - 1/d|: the
// circle of confusion. Focus comes eased from the exposure stage (a focus
// puller racking to the crosshair). The viewmodel (negative depth) is held
// sharp so the hands stay readable.
float circleOfConfusion(float depth,float focus){
 if(depth<0.0)return 0.0;
 float d=depth>0.02?depth:200.0;
 return clamp(params.lensAperture*0.045*abs(1.0/focus-1.0/d),0.0,0.032);
}
// Lateral chromatic aberration: red and blue land at slightly different
// magnifications, growing with the square of the distance from the axis.
vec3 lensSample(vec2 p){
 vec2 c=p-0.5;float k=params.lensAperture*0.0016*dot(c*vec2(params.aspect,1.0),c*vec2(params.aspect,1.0));
 return vec3(texture(sceneImage,clamp(0.5+c*(1.0+k),vec2(0.0),vec2(1.0))).r,texture(sceneImage,p).g,texture(sceneImage,clamp(0.5+c*(1.0-k),vec2(0.0),vec2(1.0))).b);
}
vec3 depthOfField(vec2 p,vec3 sharp){
 float focus=max(texture(exposureImage,vec2(0.5)).b,0.5);
 float depth=texture(sceneImage,p).a;float coc=circleOfConfusion(depth,focus);
 if(coc<0.0008)return sharp;
 vec3 sum=sharp;float weight=1.0;
 for(int i=1;i<32;++i){
  // Vogel spiral: even disc coverage, the shape of a round aperture.
  float r=sqrt(float(i)/31.0)*coc,a=float(i)*2.3999632;
  vec2 q=p+vec2(cos(a)/params.aspect,sin(a))*r;
  vec4 c=texture(sceneImage,clamp(q,vec2(0.0),vec2(1.0)));
  // Scatter-as-gather: a sample spreads here only if its own disc reaches.
  float sampleCoc=c.a<0.0?0.0:circleOfConfusion(c.a,focus);
  float reach=smoothstep(r*0.6,r,max(sampleCoc,coc*step(depth,c.a)));
  // Bright points carry more energy into the disc: highlights become bokeh.
  float w=reach*(1.0+dot(c.rgb,vec3(0.2126,0.7152,0.0722))*1.5);
  sum+=c.rgb*w;weight+=w;
 }
 return sum/weight;
}
float filmHash(vec2 v){uvec2 q=uvec2(ivec2(v)+ivec2(65536));uint h=q.x*1664525u+q.y*1013904223u+uint(params.filmTime*24.0)*2654435761u;h^=h>>16;h*=0x7feb352du;h^=h>>15;return float(h&0xffffffu)/16777216.0;}
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
 if(params.lensAperture>0.0)scene=depthOfField(uv,lensSample(uv));
 if(params.shotKick>0.001){scene.r=texture(sceneImage,uv+shift).r;scene.b=texture(sceneImage,uv-shift).b;}
 // Only bright emitters bleed. Blur their highlights, never the base image or
 // the full-resolution HUD, so low-resolution textures stay visibly crisp.
 vec2 glowPixel=1.0/vec2(textureSize(sceneImage,0));
 vec3 bloom=highlight(uv+vec2(2.0,0.0)*glowPixel)+highlight(uv-vec2(2.0,0.0)*glowPixel)
           +highlight(uv+vec2(0.0,2.0)*glowPixel)+highlight(uv-vec2(0.0,2.0)*glowPixel);
 bloom+=0.5*(highlight(uv+vec2(11.0,7.0)*glowPixel)+highlight(uv-vec2(11.0,7.0)*glowPixel)
            +highlight(uv+vec2(11.0,-7.0)*glowPixel)+highlight(uv-vec2(11.0,-7.0)*glowPixel));
 // The post pass already added a wider bloom.
 // Halation: light scattered back through film's red layer around highlights.
 if(params.lensAperture>0.0)scene+=bloom*vec3(0.06,0.018,0.006);
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
 vec3 displayScene=params.hdrScene>0.5?displayCurve(scene):scene;
 if(params.lensAperture>0.0){
  // Natural vignetting: illumination falls as cos^4 of the angle off axis.
  vec2 ndc=uv*2.0-1.0;float t2=dot(vec2(ndc.x/1.3,ndc.y/(1.3*params.aspect)),vec2(ndc.x/1.3,ndc.y/(1.3*params.aspect)));
  displayScene*=mix(1.0,1.0/((1.0+t2)*(1.0+t2)),0.55);
  // Film grain: per-frame, strongest in the midtones like silver halide.
  float l=dot(displayScene,vec3(0.2126,0.7152,0.0722));
  displayScene=clamp(displayScene+(filmHash(gl_FragCoord.xy)-0.5)*0.05*(4.0*l*(1.0-l)+0.15),0.0,1.0);
 }
 vec4 h=params.vrMode>0.5?vec4(0.0):texture(hudImage,uv);color=vec4(mix(displayScene,h.rgb,h.a),1.0);
}
