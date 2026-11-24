#version 460 core
in vec2 vUv;
layout(location=0) out vec4 oColor;
layout(location=1) out vec4 oLinear;
uniform vec2 uResolution;
uniform float uPitch,uYaw,uFov,uSunElevation,uSunAzimuth,uTime;
uniform float uFieldScaleRadius;
uniform vec3 uSkyCenter;
uniform int uMode,uOptSteps,uReferenceSteps,uReferenceLight,uPhaseIndex,uJitterMode,uTemporal;
uniform vec2 uPixelJitter;
uniform sampler3D uBounds;
uniform float uAalTolerance;
float hash13(vec3 p){p=fract(p*0.1031);p+=dot(p,p.yzx+33.33);return fract((p.x+p.y)*p.z);}
vec3 sunDirection(){return vec3(cos(uSunElevation)*cos(uSunAzimuth),sin(uSunElevation),cos(uSunElevation)*sin(uSunAzimuth));}
vec3 moonDirection(){return normalize(vec3(-0.52,0.57,-0.73));}
vec3 queryPosition(vec3 ray,vec3 samplePosition){return samplePosition;}
#include "simplified.glsl"
#include "nubis.glsl"
float stars(vec3 r,float scale){vec3 p=r*scale;return smoothstep(.9955,1.,hash13(floor(p)))*exp(-42.*dot(fract(p)-.5,fract(p)-.5));}
vec3 sky(vec3 r){
    float day=smoothstep(-.14,.12,uSunElevation);
    float y=max(r.y,0.);
    vec3 n=mix(vec3(.025,.045,.085),vec3(.0025,.008,.028),sqrt(y));
    n+=vec3(.78,.86,1.)*stars(r,310.)*smoothstep(0.,.16,y);
    float mc=max(dot(r,moonDirection()),0.);
    n+=vec3(.65,.75,.95)*(.12*pow(mc,42.)+1.35*smoothstep(cos(radians(1.15)),cos(radians(.76)),mc));
    vec3 d=lomAtmosphere(r);
    float dusk=1.-smoothstep(.04,.35,abs(uSunElevation));
    d=mix(d,d*vec3(1.3,.62,.38)+vec3(.24,.065,.015)*pow(max(dot(r,sunDirection()),0.),6.),dusk*.7);
    return mix(vec3(.055,.06,.07),mix(n,d,day),smoothstep(-.03,0.,r.y));
}
void main(){
    vec2 uv=((vUv+uPixelJitter/uResolution)*2.-1.)*vec2(uResolution.x/uResolution.y,1.)*tan(uFov*.5);
    vec3 forward=vec3(cos(uPitch)*cos(uYaw),sin(uPitch),cos(uPitch)*sin(uYaw));
    vec3 right=vec3(-sin(uYaw),0.,cos(uYaw));
    vec3 up=cross(right,forward);
    // right x forward points up for this coordinate convention.
    vec3 r=normalize(forward+right*uv.x+up*uv.y);
    vec3 bg=sky(r);
    float footprint=2.*tan(uFov*.5)/uResolution.y;
    bool night=uSunElevation<-.10;
    vec3 c=uMode==0?lomClouds(bg,r,r*uFieldScaleRadius,footprint,night):nubisClouds(bg,r,footprint);
    c=max(c,vec3(0));
    oLinear=vec4(c,1);
    oColor=vec4(pow(c/(1.+c),vec3(1./2.2)),1);
}
