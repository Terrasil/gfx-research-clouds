// Controlled Nubis-inspired reconstruction, not the proprietary game shader.
// M1..M8 share the same field, lighting phase and shell. OPT/FST/AAL are independent.
const float RADIUS=6371./3.; // field units: 3 km per unit, before noise scale
const float CLOUD_BASE=.35;
vec3 shellCenter(){return vec3(0,-RADIUS,0);}
float cloudHeight(vec3 p){
    // Stable altitude computation avoids subtracting nearly equal radii.
    float z=dot(p,p)+2.*RADIUS*p.y;
    return (z/(length(p-shellCenter())+RADIUS)-CLOUD_BASE)/uVolumeField.w;
}
float shellExit(vec3 p,vec3 d,float alt){
    float b=dot(p-shellCenter(),d);
    float c=dot(p,p)+2.*RADIUS*p.y-alt*(2.*RADIUS+alt);
    float s=sqrt(max(0.,b*b-c));
    return b>=0.?-c/max(s+b,1e-8):s-b;
}
vec3 fieldPoint(vec3 p){return (p-vec3(0,CLOUD_BASE,0))*uVolumeField.x-vec3(uVolumeMotion.x,-uVolumeMotion.z,uVolumeMotion.y)*(uTime*uVolumeField.x/max(uFieldScaleRadius,1e-4));}
float profile(float h){return smoothstep(0.,mix(.06,.14,uVolumeShape.w),h)*(1.-smoothstep(mix(.52,.76,uVolumeShape.w),1.,h));}
float coverageAt(vec3 p){vec3 f=fieldPoint(p);float w=clamp((textureLod(uCloudShape,vec3(f.x*.17,.37,f.z*.17),0.).r-.45)*2.5,0.,1.);return clamp(uVolumeField.y+(w-.5)*uVolumeMotion.w,.001,.995);}
float baseDensity(vec3 p,float coverage){float h=cloudHeight(p);if(h<=0.||h>=1.)return 0.;return clamp((textureLod(uCloudShape,fieldPoint(p),0.).r*profile(h)-(1.-coverage))/coverage,0.,1.);}
float densityAt(vec3 p,float coverage,float footprint,float interval,bool detail){float b=baseDensity(p,coverage);return detail?lomRefine(fieldPoint(p),cloudHeight(p),b,footprint,interval):b;}
// Conservative max-pyramid adaptation for trilinear texture data. Not the analytic FST25 derivative algorithm.
// Bounds are stored over interpolation cells, each leaf includes its 8 vertices.
float emptyDistance(vec3 p,vec3 d,float cov){
    vec3 f=fieldPoint(p)-vec3(.5/128.);vec3 v=d*uVolumeField.x;
    // The bound ignores height profile and erosion (both <= 1). Safe for fixed column coverage.
    for(int level=7;level>=0;--level){
        int n=128>>level;vec3 q=f*float(n);ivec3 cell=ivec3(floor(q));
        ivec3 wrap=(cell%n+n)%n;
        float upper=texelFetch(uBounds,wrap,level).r;
        if(upper<=1.-cov-1e-6){
            vec3 fr=fract(q);
            vec3 edge=mix(fr,1.-fr,greaterThan(v,vec3(0)));
            vec3 dist=edge/(max(abs(v),vec3(1e-20))*float(n));
            dist=mix(dist,vec3(1e20),lessThan(abs(v),vec3(1e-12)));
            return max(0.,min(dist.x,min(dist.y,dist.z))*.999);
        }
    }
    return 0.;
}
float lightDepth(vec3 p,float cov,float fp,vec3 light,float rho,int samples,bool aal){
    float h=cloudHeight(p);
    float extent=min(shellExit(p,light,CLOUD_BASE+uVolumeField.w),uVolumeField.w*4.);
    if(extent<=0.)return 0.;
    float k=18.*uVolumeField.z/uVolumeField.w*uVolumeShape.z;
    vec3 right=normalize(cross(light,abs(light.y)>.98?vec3(0,0,1):vec3(0,1,0)));
    vec3 up=cross(right,light);
    if(aal){
        // Experimental AAL: fit a bounded linear local trend, integrate it analytically,
        // check a farther sample, and fall back to the full cone when disagreement is high.
        float a=extent*.25;
        float b=densityAt(p+light*a,cov,fp,a*.5,true);
        float slope=(b-rho)/max(a,1e-6);
        float pred=clamp(rho+slope*extent*.65,0.,1.);
        float c=densityAt(p+light*(extent*.65),cov,fp,extent*.1,true);
        if(abs(c-pred)<=uAalTolerance && !(rho<.02 && b<.02 && c<.02)){
            // Integrate the positive, clipped linear function exactly on [0,extent].
            float x=rho+slope*extent;
            float integral;
            if(x<0.)integral=.5*rho*clamp(-rho/min(slope,-1e-8),0.,extent);
            else if(x>1.){float t=clamp((1.-rho)/max(slope,1e-8),0.,extent);integral=.5*(rho+1.)*t+extent-t;}
            else integral=.5*(rho+x)*extent;
            return max(integral,0.)*k;
        }
    }
    float sum=0.;vec2 cone=vec2(1,0);
    for(int i=0;i<samples;++i){
        float a=float(i)/float(samples),b=float(i+1)/float(samples);
        float t0=a*a*extent,t1=b*b*extent,t=.5*(t0+t1);
        vec3 off=(right*cone.x+up*cone.y)*(t*uVolumeSampling.x);
        sum+=densityAt(p+light*t+off,cov,fp,.5*(t1-t0),t<extent/9.)*(t1-t0);
        cone=vec2(-.737368878*cone.x-.675490294*cone.y,.675490294*cone.x-.737368878*cone.y);
    }
    return sum*k;
}
vec3 nubisClouds(vec3 bg,vec3 d,float fp){
    if(d.y<0.||uVolumeField.y<=0.||uVolumeField.z<=0.)return bg;
    float start=shellExit(vec3(0),d,CLOUD_BASE),end=shellExit(vec3(0),d,CLOUD_BASE+uVolumeField.w);
    if(end<=start)return bg;
    bool ref=uMode>=9;
    int mask=max(uMode-1,0);
    bool opt=!ref && (mask==1||mask==3||mask==5||mask==7);
    bool fst=!ref && (mask==2||mask==3||mask==6||mask==7);
    bool aal=!ref && mask>=4;
    int n=ref?uReferenceSteps:(opt?uOptSteps:uVolumeBudget.x);
    n=clamp(n,4,2048);
    int lights=ref?uReferenceLight:clamp(uVolumeBudget.y,1,16);
    float ds=(end-start)/float(n),cov=coverageAt(d*start),k=18.*uVolumeField.z/uVolumeField.w;
    float jitter=.5;
    if((opt && uJitterMode!=0) || uTemporal!=0)jitter=hash13(vec3(gl_FragCoord.xy,float(uPhaseIndex)+17.));
    float day=smoothstep(-.14,.12,uSunElevation);
    vec3 light=day<.1?moonDirection():sunDirection();
    // Sun below the layer is an approximation; cap the light path and fade direct intensity.
    vec3 sunlight=mix(vec3(.13,.17,.28),mix(vec3(1.,.46,.22),vec3(1.,.93,.79),smoothstep(0.,.5,uSunElevation)),day);
    vec3 ambient=mix(vec3(.018,.027,.052),vec3(.26,.32,.42),day);
    float cosine=dot(d,light);
    float phase=.55+.45*mix(lomPhase(cosine,uVolumeOptics.x),lomPhase(cosine,-.25),.18);
    float pv=1.-smoothstep(-.1,.7,cosine);
    float T=1.;vec3 L=vec3(0);
    int i=0,empty=0;bool coarse=false;
    while(i<n){
        float t=start+(float(i)+jitter)*ds;vec3 p=d*t;
        // Nubis-style coarse search; disable when conservative hierarchy is active or for REF.
        float rho0=baseDensity(p,cov);
        if(fst && rho0<=0.){float skip=emptyDistance(p,d,cov);if(skip>=ds){i+=max(1,int(floor(skip/ds)));continue;}}
        if(!ref && !fst && coarse){
            if(rho0>0.){i=max(0,i-1);coarse=false;empty=0;continue;}i+=2;continue;
        }
        // Keep detail filtering identical across M1..REF; depends on pixel footprint, not step budget.
        float rho=lomRefine(fieldPoint(p),cloudHeight(p),rho0,fp*t*uVolumeField.x,0.);
        ++i;
        if(rho<=.0001){if(++empty>=4)coarse=true;continue;}empty=0;coarse=false;
        float tau=k*rho*ds,att=exp(-tau);
        float ld=uVolumeShape.z>0.?lightDepth(p,cov,fp*t*uVolumeField.x,light,rho,lights,aal):0.;
        float single=exp(-ld),multi=uVolumeOptics.y*(.45*exp(-ld*.25)+.15*exp(-ld*.0625));
        float powder=mix(1.,.5+.5*(1.-exp(-rho*6.)),uVolumeOptics.z*pv);
        vec3 S=ambient*mix(.42,1.2,clamp(cloudHeight(p),0.,1.))+sunlight*(single+multi)*phase*powder*1.8;
        // Exact piecewise constant source integral is shared: baseline already uses Beer-Lambert.
        L+=T*(1.-att)*S;T*=att;
        if(T<(ref?1e-6:.001))break;
    }
    return L+T*bg;
}
