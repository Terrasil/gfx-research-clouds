#version 460 core
in vec2 vUv;
layout(location=0) out vec4 oColor;
layout(location=1) out vec4 oLinear;
uniform sampler2D uCurrent,uHistory;
uniform int uCount;
void main(){ivec2 p=ivec2(gl_FragCoord.xy);vec3 c=texelFetch(uCurrent,p,0).rgb;
    if(uCount>0)c=mix(texelFetch(uHistory,p,0).rgb,c,1./float(min(uCount+1,64)));
    c=max(c,vec3(0));oLinear=vec4(c,1);oColor=vec4(pow(c/(1.+c),vec3(1./2.2)),1);}
