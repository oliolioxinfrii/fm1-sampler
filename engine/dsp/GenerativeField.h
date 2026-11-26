#pragma once
// Native translation of Field Study II. The field, phrase and route are shared
// by the audio scheduler, phone drawing and FM-1 raster. Fixed-size data only.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace generative {
constexpr int maxEvents=16;
constexpr float tau=6.283185307179586f;
inline float clamp(float x,float lo=0.f,float hi=1.f){return std::clamp(x,lo,hi);}
inline int pc(int n){return (n%12+12)%12;}
inline constexpr const char* scaleNames[]={"MAJOR","MINOR","HARM MIN","MELOD MIN","DORIAN","PHRYGIAN","LYDIAN","MIXOLYD","LOCRIAN","MAJ PENTA","MIN PENTA","DIM H-W","DIM W-H","WHOLE TONE","CHROMATIC"};
inline constexpr int scaleCount=15;
inline constexpr int scaleN[]={8,8,8,8,8,8,8,8,8,6,6,9,9,7,13};
inline constexpr int scales[15][13]={
 {0,2,4,5,7,9,11,12},{0,2,3,5,7,8,10,12},{0,2,3,5,7,8,11,12},
 {0,2,3,5,7,9,11,12},{0,2,3,5,7,9,10,12},{0,1,3,5,7,8,10,12},
 {0,2,4,6,7,9,11,12},{0,2,4,5,7,9,10,12},{0,1,3,5,6,8,10,12},
 {0,2,4,7,9,12},{0,3,5,7,10,12},{0,1,3,4,6,7,9,10,12},
 {0,2,3,5,6,8,9,11,12},{0,2,4,6,8,10,12},{0,1,2,3,4,5,6,7,8,9,10,11,12}};
inline constexpr const char* motifNames[]={"ARCH","PEDAL","ASCENT","CUSTOM"};
inline constexpr const char* mapNames[]={"PITCH","START","END"};
inline constexpr const char* applyNames[]={"BOTH","PITCH","TIME"};
inline constexpr const char* pageNames[]={"FIELD","MELODY","TIME","EVOLVE"};
struct Config {
 int root=0,scale=4,octave=4,seed=42,octaves=3,iterations=2,steps=8,motif=0,apply=0;
 float angle=28,span=.78f,detail=4.5f,depth=.65f,evolution=.12f,persistence=.5f;
 float bpm=96,spacing=.55f,gate=.67f,hold=.35f,wander=0,warpRate=.35f,feedback=.45f,cx=.5f,cy=.5f;
 int customCount=0;
 std::array<int,12> custom{};
 std::array<int,maxEvents> pins; // semitones relative to the root, -1 = free
 Config(){pins.fill(-1);}
};
inline int count(const Config& c){return c.motif==3?c.customCount:c.steps;}
inline bool customKey(Config& c,int pitch){
 pitch=pc(pitch);
 if(c.customCount&&c.custom[c.customCount-1]==pitch){c.pins[--c.customCount]=-1;return true;}
 for(int i=0;i<c.customCount;i++)if(c.custom[i]==pitch)return false;
 if(c.customCount>=12)return false;
 c.pins[c.customCount]=-1;c.custom[c.customCount++]=pitch;return true;
}
inline int rootMidi(const Config& c){return 12*(c.octave+1)+c.root;}
inline int nearestDegree(const Config& c,int semi){
 int best=0;for(int i=1;i<scaleN[c.scale];i++)if(std::abs(scales[c.scale][i]-semi)<std::abs(scales[c.scale][best]-semi))best=i;
 return best;
}
inline int baseSemi(const Config& c,int i){
 if(c.motif==3)return pc(c.custom[i]-c.root);
 constexpr float pat[3][8]={{0,.28f,.57f,.85f,.57f,.42f,.28f,0},{.42f,.42f,.42f,.42f,.42f,.42f,.42f,.42f},{0,.14f,.28f,.42f,.57f,.71f,.85f,1}};
 const int j=std::clamp(int(std::lround(i*7.f/std::max(1,count(c)-1))),0,7);
 return scales[c.scale][int(std::lround((scaleN[c.scale]-1)*pat[c.motif][j]))];
}
struct Point{float x=0,y=0;};
struct Event{
 Point point;float pitch=0,startField=0,endField=0,nominalOn=0,nominalOff=0,on=0,off=0;
 int base=0,midi=60;bool pinned=false;
};
struct Phrase{
 Config config;uint32_t pass=0;int n=0;float total=0,radius=.1f;
 std::array<Event,maxEvents> event{};
 int anchors=0;std::array<Point,maxEvents> anchor{};std::array<float,maxEvents> coeff{};
};
struct Snapshot{Phrase phrase;uint32_t serial=0;bool running=false,enabled=false;float seconds=0;uint16_t active=0;int cursor=-1;};
inline float hash(int x,int y,int seed){
 uint32_t h=uint32_t(x)*374761393u^uint32_t(y)*668265263u^uint32_t(seed)*1597334677u;
 h=(h^(h>>13))*1274126177u;return float(double(h^(h>>16))/4294967296.0);
}
inline float noise(float x,float y,int seed){
 int i=int(std::floor(x)),j=int(std::floor(y));float u=x-i,v=y-j;
 auto fade=[](float t){return t*t*t*(t*(6*t-15)+10);};
 auto dot=[&](int a,int b){float th=tau*hash(i+a,j+b,seed);return std::cos(th)*(u-a)+std::sin(th)*(v-b);};
 return std::lerp(std::lerp(dot(0,0),dot(1,0),fade(u)),std::lerp(dot(0,1),dot(1,1),fade(u)),fade(v));
}
inline float raw(float x,float y,const Config& c,uint32_t pass,int kind){
 const int seed=c.seed+(kind==1?7919:kind==2?15401:0);
 if(c.wander>0){
  const float t=float(pass)*c.warpRate*.22f;float gain=.42f*c.wander;
  for(int i=0;i<c.iterations;i++){
   const float u=noise(x*1.8f+t+.37f,y*1.8f-t*.63f+.19f,seed+281);
   const float v=noise(x*1.8f-t*.51f+5.7f,y*1.8f+t+.93f,seed+593);
   x=clamp(x+gain*u,-.5f,1.5f);y=clamp(y+gain*v,-.5f,1.5f);
   gain*=c.feedback;if(gain==0)break;
  }
 }
 float sum=0,w=1,total=0,f=c.detail;
 for(int j=0;j<c.octaves;j++){sum+=w*noise(x*f*1.55f+.317f+j*13.71f,y*f/1.55f+.619f+j*7.11f,seed+j*101);total+=w;w*=c.persistence;f*=2;}
 return clamp(2*sum/total,-1,1);
}
inline float basis(Point a,Point b,float r){
 float q=std::hypot(a.x-b.x,a.y-b.y)/r;if(q>=1)return 0;float s=1-q;return s*s*s*s*(1+4*q);
}
inline float field(const Phrase& p,float x,float y,int kind){
 float v=raw(x,y,p.config,p.pass,kind);
 if(kind==0)for(int i=0;i<p.anchors;i++)v+=p.coeff[i]*basis({x,y},p.anchor[i],p.radius);
 return clamp(v,-1,1);
}
inline Phrase makePhrase(const Config& c,uint32_t pass){
 Phrase p;p.config=c;p.pass=pass;p.n=count(c);
 const float beat=60.f/c.bpm,minDur=std::min(.025f,beat*.08f);p.total=p.n*beat;
 p.radius=clamp(c.span*.45f,.05f,.30f);
 float a=c.angle*tau/360,dx=std::cos(a)*c.span*.5f,dy=std::sin(a)*c.span*.5f;
 const float phase=float(pass)*c.evolution*.23f,drift=c.evolution==0?0:.13f;
 float cx=clamp(c.cx+drift*std::sin(phase),std::abs(dx)+.015f,1-std::abs(dx)-.015f);
 float cy=clamp(c.cy+drift*(std::cos(phase*.83f)-1),std::abs(dy)+.015f,1-std::abs(dy)-.015f);
 for(int i=0;i<p.n;i++){
  float q=p.n==1?0.f:2.f*i/(p.n-1)-1.f;
  p.event[i].point={cx+dx*q,cy+dy*q};
  if(c.pins[i]>=0)p.anchor[p.anchors++]=p.event[i].point;
 }
 // Compact-support interpolation: impose zero noise offset at pinned nodes.
 // Gauss-Jordan with pivoting, bounded 16x17 work space.
 double M[maxEvents][maxEvents+1]={};
 for(int i=0;i<p.anchors;i++){
  for(int j=0;j<p.anchors;j++)M[i][j]=basis(p.anchor[i],p.anchor[j],p.radius);
  M[i][p.anchors]=-raw(p.anchor[i].x,p.anchor[i].y,c,pass,0);
 }
 bool valid=true;
 for(int k=0;k<p.anchors;k++){
  int pivot=k;for(int i=k+1;i<p.anchors;i++)if(std::abs(M[i][k])>std::abs(M[pivot][k]))pivot=i;
  for(int j=0;j<=p.anchors;j++)std::swap(M[k][j],M[pivot][j]);
  const double d=M[k][k];if(std::abs(d)<1e-10){valid=false;break;}
  for(int j=k;j<=p.anchors;j++)M[k][j]/=d;
  for(int i=0;i<p.anchors;i++)if(i!=k){double f=M[i][k];for(int j=k;j<=p.anchors;j++)M[i][j]-=f*M[k][j];}
 }
 if(valid)for(int i=0;i<p.anchors;i++)p.coeff[i]=float(M[i][p.anchors]);
 for(int i=0;i<p.n;i++){
  auto& e=p.event[i];auto q=e.point;e.base=baseSemi(c,i);e.pinned=c.pins[i]>=0;
  e.pitch=field(p,q.x,q.y,0);e.startField=field(p,q.x,q.y,1);e.endField=field(p,q.x,q.y,2);
  int semi=e.base;
  if(e.pinned)semi=c.pins[i];
  else if(c.apply!=2&&c.depth>0){
   int hi=scaleN[c.scale]-1;
   int deg=std::clamp(int(std::lround(nearestDegree(c,e.base)+c.depth*hi*e.pitch)),0,hi);
   semi=scales[c.scale][deg];
  }
  e.midi=rootMidi(c)+semi;e.nominalOn=i*beat;e.nominalOff=e.nominalOn+c.gate*beat;
  e.on=clamp(e.nominalOn+(c.apply==1?0:.44f*beat*c.spacing*e.startField),0,p.total-minDur);
  e.off=clamp(e.nominalOff+(c.apply==1?0:.44f*beat*c.hold*e.endField),e.on+minDur,p.total);
 }
 return p;
}
inline uint32_t color(float v){
 constexpr int pal[5][3]={{32,48,77},{47,83,117},{95,145,158},{186,201,162},{239,199,116}};
 const float u=clamp((v+1)*.5f)*4;int i=std::min(3,int(u));float f=u-i;uint32_t out=0xff000000u;
 for(int j=0;j<3;j++)out|=uint32_t(std::lround(std::lerp(float(pal[i][j]),float(pal[i+1][j]),f)))<<(16-8*j);
 return out;
}
} // namespace generative
