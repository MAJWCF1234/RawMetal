#version 450
layout(set=0,binding=0) uniform sampler2D colorMap;
layout(set=0,binding=1) uniform sampler2D normalMap;
layout(set=0,binding=2) uniform sampler2D emissionMap;
layout(set=0,binding=3) uniform sampler2D reliefMap;
layout(push_constant) uniform ViewState { vec4 eyeYaw; vec4 basis; vec4 effects; vec4 fogLights[4]; vec4 atmosphere; } view;
layout(location=0) in vec2 uv;
layout(location=1) in vec4 lighting;
layout(location=2) in vec4 light0;
layout(location=3) in vec4 light1;
layout(location=4) in vec4 surface;
layout(location=6) in vec3 worldPos;
layout(location=7) in float parallaxScale;
layout(location=0) out vec4 outColor;

vec3 toLinear(vec3 c){return pow(max(c,vec3(0.0)),vec3(2.2));}
vec3 toDisplay(vec3 c){return pow(max(c,vec3(0.0)),vec3(1.0/2.2));}

vec3 filmic(vec3 c){
 c=max(c,vec3(0.0));
 return clamp((c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14),vec3(0.0),vec3(1.0));
}

// GGX dielectric BRDF: rough paint retains a neutral grazing reflection.
float dielectricSpecular(vec3 N,vec3 V,vec3 L,float roughness){
 float NoL=max(dot(N,L),0.0),NoV=max(dot(N,V),0.001);
 if(NoL<=0.0)return 0.0;
 vec3 halfVector=L+V;
 vec3 H=halfVector*inversesqrt(max(dot(halfVector,halfVector),0.000001));
 float NoH=max(dot(N,H),0.0),VoH=max(dot(V,H),0.0);
 float a=roughness*roughness,a2=a*a;
 float denominator=NoH*NoH*(a2-1.0)+1.0;
 float D=a2/(3.14159265*denominator*denominator);
 float gv=NoL*sqrt(NoV*NoV*(1.0-a2)+a2);
 float gl=NoV*sqrt(NoL*NoL*(1.0-a2)+a2);
 float visibility=0.5/max(gv+gl,0.00001);
 float F=0.04+0.96*pow(1.0-VoH,5.0);
 return F*D*visibility*NoL;
}

float shaftScattering(vec3 eye,vec3 endpoint,vec4 lamp){
 float height=lamp.z-lamp.w;
 if(height<=0.0)return 0.0;

 vec3 ray=endpoint-eye;
 float rayLength=length(ray);
 if(rayLength<0.05)return 0.0;

 vec3 direction=ray/rayLength;
 float hSpeed=length(direction.xy);
 if(hSpeed<0.001)return 0.0;

 vec2 toLamp=lamp.xy-eye.xy;
 vec2 dir2D=direction.xy/hSpeed;
 float along=(toLamp.x*dir2D.x+toLamp.y*dir2D.y)/hSpeed;
 vec2 closestPoint=toLamp-direction.xy*along;
 float perpDist=length(closestPoint);

 if(perpDist>1.25)return 0.0;

 float coneRadius=1.20;
 float halfSpan=coneRadius/hSpeed;
 float start=max(0.0,along-halfSpan);
 float finish=min(rayLength,along+halfSpan);
 if(finish<=start)return 0.0;

 float stepLength=(finish-start)/8.0;
 float jitter=fract(52.9829189*fract(dot(gl_FragCoord.xy,vec2(0.06711056,0.00583715))));
 float sum=0.0;

 for(int i=0;i<8;++i){
  vec3 p=eye+direction*(start+(float(i)+jitter)*stepLength);
  float h=(p.z-lamp.w)/height;
  if(h<=0.0||h>=1.0)continue;

  float r=mix(1.10,0.30,h);
  float d=length(p.xy-lamp.xy);
  if(d>r)continue;

  float core=1.0-smoothstep(r*0.25,r,d);
  float ends=smoothstep(0.0,0.12,h)*smoothstep(0.0,0.06,1.0-h);
  float dust=0.88+0.12*sin(p.x*7.3+p.y*5.7+p.z*3.9)*sin(p.x*4.1-p.y*8.2+p.z*2.7);
  sum+=core*ends*dust;
 }

 float towardEye=max(0.0,dot(-direction,normalize(vec3(eye.xy-lamp.xy,eye.z-lamp.z))));
 float opticalDepth=sum*stepLength*(0.04+0.14*towardEye*towardEye);
 return min(1.0-exp(-opticalDepth),0.075);
}

void main(){
 vec2 sampleUV=uv;
 vec2 uvDx=dFdx(uv),uvDy=dFdy(uv);

 float materialDistance=length(worldPos-view.eyeYaw.xyz);
 float parallaxFade=1.0-smoothstep(8.0,18.0,materialDistance);
 bool parallaxMaterial=parallaxScale>0.0&&surface.z<0.5&&parallaxFade>0.001;
 bool litMaterial=lighting.w>0.5||light0.w+light1.w>0.0;
 vec3 tangent=vec3(0.0),bitangent=vec3(0.0),geometricNormal=vec3(0.0);
 bool tangentFrameValid=false;
 if(parallaxMaterial||litMaterial){
  vec3 dx=dFdx(worldPos),dy=dFdy(worldPos);
  float determinant=uvDx.x*uvDy.y-uvDx.y*uvDy.x;
  if(abs(determinant)>0.00001){
   tangent=normalize((dx*uvDy.y-dy*uvDx.y)/determinant);
   bitangent=normalize((dy*uvDx.x-dx*uvDy.x)/determinant);
   geometricNormal=normalize(cross(tangent,bitangent));
   tangentFrameValid=true;
  }
 }

 if(parallaxMaterial&&tangentFrameValid){
   vec3 toEye=normalize(view.eyeYaw.xyz-worldPos);
   vec2 direction=vec2(dot(toEye,tangent),dot(toEye,bitangent))/max(abs(dot(toEye,geometricNormal)),0.35);
   vec2 stepUV=clamp(direction,vec2(-2.0),vec2(2.0))*parallaxScale*parallaxFade/4.0;

   sampleUV=uv+stepUV*2.0;

   for(int layer=0;layer<4;++layer){
    float height=textureGrad(reliefMap,sampleUV,uvDx,uvDy).r;
    if(float(layer)/4.0>=height)break;
    sampleUV-=stepUV;
   }
 }

 vec4 color=textureGrad(colorMap,sampleUV,uvDx,uvDy);
 if(color.a<(surface.z>0.5?0.01:0.5))discard;

 float vertexLight=lighting.y;
 float specularLight=0.0;
 float waterFresnel=0.0;

 if(lighting.w>0.5||light0.w+light1.w>0.0){
  vec4 filteredNormal=lighting.w>0.5?textureGrad(normalMap,sampleUV,uvDx,uvDy):vec4(0.0,0.0,1.0,1.0);
  vec3 n=normalize(filteredNormal.xyz);
  float response=0.22+light0.w*max(0,dot(n,light0.xyz))+light1.w*max(0,dot(n,light1.xyz));
  vertexLight*=clamp(response/lighting.z,0.35,1.8);

  float gloss=fract(surface.z);

  if(tangentFrameValid){
    vec3 normal=normalize(cross(tangent,bitangent));
    vec3 eyeDirection=normalize(view.eyeYaw.xyz-worldPos);
    vec3 viewTangent=normalize(vec3(dot(eyeDirection,tangent),dot(eyeDirection,bitangent),abs(dot(eyeDirection,normal))));
    float roughness=clamp(1.0-gloss,0.18,0.96);
    vec3 dnX=dFdx(n),dnY=dFdy(n);
    float normalVariance=(1.0-filteredNormal.w)/max(filteredNormal.w,0.05);
    roughness=sqrt(clamp(roughness*roughness+normalVariance+0.35*(dot(dnX,dnX)+dot(dnY,dnY)),0.0324,1.0));

    if(light0.w>0.0){
     specularLight+=light0.w*dielectricSpecular(n,viewTangent,light0.xyz,roughness);
    }

    if(light1.w>0.0){
     specularLight+=light1.w*dielectricSpecular(n,viewTangent,light1.xyz,roughness);
    }

    if(surface.z>1.5){
     float NdotV=clamp(viewTangent.z,0.0,1.0);
     waterFresnel=0.04+0.96*pow(1.0-NdotV,5.0);
    }
  }
 }

 if(surface.z>0.5&&surface.z<1.5){
  vec3 additive=color.rgb*color.a*lighting.x*vec3(1,0.72,0.35);
  outColor=vec4(view.effects.w>0.5?toLinear(additive):additive,1);
  return;
 }

 // Dielectrics return approximately four percent through the specular lobe.
 vec3 result=toLinear(color.rgb)*lighting.x*vertexLight*0.96/(1+surface.x*0.018);

 if(surface.z<0.5&&surface.w<1.5){
  float key=clamp((vertexLight-0.24)/0.75,0.0,1.0);
  result*=mix(vec3(0.82,0.88,0.97),vec3(1.04,0.98,0.90),key);
 }

 result+=vec3(specularLight*lighting.x);

 if(surface.z>1.5){
  vec3 skySheen=toLinear(vec3(0.55,0.72,0.92));
  result+=skySheen*waterFresnel*0.22;
 }

 if(surface.y>0.0){
  result+=toLinear(textureGrad(emissionMap,sampleUV,uvDx,uvDy).rgb)*1.6*surface.y;
 }

 // Fixture lighting is already visibility-tested in vertexLight and the two
 // tangent-space lights. A second unshadowed lamp loop leaked through walls
 // and incorrectly dotted tangent-space normals with world-space light vectors.

 if(surface.z<0.5&&surface.w<1.5&&view.atmosphere.w>0.0){
  float distance=length(worldPos-view.eyeYaw.xyz);
  float density=view.atmosphere.w;
  float heightDensity=exp(-max(worldPos.z*0.06,0.0));
  float nearWeight=smoothstep(0.75,7.0,distance);
  float extinction=(1.0-exp(-distance*density*mix(0.80,1.20,heightDensity)))*nearWeight;
  vec3 rayDir=(worldPos-view.eyeYaw.xyz)/max(distance,0.001);
  vec3 sunDir=normalize(vec3(0.5,0.7,0.5));
  float cosTheta=dot(rayDir,sunDir);
  float phase=0.90+0.10*cosTheta*cosTheta;
  result=mix(result,toLinear(view.atmosphere.rgb)*phase,min(extinction,0.35));
 }

 if(surface.z<0.5&&surface.w<1.5){
  float scatter=0.0;
  for(int light=0;light<4;++light){
   scatter+=shaftScattering(view.eyeYaw.xyz,worldPos,view.fogLights[light]);
  }
  // Keep shafts subordinate to surface lighting and architectural silhouettes.
  scatter=min(scatter,0.045);
  result+=toLinear(vec3(0.98,0.90,0.78))*scatter*1.0;
 }

 float alpha=surface.z>1.5?color.a:1;
 outColor=view.effects.w>0.5?vec4(max(result,vec3(0)),alpha):vec4(toDisplay(filmic(result*0.95)),alpha);
}
