#version 450
layout(location=0) in vec4 position;
layout(location=1) in vec2 uv;
layout(location=2) in vec4 lighting;
layout(location=3) in vec4 light0;
layout(location=4) in vec4 light1;
layout(location=5) in vec4 surface;
layout(location=0) out vec2 outUV;
layout(location=1) out vec4 outLighting;
layout(location=2) out vec4 outLight0;
layout(location=3) out vec4 outLight1;
layout(location=4) out vec4 outSurface;
void main(){gl_Position=position;outUV=uv;outLighting=lighting;outLight0=light0;outLight1=light1;outSurface=surface;}
