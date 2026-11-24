#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
double integral(double r,double slope,double length){double x=r+slope*length;if(x<0.)return .5*r*std::clamp(-r/slope,0.,length);if(x>1.){double t=std::clamp((1.-r)/slope,0.,length);return .5*(r+1.)*t+length-t;}return .5*(r+x)*length;}
int main(){try{
 for(double r:{0.,.01,.4,.9,1.})for(double slope:{-10.,-1.,-.01,0.,.01,1.,10.})for(double l:{.01,.5,2.}){double sum=0;for(int j=0;j<100000;++j)sum+=std::clamp(r+slope*(j+.5)*l/100000.,0.,1.)*l/100000.;require(std::abs(sum-integral(r,slope,l))<1e-8,"AAL integral");}
 for(double dy:{0.,.1,.5,1.}){double R=6371./3.;for(double altitude:{.35,.63}){double b=R*dy,c=-altitude*(2*R+altitude),t=-c/(std::sqrt(b*b-c)+b);double stable=(t*t+2*R*t*dy)/(std::sqrt(R*R+t*t+2*R*t*dy)+R);require(std::abs(stable-altitude)<1e-12,"shell intersection");}}
 // Trilinear interpolation is bounded by its eight vertices, and a parent by child maxima.
 uint32_t seed=123;auto rng=[&]{seed=1664525u*seed+1013904223u;return double(seed>>8)/16777216.;};
 for(int k=0;k<10000;++k){double vertices[8];for(auto&v:vertices)v=rng();double x=rng(),y=rng(),z=rng(),sum=0;for(int i=0;i<8;++i)sum+=vertices[i]*(i&1?x:1-x)*(i&2?y:1-y)*(i&4?z:1-z);require(sum<=*std::max_element(vertices,vertices+8)+1e-12,"interpolation bound");}
 for(int n:{1,8,64,512}){double tau=2.3/n,T=1.,L=0.;for(int i=0;i<n;++i){L+=T*(1.-std::exp(-tau))*.7;T*=std::exp(-tau);}require(std::abs(L-.7*(1.-std::exp(-2.3)))<1e-12,"Beer-Lambert integral");}
 std::cout<<"All reference contracts passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
