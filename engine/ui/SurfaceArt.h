#pragma once
#include "UiOp.h"
#include "Theme.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace surfaceArt {
inline std::array<uint32_t,16> palette() {
    using namespace theme;
    return {WELL,GRID,WELL_DIM,WELL_TEXT,TRACE_BLUE,TRACE_GREEN,TRACE_VIOLET,TRACE_AMBER,
            TRACE_RED,TRACE_PINK,SHADOW,HI,TRACK,SURFACE,RAISED,DRIVE_TRACE};
}
inline uint8_t nearest(uint32_t c,const std::array<uint32_t,16>& pal) {
    int best=1<<30,idx=0;
    for(int i=0;i<16;++i) {
        if((c&0xffffff)==(pal[i]&0xffffff))return uint8_t(i);
        const int r=int(c>>16&255)-int(pal[i]>>16&255),g=int(c>>8&255)-int(pal[i]>>8&255),b=int(c&255)-int(pal[i]&255);
        const int d=2*r*r+4*g*g+b*b;
        if(d<best){best=d;idx=i;}
    }
    return uint8_t(idx);
}
class Raster {
    static constexpr int W=216,H=46;
    std::array<uint32_t,W*H> pixels_{};
    struct Bounds{int l,t,r,b;};
    Bounds clip_{0,0,W,H};
    static float X(float v){return v*.5f;}
    static float Y(float v){return (v-22)*.5f;}
    void put(int x,int y,uint32_t c) {
        if(x<clip_.l||x>=clip_.r||y<clip_.t||y>=clip_.b)return;
        auto& dst=pixels_[y*W+x];
        const unsigned a=c>>24;
        if(a==255){dst=c;return;}
        unsigned out=0xff000000;
        for(int shift: {0,8,16})out|=(((c>>shift&255)*a+(dst>>shift&255)*(255-a)+127)/255)<<shift;
        dst=out;
    }
    template<class Inside> void shape(float l,float t,float r,float b,uint32_t c,Inside inside) {
        const int x0=std::max(clip_.l,int(std::floor(l))),x1=std::min(clip_.r,int(std::ceil(r)));
        const int y0=std::max(clip_.t,int(std::floor(t))),y1=std::min(clip_.b,int(std::ceil(b)));
        for(int y=y0;y<y1;++y)for(int x=x0;x<x1;++x)if(inside(x+.5f,y+.5f))put(x,y,c);
    }
    void rect(float x,float y,float w,float h,uint32_t c) {
        shape(x,y,x+w,y+h,c,[=](float a,float b){return a>=x&&a<x+w&&b>=y&&b<y+h;});
    }
    void line(float x,float y,float u,float v,float width,uint32_t c) {
        const float rad=std::max(.53f,width*.5f),dx=u-x,dy=v-y,ds=dx*dx+dy*dy;
        shape(std::min(x,u)-rad,std::min(y,v)-rad,std::max(x,u)+rad,std::max(y,v)+rad,c,
            [=](float a,float b){const float q=ds>0?std::clamp(((a-x)*dx+(b-y)*dy)/ds,0.f,1.f):0;
                const float rx=a-x-q*dx,ry=b-y-q*dy;return rx*rx+ry*ry<=rad*rad;});
    }
    void triangle(float x,float y,float u,float v,float p,float q,uint32_t c) {
        auto cross=[](float a,float b,float c,float d,float e,float f){return (c-a)*(f-b)-(d-b)*(e-a);};
        shape(std::min({x,u,p}),std::min({y,v,q}),std::max({x,u,p}),std::max({y,v,q}),c,
            [=](float a,float b){float d=cross(x,y,u,v,a,b),e=cross(u,v,p,q,a,b),f=cross(p,q,x,y,a,b);
                return (d>=0&&e>=0&&f>=0)||(d<=0&&e<=0&&f<=0);});
    }
public:
    explicit Raster(const std::vector<UiOp>& ops) {
        pixels_.fill(theme::WELL);
        for(const auto& o:ops){
            const float* f=o.f; const auto c=o.color;
            const float x=X(f[0]),y=Y(f[1]);
            switch(o.type) {
            case UiOp::RRECT:rect(x,y,X(f[2]),X(f[3]),c);break;
            case UiOp::STROKE:{
                const float u=x+X(f[2]),v=y+X(f[3]),wd=X(f[5]);
                line(x,y,u,y,wd,c);line(u,y,u,v,wd,c);line(u,v,x,v,wd,c);line(x,v,x,y,wd,c);break;}
            case UiOp::LINE:line(x,y,X(f[2]),Y(f[3]),X(f[4]),c);break;
            case UiOp::CIRCLE:{
                const float r=std::max(.6f,X(f[2]));
                shape(x-r,y-r,x+r,y+r,c,[=](float a,float b){return (a-x)*(a-x)+(b-y)*(b-y)<=r*r;});break;}
            case UiOp::TRI:triangle(x,y,X(f[2]),Y(f[3]),X(f[4]),Y(f[5]),c);break;
            case UiOp::CLIP:
                clip_={std::clamp(int(std::ceil(x)),0,W),std::clamp(int(std::ceil(y)),0,H),
                    std::clamp(int(std::floor(x+X(f[2]))),0,W),std::clamp(int(std::floor(y+X(f[3]))),0,H)};break;
            case UiOp::ENDCLIP:clip_={0,0,W,H};break;
            case UiOp::TEXT:break; // hardware has its own textual header and parameter cells
            }
        }
    }
    uint32_t pixel(int x,int y) const{return pixels_[y*W+x];}
};
} // namespace surfaceArt
