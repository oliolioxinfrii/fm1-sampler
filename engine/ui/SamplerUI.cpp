#include "SamplerUI.h"
#include "../dsp/Sampler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace theme;

// Shared shell colors; signal displays use the high-contrast trace palette.
namespace {
// Use the shared DSP kPi from Sampler.h / Common.h.
uint32_t kBg, kPaper, kOrange, kInk, kLine, kDim;
void syncColors() { kBg = theme::BG; kPaper = theme::TEXT; kOrange = theme::AMBER; kInk = theme::BG; kLine = theme::TRACK; kDim = theme::DIM; }
}

namespace {
float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
std::string num(const char* f, double v) { char b[48]; snprintf(b, sizeof b, f, v); return b; }
using Page = SamplerApp::Page;
struct MenuGroup {
    const char* deckLabel;
    const char* editorLabel;
    int count;
    Page pages[4];
    const char* labels[4];
};
// The primary buttons stay in hardware order. Only the selected group's
// available children occupy the secondary row.
constexpr MenuGroup kMenus[] = {
    {"S", "SAMPLE", 4, {Page::P_HOME, Page::P_ECHO, Page::P_GRAIN, Page::P_MOTION}, {"SAMPLE", "ECHO", "GRAIN", "MOTION"}},
    {"FX", "FX", 4, {Page::P_FX, Page::P_EFILTER, Page::P_EDELAY, Page::P_EREVERB}, {"FX", "FILTER", "DELAY", "REVERB"}},
    {"E", "ENV", 1, {Page::P_ENV}, {"ENV"}},
    {"L", "LFO", 1, {Page::P_LFO}, {"LFO"}},
    {"A", "ARP", 1, {Page::P_ARP}, {"ARP"}},
    {"T", "TAPE", 2, {Page::P_ETAPE, Page::P_EMIX}, {"EF TAPE", "EF MIX"}},
    {"M", "MIX", 1, {Page::P_MIX}, {"MIX"}},
    {"K", "KEY", 1, {Page::P_KEY}, {"KEY"}},
    {"R", "LOOP", 1, {Page::P_SAVE}, {"LOOP"}},
    {"N", "LEN", 2, {Page::P_LEN, Page::P_TEMPO}, {"LENGTH", "TEMPO"}}
};
constexpr int kMenuCount = sizeof(kMenus) / sizeof(kMenus[0]);
int activeMenu(Page page) {
    for (int i=0;i<kMenuCount;++i)
        for (int j=0;j<kMenus[i].count;++j)
            if (kMenus[i].pages[j]==page) return i;
    return 0;
}
int childCount(const SamplerApp& app, const MenuGroup& group) {
    int n=0;
    for(int j=0;j<group.count;++j) if(app.pageAvailable(group.pages[j])) ++n;
    return n;
}
Page nextMenuPage(const SamplerApp& app, const MenuGroup& group) {
    for(int i=0;i<group.count;++i) if(group.pages[i]==app.page()) {
        for(int step=1;step<=group.count;++step) {
            Page p=group.pages[(i+step)%group.count];
            if(app.pageAvailable(p)) return p;
        }
    }
    for(int i=0;i<group.count;++i) if(app.pageAvailable(group.pages[i])) return group.pages[i];
    return group.pages[0];
}
uint32_t pageColor(SamplerApp::Page p, bool trace = false) {
    using P = SamplerApp;
    switch (p) {
    case P::P_ETAPE: case P::P_EMIX: return trace ? TRACE_AMBER : AMBER;
    case P::P_GRAIN: case P::P_MOTION: case P::P_EFILTER: case P::P_EDELAY: case P::P_EREVERB:
    case P::P_FX: case P::P_LFO: case P::P_ECHO: return trace ? TRACE_VIOLET : VIOLET;
    case P::P_ENV: case P::P_ARP: case P::P_KEY: return trace ? TRACE_GREEN : GREEN;
    case P::P_SEQ: case P::P_LEN: case P::P_TEMPO: return trace ? TRACE_AMBER : AMBER;
    case P::P_SAVE: return trace ? TRACE_RED : RED;
    case P::P_MIX: return trace ? TRACE_PINK : PINK;
    default: return trace ? TRACE_BLUE : ACCENT;
    }
}
}  // namespace

SamplerUI::SamplerUI(SamplerApp& app, bool loadTheme) : app_(app) {
    if (!loadTheme) return;
    FILE* f = fopen((app_.session().dataDir() + "/theme").c_str(), "r");   // the chosen theme survives restarts; light is the default
    if (f) { char c = 0; if (fread(&c, 1, 1, f) == 1) theme::apply(c != 'd'); fclose(f); }
    syncColors();
}
void SamplerUI::setLight(bool on) {
    app_.setLight(on); syncColors();
}

// ---- primitives ----
void SamplerUI::rrect(Rect r, float rad, uint32_t c) { UiOp o; o.type = UiOp::RRECT; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.color = c; ops_.push_back(o); }
void SamplerUI::stroke(Rect r, float rad, uint32_t c, float w) { UiOp o; o.type = UiOp::STROKE; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; o.f[4] = rad; o.f[5] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::bevel(Rect r, uint32_t fill, bool inset) {
    rrect(r, 0, fill);
    const uint32_t a = inset ? SHADOW : HI, b = inset ? HI : SHADOW;
    line(r.x + 0.5f, r.y + r.h - 0.5f, r.x + 0.5f, r.y + 0.5f, a);
    line(r.x + 0.5f, r.y + 0.5f, r.x + r.w - 0.5f, r.y + 0.5f, a);
    line(r.x + r.w - 0.5f, r.y + 0.5f, r.x + r.w - 0.5f, r.y + r.h - 0.5f, b);
    line(r.x + r.w - 0.5f, r.y + r.h - 0.5f, r.x + 0.5f, r.y + r.h - 0.5f, b);
}
void SamplerUI::scope(Rect r, bool grid) {
    bevel(r, WELL, true);
    if (!grid) return;
    for (int i = 1; i < 8; ++i) line(r.x + r.w * i / 8, r.y + 2, r.x + r.w * i / 8, r.y + r.h - 2, GRID, 0.7f);
    for (int i = 1; i < 4; ++i) line(r.x + 2, r.y + r.h * i / 4, r.x + r.w - 2, r.y + r.h * i / 4, GRID, 0.7f);
}
void SamplerUI::strip(Rect r, uint32_t color, const std::string& label, float size) {
    rrect(r, 0, color);
    text(r.x + 5, r.y + r.h / 2, size, contrastInk(color), fit(label, r.w - 10, size), 0, true);
}
void SamplerUI::text(float x, float cy, float size, uint32_t c, const std::string& s, int align, bool bold) {
    UiOp o; o.type = UiOp::TEXT; o.f[0] = x; o.f[1] = cy + size * 0.36f; o.f[2] = size; o.f[3] = (float)align; o.f[4] = bold; o.color = c; o.text = s; ops_.push_back(o);
}
void SamplerUI::line(float x1, float y1, float x2, float y2, uint32_t c, float w) { UiOp o; o.type = UiOp::LINE; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = w; o.color = c; ops_.push_back(o); }
void SamplerUI::circle(float cx, float cy, float r, uint32_t c) { UiOp o; o.type = UiOp::CIRCLE; o.f[0] = cx; o.f[1] = cy; o.f[2] = r; o.color = c; ops_.push_back(o); }
void SamplerUI::tri(float x1, float y1, float x2, float y2, float x3, float y3, uint32_t c) { UiOp o; o.type = UiOp::TRI; o.f[0] = x1; o.f[1] = y1; o.f[2] = x2; o.f[3] = y2; o.f[4] = x3; o.f[5] = y3; o.color = c; ops_.push_back(o); }
void SamplerUI::clip(Rect r) { UiOp o; o.type = UiOp::CLIP; o.f[0] = r.x; o.f[1] = r.y; o.f[2] = r.w; o.f[3] = r.h; ops_.push_back(o); }
void SamplerUI::endClip() { UiOp o; o.type = UiOp::ENDCLIP; ops_.push_back(o); }
void SamplerUI::hit(Rect r, std::function<void(float, float)> d, std::function<void(float, float)> m, std::function<void()> u) { hits_.push_back({r, std::move(d), std::move(m), std::move(u)}); }
void SamplerUI::tap(Rect r, std::function<void()> f) { hit(r, [f](float, float) { f(); }); }
std::string SamplerUI::fit(const std::string& s, float width, float size) const {
    const size_t maxc = (size_t)std::max(0.f, std::floor(width / (size * 0.61f)));
    if (!maxc) return "";
    size_t count = 0, cut = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (((unsigned char)s[i] & 0xc0) == 0x80) continue;
        if (count == maxc - 1) cut = i;
        if (++count > maxc) return s.substr(0, cut) + "\xE2\x80\xA6";
    }
    return s;
}
void SamplerUI::touch(int action, float x, float y, uint32_t now) {
    now_ = now;
    if (action == 0) {
        haveActive_ = false;
        for (size_t i = hits_.size(); i-- > 0;) if (hits_[i].r.has(x, y)) { active_ = hits_[i]; haveActive_ = true; if (active_.down) active_.down(x, y); break; }
    } else if (action == 1) {
        if (haveActive_ && active_.move) active_.move(x, y);
    } else if (haveActive_) {
        if (active_.up) active_.up();
        haveActive_ = false;
    }
}
void SamplerUI::button(Rect r, const std::string& label, bool on, std::function<void()> f, float size, bool accent, uint32_t color) {
    const uint32_t col = color ? color : ACCENT;
    bevel(r, on ? col : SURFACE, on);
    if (!on && accent) rrect({r.x + 2, r.y + 2, 3, r.h - 4}, 0, col);
    text(r.x + r.w / 2, r.y + r.h / 2, size, on ? contrastInk(col) : accent ? col : TEXT, fit(label, r.w - 8, size), 1, true);
    tap(r, std::move(f));
}

// ---- frame ----
// The insert symbol is an animated, signal-reactive schematic rather than
// a calibrated M/S measurement. Its basic path is unchanged by playback.
// Windowed audio energy controls only a small aesthetic ripple.
void SamplerUI::studyScopeEnergy(int track,float& mid,float& side) {
    float m[Session::kScopePoints],s[Session::kScopePoints];
    const int count=app_.session().insertScope(track,m,s,Session::kScopePoints);
    float mr=0.f,sr=0.f,weight=0.f;
    for(int i=0;i<count;i++){
        const float w=.5f-.5f*std::cos(2*kPi*(i+.5f)/count);
        mr+=w*m[i]*m[i];sr+=w*s[i]*s[i];weight+=w;
    }
    if(weight>0.f){mr=std::sqrt(mr/weight);sr=std::sqrt(sr/weight);}
    if(scopeEnergyTrack_!=track){
        scopeEnergyTrack_=track;scopeEnergyAt_=now_;
        scopeEnergyMid_=mr;scopeEnergySide_=sr;
    }else if(scopeEnergyAt_!=now_){
        const float dt=std::min(1.f,float(now_-scopeEnergyAt_)*.001f);
        scopeEnergyAt_=now_;
        auto smooth=[dt](float old,float target){
            const float tc=target>old?.040f:.180f;
            return old+(1.f-std::exp(-dt/tc))*(target-old);
        };
        scopeEnergyMid_=smooth(scopeEnergyMid_,mr);
        scopeEnergySide_=smooth(scopeEnergySide_,sr);
    }
    mid=scopeEnergyMid_;side=scopeEnergySide_;
}
namespace {
struct InsertVizPoint { float side, mid; };
InsertVizPoint insertViz(float q,uint32_t now,float width,float level,float midRms,float sideRms) {
    constexpr float tau=2*kPi;
    const float t=float(now%600000u)*.001f;
    const float u=q*tau*5+2.1f*t;
    const float gain=.76f+.24f*clamp01(1.6f*level*level);
    const float horizontal=gain*(.4f+.8f*width);
    const float baseM=gain*(.72f*std::sin(u)+.20f*std::sin(2.02f*u+.6f*t));
    const float baseS=horizontal*(.60f*std::sin(1.37f*u+.73f*t)+.15f*std::sin(2.63f*u-.5f*t));
    // Derivative of the unperturbed path; displace along its normal,
    // producing a wiggle rather than translating the entire figure.
    const float dx=horizontal*(.60f*1.37f*std::cos(1.37f*u+.73f*t)+.15f*2.63f*std::cos(2.63f*u-.5f*t));
    const float dy=gain*(.72f*std::cos(u)+.20f*2.02f*std::cos(2.02f*u+.6f*t));
    const float norm=std::max(.08f,std::hypot(dx,dy));
    const float energy=std::hypot(midRms,sideRms);
    // A small noise floor and compressive response make quiet material
    // visible without allowing loud material to overwhelm the idle figure.
    const float amplitude=.26f*std::tanh(3.8f*std::sqrt(std::max(0.f,energy-.0005f)));
    const float sideFraction=energy>0.f?clamp01(sideRms*sideRms/(energy*energy)):.5f;
    // A ripple travels toward the head (increasing q), rather than
    // remaining localized at its tip. The tail still moves more gently.
    const float taper=.35f+.65f*q*q*(3.f-2.f*q);
    const float ripple=amplitude*taper*(.78f*std::sin(tau*9*q-7.2f*t)+.22f*std::sin(tau*14*q-10.4f*t));
    // Bias the normal displacement toward the dominant M/S component.
    // Keep a nonzero response on both axes so mono signals still wiggle.
    const float sideWeight=.70f+.30f*sideFraction;
    const float midWeight=1.f-.30f*sideFraction;
    return {std::clamp(baseS-ripple*dy/norm*sideWeight,-.99f,.99f),
            std::clamp(baseM+ripple*dx/norm*midWeight,-.99f,.99f)};
}
}
#include "StudyGraphic.inc"
#include "StudyPhone.inc"

const std::vector<UiOp>& SamplerUI::surfaceGraphic(uint32_t now) {
    using P=SamplerApp;
    now_=now;st_=app_.status();syncColors();
    ops_.clear();hits_.clear();
    const auto pg=app_.page();
    const auto k=app_.knobs(pg);
    const auto t=app_.track();
    // Compact symbol drawn at 2x, independently of the phone artwork.
    // The bottom eleven hardware pixels are reserved for amount bars.
    constexpr float top=22,bot=92,cy=57;
    const uint32_t ink=pageColor(pg,true);
    rrect({0,top,432,bot-top},0,WELL);
    if(app_.generativeDeck()&&(pg==P::P_HOME||pg==P::P_ECHO||pg==P::P_GRAIN||pg==P::P_MOTION||pg==P::P_KEY||pg==P::P_ARP||pg==P::P_SAVE)){
        const auto& p=app_.genPhrase();
        if(pg==P::P_SAVE){
            const bool rec=app_.outputRecordState()==1;circle(216,57,17,rec?TRACE_RED:WELL_DIM);
            if(rec)circle(216,57,8,WELL);
        }else if(pg==P::P_KEY||pg==P::P_ARP){
            for(int i=0;i<12;i++){uint32_t c=app_.genKeyState(i)==3?TRACE_AMBER:app_.genKeyState(i)?TRACE_BLUE:GRID;rrect({8.f+i*35,39,31,36},0,c);}
        }else{
            int kind=app_.genMap();
            for(int y=0;y<8;y++)for(int x=0;x<32;x++){
                float v=generative::field(p,(x+.5f)/32,1-(y+.5f)/8,kind);
                rrect({x*13.5f,top+y*8.75f,13.5f,8.75f},0,generative::color(v));
            }
            for(int i=1;i<p.n;i++)line(432*p.event[i-1].point.x,top+70*(1-p.event[i-1].point.y),432*p.event[i].point.x,top+70*(1-p.event[i].point.y),i<=app_.genCursor()?TRACE_AMBER:WELL_TEXT,2);
        }
        return ops_;
    }
    if(studyGraphic({0,top,432,bot-top},pg,true))return ops_;
    auto path=[&](const std::vector<std::pair<float,float>>& pts,uint32_t c,float width=3.f){
        for(size_t i=1;i<pts.size();++i)line(pts[i-1].first,pts[i-1].second,pts[i].first,pts[i].second,c,width);
    };
    auto wave=[&](float x,float y,float radius,uint32_t c){
        path({{x+radius*.50f,y-radius*.86f},{x+radius*.87f,y-radius*.5f},{x+radius,y},{x+radius*.87f,y+radius*.5f},{x+radius*.50f,y+radius*.86f}},c,3);
    };
    auto speaker=[&](float x,float y,float size){
        rrect({x,y-size*.7f,size*.7f,size*1.4f},0,WELL_DIM);
        rrect({x+2,y-size*.7f+2,size*.7f-4,size*1.4f-4},0,GRID);
        circle(x+size*.35f,y-size*.35f,size*.13f,WELL_TEXT);
        circle(x+size*.35f,y+size*.25f,size*.20f,ink);
        circle(x+size*.35f,y+size*.25f,size*.08f,WELL);
    };
    auto delay=[&](float x,float width,float time,float fb,float send,bool ping){
        speaker(x+5,cy,25);
        const float origin=x+36,span=std::max(12.f,width-47);
        const float spacing=std::max(12.f,span*(.06f+.22f*time));
        for(int i=0;i<14;++i){
            const float xx=origin+(i+.5f)*spacing;
            if(xx+8>x+width)break;
            const float r=std::max(3.f,std::min(12.f,spacing*.39f)*(.3f+.7f*std::pow(std::max(.02f,fb),i*.5f)));
            const float yy=cy+(ping?(i%2?12:-12):0);
            const auto c=send>.001f?(ping&&i%2?TRACE_BLUE:TRACE_VIOLET):WELL_DIM;
            circle(xx,yy,r,c);circle(xx,yy,std::max(1.f,r-3),WELL);
            if(r>=7)line(xx,yy-r-3,xx,yy-r-7,DRIVE_TRACE,2);
        }
    };
    auto room=[&](float x,float width,float size,float damp,float shimmer){
        const float left=x+4,right=x+width-4;
        stroke({left,top+5,right-left,bot-top-10},0,WELL_DIM,3);
        speaker(left+7,cy,24);
        const float sx=left+31,extent=(right-sx)*(.23f+.63f*size);
        for(int i=0;i<4;++i){
            const float x=sx+extent*(i+.5f)/4;
            const float radius=8+17*(i+1)/4.f*(1-.55f*damp);
            wave(x,cy,radius,i<2?DRIVE_TRACE:damp>.75f?GRID:TRACE_VIOLET);
        }
        if(shimmer>.001f)for(int i=0;i<1+int(shimmer*3);++i){
            const float xx=right-15-i*12,yy=cy+(i%2?12:-12);
            line(xx-4,yy,xx+4,yy,DRIVE_TRACE,2);line(xx,yy-5,xx,yy+5,DRIVE_TRACE,2);
        }
    };
    auto tape=[&](bool echo){
        const float a=k[0].norm,b=k[1].norm,c=k[2].norm,d=k[3].norm;
        const float l=17+10*(echo?a:b),r=13+12*(echo?b:c),cap=210+48*(echo?c:d);
        line(90,cy-l,342,cy-r,TRACE_AMBER,3);
        path({{90,cy+l},{cap,cy+26},{342,cy+r}},TRACE_AMBER,3);
        if(!echo&&b>.001f)line(90,cy-l,342,cy-r,DRIVE_TRACE,3+3*b);
        for(int j=0;j<2;++j){
            const float x=j?342:90,rr=j?r:l;
            circle(x,cy,rr,WELL_DIM);circle(x,cy,rr-3,GRID);circle(x,cy,rr*.24f,WELL_TEXT);
            for(int i=0;i<3;++i){
                const float angle=i*2*kPi/3+(echo?0:a*kPi);
                line(x+rr*.36f*std::cos(angle),cy+rr*.36f*std::sin(angle),x+rr*.67f*std::cos(angle),cy+rr*.67f*std::sin(angle),WELL,4);
            }
        }
        circle(cap,cy+26,7,TRACE_GREEN);circle(cap,cy+26,3,WELL);
        rrect({202,top+4,18,23},0,WELL_DIM);rrect({208,top+7,6,17},0,DRIVE_TRACE);
        if((echo?d:c)>.001f)for(int i=0;i<1+int(8*(echo?d:c));++i)rrect({122.0f+i*8.0f,top+7+(i%2)*3,3,3},0,echo?TRACE_PINK:TRACE_VIOLET);
    };
    switch(pg){
    case P::P_EDELAY:delay(0,432,k[0].norm,std::min(.92f,k[1].norm),k[2].norm,k[3].norm>.5f);break;
    case P::P_EREVERB:room(0,432,k[1].norm,k[2].norm,k[3].norm);break;
    case P::P_FX:room(0,208,k[1].norm,0,0);delay(220,212,k[3].norm,.78f,k[2].norm,false);break;
    case P::P_SEQ:{
        line(12,cy,420,cy,WELL_DIM,3);
        for(int i=0;i<=12;++i)line(12+i*34.f,cy-8,12+i*34.f,cy+8,GRID,2);
        circle(12+408*k[0].norm,cy,7,TRACE_AMBER);break;}
    case P::P_ETAPE:tape(true);break;
    case P::P_EFILTER:{
        const auto& ts=app_.state(t);
        std::vector<std::pair<float,float>> pts;
        for(int i=0;i<=24;++i){
            const float f=i/24.f,d=(f-k[1].norm)*16,low=1/(1+std::exp(d)),band=std::exp(-d*d*.14f);
            const float v=clamp01((ts.echoInsert.filter==0?low:ts.echoInsert.filter==1?band:1-low)+.35f*k[2].norm*band);
            pts.push_back({8+416*f,bot-7-v*(bot-top-14)});
        }
        path(pts,ink,4);
        if(k[3].norm>.001f){for(auto& pt:pts)pt.second=std::clamp(pt.second+4*k[3].norm*std::sin(pt.first),top+3,bot-3);path(pts,DRIVE_TRACE,3);}
        break;}
    case P::P_EMIX:{
        line(10,cy,350,cy,WELL_DIM,3);
        for(int j=0;j<2;++j){
            const float x=38+j*146;
            const auto c=encoderTrace(j+2);
            rrect({x,top+9,126,bot-top-18},0,GRID);stroke({x,top+9,126,bot-top-18},0,WELL_DIM,2);
            rrect({x+9,cy-6,108,12},0,WELL);
            if(k[j+2].norm>0)rrect({x+9,cy-6,108*k[j+2].norm,12},0,c);
        }
        const float sep=8+18*k[0].norm;
        path({{350,cy-sep},{350,cy+sep}},TRACE_BLUE,3);
        for(float y:{cy-sep,cy+sep}){line(350,y,410,y,TRACE_BLUE,3);circle(410,y,6,TRACE_BLUE);}
        break;}
    case P::P_KEY:{
        const auto sc=app_.scale();const auto ghosts=app_.chordGhostMask();
        const uint32_t colors[]={TRACE_BLUE,TRACE_GREEN,TRACE_GREEN,TRACE_VIOLET,TRACE_PINK,TRACE_VIOLET,TRACE_PINK,WELL_DIM};
        for(int i=0;i<12;++i){
            const float x=7+i*35;const int rel=(i-sc.root+12)%12;
            const bool legal=P::inScale(sc.scale,rel);
            const auto c=rel==0?DRIVE_TRACE:legal?colors[std::clamp(sc.scale,0,7)]:GRID;
            if(ghosts&(1u<<i))stroke({x-2,top+14,32,44},0,c,2);
            rrect({x+2,top+19,24,34},0,c);
        }
        const float ox=15+(sc.octave+3)*67;
        rrect({ox,top+4,8,5},0,TRACE_BLUE);
        break;}
    case P::P_ENV:{
        const float aa=12+90*k[0].norm,dd=12+90*k[1].norm,rr=12+90*k[3].norm,sus=bot-5-(bot-top-12)*k[2].norm;
        const float factor=416.f/(aa+dd+90+rr);
        path({{8,bot-5},{8+aa*factor,top+5},{8+(aa+dd)*factor,sus},{8+(aa+dd+90)*factor,sus},{424,bot-5}},ink,4);
        break;}
    case P::P_LFO:{
        const auto& ts=app_.state(t);std::vector<std::pair<float,float>> pts;
        for(int i=0;i<=56;++i){
            const float ph=std::fmod((1+5*k[0].norm)*i/56,1.f);
            float v=std::sin(2*kPi*ph);
            if(ts.lfoShape==1)v=ph<.5f?4*ph-1:3-4*ph;
            else if(ts.lfoShape==2)v=ph<.5f?1:-1;
            else if(ts.lfoShape==3)v=std::sin(std::floor((1+5*k[0].norm)*i/7)*7.3f);
            pts.push_back({8+390*i/56.f,cy-v*(4+26*k[1].norm)});
        }
        path(pts,encoderTrace(ts.lfoTarget),3);
        break;}
    case P::P_ARP:{
        const int n=6+int(44*k[0].norm),levels=3+int(9*k[2].norm);
        const float pitch=416.f/n;
        for(int i=0;i<n;++i){int step=i%levels;
            if(app_.state(t).arpPatt==1)step=levels-1-step;
            else if(app_.state(t).arpPatt==2)step=levels-1-std::abs(i%(2*levels-2)-(levels-1));
            else if(app_.state(t).arpPatt==3)step=i%2?levels-1-(i/2)%levels:(i/2)%levels;
            else if(app_.state(t).arpPatt==4)step=(i*17+i*i*7)%levels;
            rrect({8+i*pitch,bot-8-step*46.f/levels,std::max(2.f,pitch*(.15f+.8f*k[3].norm)),5},0,k[0].norm>0?ink:WELL_DIM);
        }
        break;}
    case P::P_MIX:{
        const float level=st_.scLevel[t],gain=st_.scGain[t];
        rrect({10,cy-11,250,22},0,GRID);
        rrect({10,cy-11,250*clamp01(level),22},0,WELL_DIM);
        rrect({10,cy-11,250*clamp01(level*gain),22},0,ink);
        const float thr=10+250*k[2].norm;
        line(thr,cy-18,thr,cy+18,TRACE_AMBER,3);
        path({{285,bot-8},{340,cy},{420,cy-(1-k[3].norm)*25}},TRACE_BLUE,3);
        break;}
    case P::P_SAVE:{
        rrect({8,cy-9,416,18},0,GRID);
        if(st_.inputPeak>0)rrect({8,cy-9,416*clamp01(st_.inputPeak*2),18},0,TRACE_GREEN);
        const float x=8+416*k[3].norm;
        line(x,cy-24,x,cy+24,TRACE_AMBER,3);break;}
    case P::P_LEN:{
        const int n=std::clamp(int(st_.loopBars),1,16);
        const float pitch=416.f/n;
        for(int i=0;i<n;++i)rrect({8+i*pitch,cy-16,pitch-3,32},0,st_.loopSamples>0?ink:GRID);
        const float pos=8+416*clamp01(loopPos());
        line(pos,cy-25,pos,cy+25,DRIVE_TRACE,3);break;}
    case P::P_TEMPO:{
        const int n=6+int(28*k[1].norm);
        line(8,cy,424,cy,WELL_DIM,2);
        for(int i=0;i<n;++i){
            const float x=8+i*416.f/n;
            const float height=i%4==0?22.f:11.f;
            line(x,cy-height,x,cy+height,st_.loopSamples>0?ink:WELL_DIM,i%4==0?4:2);
        }
        const float pos=8+416*clamp01(loopPos());
        circle(pos,cy,5,DRIVE_TRACE);
        if(st_.tempoStretch)line(8,top+4,424,top+4,TRACE_VIOLET,3);
        break;}
    default:break;
    }
    return ops_;
}

void SamplerUI::frame(uint32_t now) {
    now_ = now;
    st_ = app_.status();
    if (app_.page() != SamplerApp::P_LFO || app_.editor()) lfoSweepTrack_ = -1;
    syncColors();
    ops_.clear(); hits_.clear();
    waveRect_={};fieldRect_={};waveTrack_=-1;
    rrect({0, 0, W_, H_}, 0, BG);
    if(app_.generativeDeck()&&!app_.editor()){
        const float scale=std::min(W_/960.f,H_/450.f),ox=(W_-960*scale)*.5f,oy=(H_-450*scale)*.5f;
        const size_t first=ops_.size();generativeDeck();scaleStudy(scale,ox,oy,first);
        fieldRect_={ox+fieldRect_.x*scale,oy+fieldRect_.y*scale,fieldRect_.w*scale,fieldRect_.h*scale};
        if(sheetOpen_||genSelector_>=0)fieldRect_={};
        if(sheetOpen_)librarySheet();
        if(genSelector_>=0)generativeSelector();
        saveNotice();return;
    }
    genSelector_=-1;
    const float tb = 44, left = 176, right = 252;
    if (!app_.editor()) {   // the cassette deck fills the screen; the old sample / page editor stays one tap away (EDIT)
        const float scale=std::min(W_/960.f,H_/450.f);
        const float ox=(W_-960*scale)*.5f,oy=(H_-450*scale)*.5f;
        const size_t first=ops_.size();
        studyDeck();
        button({350,299,53,25},"GEN",false,[this]{sheetOpen_=projectOpen_=false;app_.setGenerativeDeck(true);},10,true,VIOLET);
        scaleStudy(scale,ox,oy,first);
        if (sheetOpen_) librarySheet();
        if (projectOpen_) projectSheet();
        saveNotice();return;
    }
    topBar({0, 0, W_, tb});
    trackList({MARGIN, tb + MARGIN, left - MARGIN, H_ - tb - 2 * MARGIN});
    sampleArea({left + MARGIN, tb + MARGIN, W_ - left - right - 2 * MARGIN, H_ - tb - 2 * MARGIN});
    pagePanel({W_ - right, tb + MARGIN, right - MARGIN, H_ - tb - 2 * MARGIN});
    if (sheetOpen_) {librarySheet();waveRect_={};}
    saveNotice();
}

void SamplerUI::topBar(Rect r) {
    bevel(r, SURFACE);
    rrect({0, 0, W_, 3}, 0, ACCENT);
    const float cy = r.h / 2;
    float x = MARGIN;
    // PLAY / STOP
    Rect pl{x, 6, 44, r.h - 12};
    bevel(pl, (app_.generativeDeck()?app_.genRunning():st_.playing) ? GREEN : RAISED, app_.generativeDeck()?app_.genRunning():st_.playing);
    if (app_.generativeDeck()?app_.genRunning():st_.playing) { rrect({pl.x + 15, cy - 8, 5, 16}, 0, contrastInk(GREEN)); rrect({pl.x + 24, cy - 8, 5, 16}, 0, contrastInk(GREEN)); }
    else tri(pl.x + 16, cy - 9, pl.x + 16, cy + 9, pl.x + 30, cy, GREEN);
    tap(pl, [this] { app_.transportPlay(); });
    x += 52;
    // REC: arms the first take on a clean looper, overdub afterwards
    Rect rc{x, 6, 86, r.h - 12};
    const bool recOn = app_.generativeDeck()?app_.outputRecordState()==1:(st_.freeRecording || st_.track_clip_rec[app_.track()]), armed = !app_.generativeDeck()&&st_.freeArmed;
    bevel(rc, recOn ? RED : RAISED, recOn);
    if (armed) stroke(rc, 0, RED, 2);
    circle(rc.x + 18, cy, 6, recOn ? contrastInk(RED) : RED);
    text(rc.x + 28, cy, 9.5f, recOn ? contrastInk(RED) : TEXT, app_.generativeDeck()?"REC":app_.loopExists() ? "OVERDUB" : "REC", 0, true);
    tap(rc, [this] { app_.loopRecord(); });
    x += 94;
    button({x, 6, 56, r.h - 12}, "SAVE", false, [this] { app_.loopSave(); }, 11, true, GREEN);
    x += 62;
    button({x, 6, 56, r.h - 12}, app_.generativeDeck()?"PINS":"CLEAR", false, [this] { app_.loopClear(); }, 11, true, RED);
    x += 62;
    button({x, 6, 56, r.h - 12}, "DECK", false, [this] { app_.setEditor(false); }, 11);
    x += 70;
    text(x, cy - 6, 12, st_.freeRecording || recOn || armed ? RED : AMBER, app_.loopText(), 0, true);
    text(x, cy + 11, 9.5f, DIM, app_.generativeDeck()?std::string("SOURCE: MASTER OUTPUT"):std::string("LOOP SOURCE: ") + (app_.knobs(SamplerApp::P_SAVE)[1].value), 0, false);
    tap({x - 4, 0, 200, r.h}, [this] { auto k = app_.knobs(SamplerApp::P_SAVE); if(k[1].step)k[1].step(k[1].norm > 0.5f ? -1 : 1); });
    // link state and load
    const uint32_t lc = linkMidi_ && linkAudio_ ? GREEN : linkMidi_ ? AMBER : DIM;
    circle(W_ - 22, cy, 5, lc);
    text(W_ - 34, cy - 6, 10, DIM, linkMidi_ ? "FM-1" : "NO FM-1", 2, true);
    text(W_ - 34, cy + 9, 9, DIM, "CPU " + num("%.0f", st_.cpu_permille / 10.0) + "%", 2, false);
}

// ---- cassette deck ----
// The deck shares desktop controls with the editor; its diagrams occupy recessed signal displays.

// The reel flange stays inside the ribbon's contact radius. Cutouts,
// index marks and the central spindle rotate together with the tape.
void SamplerUI::reel(float cx, float cy, float r, float ang) {
    const uint32_t metal = theme::light ? 0xff485362 : 0xff39304c;
    const uint32_t lip = theme::light ? 0xff8b9295 : 0xff807194;
    const uint32_t winding = theme::light ? 0xff605245 : 0xff574050;
    const float edge = r - 3.f;
    circle(cx, cy, edge, lip);
    circle(cx, cy, edge - 1.2f, metal);
    circle(cx, cy, r * 0.79f, GRID);
    circle(cx, cy, r * 0.75f, winding);
    circle(cx, cy, r * 0.69f, metal);

    auto point = [&](float rad, float a, float& x, float& y) {
        x = cx + rad * std::cos(a); y = cy + rad * std::sin(a);
    };
    // Three recessed, tapered openings in the rotating flange.
    for (int k = 0; k < 3; ++k) {
        const float a = ang + k * 2 * kPi / 3;
        float x0, y0, x1, y1, x2, y2, x3, y3;
        point(r * 0.30f, a - 0.27f, x0, y0);
        point(r * 0.64f, a - 0.45f, x1, y1);
        point(r * 0.64f, a + 0.45f, x2, y2);
        point(r * 0.30f, a + 0.27f, x3, y3);
        tri(x0, y0, x1, y1, x2, y2, WELL);
        tri(x0, y0, x2, y2, x3, y3, WELL);
        line(x1, y1, x2, y2, k == 0 ? TRACE_BLUE : lip, 1.3f);
        line(x0, y0, x1, y1, GRID, 1.0f);
    }
    // Short index marks on the flange make rotation visible even on the
    // smaller reel, without suggesting additional touch targets.
    for (int k = 0; k < 12; ++k) {
        const float a = ang + k * 2 * kPi / 12;
        float x0, y0, x1, y1;
        point(r * 0.83f, a, x0, y0);
        point(r * 0.88f, a, x1, y1);
        line(x0, y0, x1, y1, k == 0 ? TRACE_AMBER : lip, k == 0 ? 1.8f : 0.9f);
    }
    circle(cx, cy, r * 0.24f, lip);
    circle(cx, cy, r * 0.20f, WELL_TEXT);
    circle(cx, cy, r * 0.13f, metal);
    float x0, y0, x1, y1;
    point(r * 0.075f, ang, x0, y0);
    point(r * 0.075f, ang + kPi, x1, y1);
    line(x0, y0, x1, y1, WELL, 1.5f);
}

// The schematic of the active page, drawn where the sample is: ink lines (2.7), a dim tone for what is secondary, orange for what is live.
// Everything is computed from the same values as the four knobs, so turning a knob redraws the picture.
void SamplerUI::pageView(Rect r) {
    using P = SamplerApp;
    const int t = app_.track();
    const P::Page pg = app_.page();
    const auto ts = app_.state(t);
    const auto kn = app_.knobs(pg);
    const Rect panel{r.x, r.y + 22, r.w, r.h - 22};
    scope(panel);
    const Rect w{panel.x + 4, panel.y + 4, panel.w - 8, panel.h - 8};
    clip(w);

    const uint32_t signal = pageColor(pg, true);
    const float cy = w.y + w.h / 2, base = w.y + w.h - 4, H = w.h - 10;
    auto vbar = [&](float x, float y0, float y1, uint32_t c, float wd) { rrect({x - wd / 2, std::min(y0, y1), wd, std::max(1.f, std::fabs(y1 - y0))}, 0, c); };
    auto poly = [&](const std::vector<std::pair<float, float>>& p, uint32_t c, float wd) {
        for (size_t i = 0; i + 1 < p.size(); i++) line(p[i].first, p[i].second, p[i + 1].first, p[i + 1].second, c, wd);
        for (size_t i = 1; i + 1 < p.size(); i++) circle(p[i].first, p[i].second, wd / 2, c);
    };
    // Clean instrument schematics over the Cartesian scope grid.
    auto ring = [&](float x,float y,float rx,float ry,uint32_t color,float width) {
        std::vector<std::pair<float,float>> p;
        for(int i=0;i<=32;++i){float a=2*kPi*i/32;p.push_back({x+rx*std::cos(a),y+ry*std::sin(a)});}
        poly(p,color,width);
    };
    auto prism = [&](float x,float y,float ww,float hh,float depth,uint32_t c) {
        if(ww<1||hh<1)return;
        rrect({x,y,ww,hh},0,withAlpha(c,120));
        tri(x,y,x+depth,y-depth,x+ww+depth,y-depth,withAlpha(c,190));
        tri(x,y,x+ww+depth,y-depth,x+ww,y,withAlpha(c,190));
        tri(x+ww,y,x+ww+depth,y-depth,x+ww+depth,y+hh-depth,withAlpha(c,85));
        tri(x+ww,y,x+ww+depth,y+hh-depth,x+ww,y+hh,withAlpha(c,85));
        stroke({x,y,ww,hh},0,c,1.3f);
        line(x,y,x+depth,y-depth,c);line(x+depth,y-depth,x+ww+depth,y-depth,c);
        line(x+ww+depth,y-depth,x+ww,y,c);line(x+ww+depth,y-depth,x+ww+depth,y+hh-depth,c);
        line(x+ww+depth,y+hh-depth,x+ww,y+hh,c);
    };
    const uint32_t metal=theme::light?0xff485362:0xff39304c;
    const uint32_t lip=theme::light?0xff8b9295:0xff807194;
    const uint32_t oxide=theme::light?0xff605245:0xff574050;
    // Separate the secondary meters from the illustrated mechanism.
    auto meter=[&](float x,float y,float width,float value,uint32_t color,const char* label) {
        if(width<10)return;
        const float labelW=std::min(43.f,width*.28f);
        text(x,y+3,8,WELL_DIM,label,0,true);
        const float bx=x+labelW,bw=width-labelW;
        rrect({bx,y-1,bw,8},0,WELL);
        stroke({bx,y-1,bw,8},0,lip,.8f);
        const int n=std::max(3,int(bw/7));
        const float pitch=(bw-4)/n;
        for(int i=0;i<n;++i) {
            const float fill=clamp01(value*n-i);
            rrect({bx+2+i*pitch,y+1,std::max(.7f,pitch-1),4},0,GRID);
            if(fill>0)rrect({bx+2+i*pitch,y+1,std::max(.7f,(pitch-1)*fill),4},0,color);
        }
    };
    auto arc=[&](float x,float y,float rad,float from,float to,uint32_t c,float thickness) {
        std::vector<std::pair<float,float>> p;
        const int n=std::max(4,int(std::abs(to-from)*rad/5));
        for(int i=0;i<=n;++i){const float a=from+(to-from)*i/n;p.push_back({x+rad*std::cos(a),y+rad*std::sin(a)});}
        poly(p,c,thickness);
    };
    auto burst=[&](float x,float y,float rad,float angle,int count,float length,uint32_t c){
        for(int i=0;i<count;++i){
            const float a=angle+(i-(count-1)*.5f)*.32f;
            line(x+rad*std::cos(a),y+rad*std::sin(a),x+(rad+length)*std::cos(a),y+(rad+length)*std::sin(a),c,2.2f);
        }
    };
    auto sparkle=[&](float x,float y,float r,uint32_t c){
        tri(x-r,y,x,y-r*.32f,x+r,y,c);tri(x-r,y,x,y+r*.32f,x+r,y,c);
        tri(x,y-r,x-r*.32f,y,x,y+r,c);tri(x,y-r,x+r*.32f,y,x,y+r,c);
    };
    auto cabinet=[&](float x,float y,float ww,float hh){
        rrect({x+2,y+2,ww,hh},2,SHADOW);
        rrect({x,y,ww,hh},2,metal);stroke({x,y,ww,hh},2,lip,1.2f);
        const float cx=x+ww/2, rr=std::min(ww*.34f,hh*.22f);
        circle(cx,y+hh*.25f,rr*.57f,lip);circle(cx,y+hh*.25f,rr*.35f,WELL_TEXT);
        circle(cx,y+hh*.68f,rr+1,lip);circle(cx,y+hh*.68f,rr,WELL);circle(cx,y+hh*.68f,rr*.72f,metal);
        circle(cx,y+hh*.68f,rr*.25f,DRIVE_TRACE);
    };
    auto delayDiagram=[&](float left,float width,float time,float feedback,float send,bool ping,bool full){
        const float bottom=w.y+w.h-15,top=w.y+(full?17.f:4.f),hh=bottom-top;
        const float spw=std::min(width*.13f,hh*.32f), sph=std::min(hh*.7f,spw*1.8f);
        const float sx=left+4,sy=top+(hh-sph)/2;
        cabinet(sx,sy,spw,sph);
        const float origin=sx+spw+5,y=top+hh*.5f;
        const float available=std::max(10.f,left+width-origin-5);
        const float pitch=std::max(9.f,available*(.035f+.22f*time));
        const float r0=std::min({hh*.27f,pitch*.36f,17.f});
        for(int k=0;k<2;++k)arc(origin-2,y,4+k*4,-.7f,.7f,encoderTrace(2),1.6f);
        for(int j=0;j<32;++j){
            const float x=origin+pitch*(j+.6f);
            if(x+r0+4>left+width)break;
            const float level=std::pow(std::max(.025f,feedback),float(j));
            const float rr=std::max(2.f,r0*(.25f+.75f*std::sqrt(level)));
            const float yy=y+(ping?(j%2?1:-1)*hh*.19f:0);
            const uint32_t c=ping&&j%2?encoderTrace(3):encoderTrace(0);
            const uint32_t face=withAlpha(c,int(65+190*std::sqrt(level)*(.35f+.65f*send)));
            // One solid pulse disc, with a narrow metallic flange. No
            // offset outline or connecting trace behind the pulses.
            circle(x+1.5f,yy,rr,lip);circle(x,yy,rr,metal);
            circle(x,yy,std::max(1.f,rr-1.6f),face);
            circle(x,yy,std::max(1.f,rr*.18f),WELL_TEXT);
            if(rr>=4 && send>.001f) {
                const int marks=1+int(6*send);
                burst(x,yy,rr+2,-kPi/2,marks,2+4*send,encoderTrace(2));
                if(send>.35f)burst(x,yy,rr+2,kPi/2,1+int(4*send),2+3*send,encoderTrace(2));
            }
        }
        meter(left+4,bottom+5,width-8,send,TRACE_GREEN,"SEND");
        if(full)meter(left+4,w.y+5,std::min(width*.45f,138.f),feedback,encoderTrace(1),"FB");
    };
    auto roomDiagram=[&](float left,float width,float send,float size,float damp,float shimmer,bool full){
        const float x=left+4,y=w.y+4,ww=width-8,hh=w.h-21;
        if(ww<20||hh<16)return;
        const float wall=std::min(7.f,hh*.12f);
        rrect({x,y,ww,hh},1,metal);stroke({x,y,ww,hh},1,lip,1.4f);
        rrect({x+wall,y+wall,ww-2*wall,hh-2*wall},0,WELL);
        stroke({x+wall,y+wall,ww-2*wall,hh-2*wall},0,lip,.8f);
        // Fixed rectangular room with acoustic panels. Size is the
        // reach of the sound bands, not a change of aspect ratio.
        for(int j=0;j<5;++j){
            const float bx=x+ww*(.21f+j*.14f);
            rrect({bx,y+2,ww*.085f,std::max(2.f,wall-3)},0,GRID);
            rrect({bx,y+hh-wall+1,ww*.085f,std::max(2.f,wall-3)},0,GRID);
        }
        const float sw=std::min(ww*.16f,hh*.27f),sh=std::min(hh*.58f,sw*1.85f);
        const float sx=x+wall+3,sy=y+(hh-sh)*.5f, ox=sx+sw+2,oy=y+hh*.5f;
        cabinet(sx,sy,sw,sh);
        const float span=std::max(5.f,x+ww-wall-ox-7);
        const float reach=span*(.23f+.65f*size);
        const int bands=5+int(5*send);
        for(int j=0;j<bands;++j){
            const float q=(j+1)/float(bands),xx=ox+reach*q;
            const float amplitude=std::min(hh*.5f-wall-4,hh*(.10f+.25f*q))*(1.f-.52f*damp);
            const float bow=std::min(8.f,std::max(2.f,reach/14));
            const float fade=(1.f-.85f*damp)*(.96f-.50f*q);
            const uint32_t c=j<2?encoderTrace(0):withAlpha(encoderTrace(1),int(60+195*fade*(.35f+.65f*send)));
            // Damp colors the two ends of each wave, growing inward.
            // At 40% damp, each purple end spans 20% of the wave.
            auto wavePoint=[&](float t){
                const float a=kPi*.8f*(t-.5f);
                return std::pair<float,float>{xx+bow*std::cos(a),oy+amplitude*std::sin(a)};
            };
            auto waveSection=[&](float from,float to,uint32_t color){
                if(to-from<.0001f)return;
                std::vector<std::pair<float,float>> points;
                const int steps=std::max(2,int(std::ceil((to-from)*24)));
                for(int k=0;k<=steps;++k)points.push_back(wavePoint(from+(to-from)*k/steps));
                poly(points,color,1.8f+1.3f*fade);
            };
            const float edge=.5f*clamp01(damp);
            waveSection(edge,1.f-edge,c);
            waveSection(0.f,edge,encoderTrace(2));
            waveSection(1.f-edge,1.f,encoderTrace(2));
        }
        // Stable pseudorandom positions across the room; increasing shimmer
        // reveals more stars without moving existing stars on every frame.
        if(shimmer>.001f) {
            uint32_t seed=0x9e3779b9u;
            auto random=[&](){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return float(seed&0xffffffu)/16777215.f;};
            const int count=1+int(23*shimmer);
            const float left=ox+5,right=std::max(left,x+ww-wall-7);
            const float upper=y+wall+6,lower=std::max(upper,y+hh-wall-6);
            for(int i=0;i<count;++i) {
                const float xx=left+(right-left)*random(),yy=upper+(lower-upper)*random();
                const float rad=1.5f+3.f*shimmer*random();
                sparkle(xx,yy,rad,encoderTrace(3));
            }
        }
        const float my=y+hh+6;
        meter(left+4,my,full?width*.54f-8:width-8,send,encoderTrace(0),"SEND");
        if(full)meter(left+width*.56f,my,width*.44f-4,damp,encoderTrace(2),"DAMP");
    };
    auto tapeDiagram=[&](bool echo){
        const float hh=w.h-12,ww=w.w,mid=w.y+hh*.49f;
        const float a=kn[0].norm,b=kn[1].norm,c=kn[2].norm,d=kn[3].norm;
        const float maxR=std::min(hh*.37f,ww*.135f);
        const float r1=maxR*(.68f+.32f*(echo?a:b)),r2=maxR*(.46f+.42f*(echo?b:c));
        const float capR=std::max(3.f,maxR*.17f);
        struct Post{float x,y,r;};
        const Post p[3]={{w.x+ww*.20f,mid-1,r1},{w.x+ww*.80f,mid+2,r2},
            {w.x+ww*(.44f+.12f*(echo?c:d)),w.y+hh*.84f,capR}};
        // Common external tangents and circular arcs for a closed tape
        // belt. The radii/roller position are symbolic controls only.
        float nx[3],ny[3];
        for(int i=0;i<3;++i){
            const auto& q=p[(i+1)%3];const auto& z=p[i];
            const float dx=q.x-z.x,dy=q.y-z.y,dist=std::hypot(dx,dy);
            const float k=std::clamp((z.r-q.r)/std::max(1.f,dist),-.98f,.98f),v=std::sqrt(1-k*k);
            nx[i]=(dx*k+dy*v)/dist;ny[i]=(dy*k-dx*v)/dist;
        }
        std::vector<std::pair<float,float>> belt;
        for(int i=0;i<3;++i){
            const auto& z=p[i];const int prev=(i+2)%3;
            float from=std::atan2(ny[prev],nx[prev]),to=std::atan2(ny[i],nx[i]);
            while(to<from)to+=2*kPi;
            for(int j=0;j<=12;++j){const float angle=from+(to-from)*j/12;
                belt.push_back({z.x+z.r*std::cos(angle),z.y+z.r*std::sin(angle)});}
        }
        belt.push_back(belt.front());
        if(!echo&&b>.001f)poly(belt,withAlpha(DRIVE_TRACE,int(35+95*b)),4.f+7*b);
        poly(belt,oxide,4.4f);poly(belt,!echo&&b>.001f?DRIVE_TRACE:TRACE_AMBER,1.6f+(!echo?b:0));
        // Derive rotation from transport position, not elapsed UI time.
        const float ph=loopPos()*4*kPi;
        reel(p[0].x,p[0].y,r1,ph);
        reel(p[1].x,p[1].y,r2,ph*r1/std::max(1.f,r2));
        circle(p[2].x,p[2].y,capR,lip);circle(p[2].x,p[2].y,capR*.76f,metal);circle(p[2].x,p[2].y,capR*.38f,TRACE_GREEN);
        const float capPhase=ph*r1/std::max(1.f,capR)+(echo?c*2*kPi:0.f);
        line(p[2].x,p[2].y,p[2].x+capR*.7f*std::cos(capPhase),p[2].y+capR*.7f*std::sin(capPhase),WELL_TEXT,1.5f);
        if(echo&&c>.001f) {
            burst(p[2].x,p[2].y,capR+3,kPi/2,1+int(5*c),2+5*c,encoderTrace(2));
            burst(p[0].x,p[0].y,r1+3,-kPi/2,1+int(4*c),2+4*c,encoderTrace(2));
        }
        const float hx=(p[0].x+p[1].x)*.5f,hy=(p[0].y+p[1].y)*.5f-(r1+r2)*.5f;
        const float hw=std::min(24.f,ww*.10f), ht=std::min(24.f,hh*.32f);
        rrect({hx-hw/2,hy-ht*.25f,hw,ht},1,metal);
        stroke({hx-hw/2,hy-ht*.25f,hw,ht},1,lip,1.2f);
        rrect({hx-2,hy-ht*.17f,4,ht*.82f},0,!echo&&b>.001f?DRIVE_TRACE:WELL_DIM);
        for(float dx:{-hw*.32f,hw*.32f})for(float dy:{-.13f,.62f})circle(hx+dx,hy+ht*dy,1.2f,lip);
        // Energy marks are outside the tape belt, not distortions of it.
        const float motion=echo?a:a*.65f+.35f*d;
        if(motion>.001f)for(int j=0;j<2;++j)arc(p[0].x,p[0].y,r1+3+j*4,2.9f,3.1f+.9f*motion,encoderTrace(0),1.5f+motion);
        const float accent=echo?b:c;
        if(accent>.001f)burst(p[1].x,p[1].y,r2+3,-.85f,2+int(4*accent),3+6*accent,encoderTrace(1));
        if(d>.001f) {
            if(!echo){for(int j=0;j<2;++j)arc(p[2].x,p[2].y,capR+3+j*3,-2.65f,-.5f,TRACE_GREEN,1.2f+d);}
            else {
                // One zigzag per cell, distributed across the tape window.
                // A coprime stride spreads the first marks out at low hiss.
                const int count=1+int(19*d);
                for(int j=0;j<count;++j){
                    const int cell=(j*7+3)%20;
                    const float xx=w.x+ww*(.08f+.84f*((cell%5+.5f)/5.f));
                    const float yy=w.y+hh*(.10f+.78f*((cell/5+.5f)/4.f));
                    const float z=2.f+2.f*d;
                    if(std::hypot(xx-p[0].x,yy-p[0].y)<r1+z ||
                       std::hypot(xx-p[1].x,yy-p[1].y)<r2+z ||
                       std::hypot(xx-p[2].x,yy-p[2].y)<capR+z)continue;
                    const float sign=j%2?1.f:-1.f;
                    poly({{xx-1.5f*z,yy+sign*z},{xx-.5f*z,yy-sign*z},{xx+.4f*z,yy+sign*.5f*z},{xx+1.5f*z,yy-sign*.7f*z}},encoderTrace(3),1.3f);
                }
            }
        }
        if(echo&&c>.001f)for(int j=0;j<int(12*c);++j){
            const float angle=j*2.39f;const float rr=r1*(.78f+.07f*std::fmod(j*.37f,1.f));
            line(p[0].x+rr*std::cos(angle),p[0].y+rr*std::sin(angle),p[0].x+(rr+2)*std::cos(angle+.08f),p[0].y+(rr+2)*std::sin(angle+.08f),encoderTrace(2),1);
        }
        const float my=w.y+w.h-6;
        meter(w.x+3,my,ww*.48f-5,echo?c:b,echo?encoderTrace(2):DRIVE_TRACE,echo?"AGE":"DRIVE");
        meter(w.x+ww*.51f,my,ww*.49f-3,echo?d:c,echo?encoderTrace(3):TRACE_VIOLET,echo?"HISS":"LO-FI");
    };
    std::string head;
    switch (pg) {
    case P::P_EFILTER: {
        head=ts.echoInsert.filterMix>.001f?"ECHOFIELD FILTER":"FILTER / BYPASSED";
        const float cutoff=kn[1].norm,res=kn[2].norm,drive=kn[3].norm;
        const float x0=w.x+7,x1=w.x+w.w-7,y0=w.y+5,y1=w.y+w.h-18;
        rrect({x0,y0,x1-x0,y1-y0},1,metal);stroke({x0,y0,x1-x0,y1-y0},1,lip,1.2f);
        rrect({x0+4,y0+4,x1-x0-8,y1-y0-8},0,WELL);
        std::vector<std::pair<float,float>> pts;
        for(int i=0;i<=64;++i){
            const float f=i/64.f,delta=(f-cutoff)*16,low=1/(1+std::exp(delta)),band=std::exp(-delta*delta*.14f);
            float v=ts.echoInsert.filter==0?low:ts.echoInsert.filter==1?band:1-low;
            v=clamp01(v+.35f*res*band);
            const float x=x0+13+f*std::max(1.f,x1-x0-26);
            const float lo=y0+std::min(22.f,(y1-y0)*.40f),hi=y1-std::min(12.f,(y1-y0)*.23f);
            const float y=hi-v*std::max(1.f,hi-lo);
            pts.push_back({x,y});
            if(i%8==0)line(x,y0+5,x,y1-5,GRID,.8f);
        }
        poly(pts,signal,2.5f);
        if(drive>.001f) {
            // Fixed, irregularly spaced motion strokes along the entire
            // response. Thresholds are nested so increasing drive never
            // removes existing marks or rearranges the layout.
            struct Cluster {int center,half,layers;float onset,side;};
            constexpr Cluster clusters[]={
                {3,2,3,.06f,1.f},{7,1,1,.30f,-1.f},
                {11,2,4,.12f,-1.f},{15,1,2,.48f,1.f},
                {20,3,5,.04f,1.f},{25,1,1,.70f,-1.f},
                {29,2,3,.17f,-1.f},{34,2,4,.09f,1.f},
                {39,1,1,.38f,1.f},{43,2,5,.03f,-1.f},
                {48,2,2,.55f,1.f},{53,2,4,.14f,1.f},
                {57,1,1,.76f,-1.f},{61,2,3,.10f,-1.f}
            };
            auto contourStroke=[&](const Cluster& cl,int layer,float side){
                std::vector<std::pair<float,float>> contour;
                const float gap=3.f+2.1f*(layer-1);
                // Outer layers are shorter, like comic-book speed lines.
                const int half=std::max(1,cl.half-(layer>=4?1:0));
                for(int j=std::max(1,cl.center-half);j<=std::min(63,cl.center+half);++j){
                    const float dx=pts[j+1].first-pts[j-1].first,dy=pts[j+1].second-pts[j-1].second;
                    const float len=std::max(1.f,std::hypot(dx,dy));
                    const float xx=pts[j].first-side*dy/len*gap,yy=pts[j].second+side*dx/len*gap;
                    if(xx<x0+5||xx>x1-5||yy<y0+5||yy>y1-5){
                        if(contour.size()>1)poly(contour,encoderTrace(3),1.25f);
                        contour.clear();continue;
                    }
                    contour.push_back({xx,yy});
                }
                if(contour.size()>1)poly(contour,encoderTrace(3),1.25f);
            };
            for(const auto& cl:clusters){
                for(int layer=1;layer<=cl.layers;++layer){
                    const float threshold=cl.onset+(1.f-cl.onset)*.82f*(layer-1)/std::max(1,cl.layers-1);
                    if(drive<threshold)continue;
                    contourStroke(cl,layer,cl.side);
                    if(cl.layers>=3 && drive>=threshold+.11f && layer<cl.layers)
                        contourStroke(cl,layer,-cl.side);
                }
            }
        }
        meter(w.x+4,w.y+w.h-6,w.w-8,drive,encoderTrace(3),"DRIVE");
        break; }
    case P::P_EDELAY:
        head="ECHOFIELD DELAY";
        delayDiagram(w.x,w.w,kn[0].norm,std::min(.92f,kn[1].norm),kn[2].norm,kn[3].norm>.5f,true);
        break;
    case P::P_EREVERB:
        head="ECHOFIELD FDN + SHIMMER";
        roomDiagram(w.x,w.w,kn[0].norm,kn[1].norm,kn[2].norm,kn[3].norm,true);
        break;
    case P::P_ETAPE:
        head=ts.echoInsert.tape>.001f?"ECHOFIELD TAPE":"TAPE / BYPASSED";
        tapeDiagram(true);break;
    case P::P_EMIX: {
        head="ECHOFIELD INSERT MIX";
        const float x=w.x,ww=w.w,y=w.y+7,h=w.h-25,mid=y+h*.5f;
        const float mw=ww*.29f,x1=x+ww*.09f,x2=x+ww*.44f;
        line(x+3,mid,x+ww*.82f,mid,lip,2.2f);
        for(int j=0;j<2;++j){
            const float xx=j?x2:x1;const uint32_t c=encoderTrace(j+2);
            rrect({xx,y,mw,h},2,metal);stroke({xx,y,mw,h},2,lip,1.2f);
            for(float dx:{3.f,mw-3})for(float dy:{3.f,h-3})circle(xx+dx,y+dy,1.2f,lip);
            circle(xx,mid,3,lip);circle(xx,mid,1.5f,WELL);
            circle(xx+mw,mid,3,lip);circle(xx+mw,mid,1.5f,c);
            text(xx+mw*.5f,y+h*.20f,9,c,j?"02 / FILTER":"01 / TAPE",1,true);
            meter(xx+5,y+h*.55f,mw-10,kn[j+2].norm,c,"MIX");
            text(xx+mw*.5f,y+h*.81f,9,WELL_TEXT,kn[j+2].value,1,true);
        }
        const float ox=x+ww*.82f,ex=x+ww*.95f,sep=std::min(h*.4f,4+kn[0].norm*h*.34f);
        line(ox,mid,ox+4,mid,TRACE_BLUE,2);line(ox+4,mid-sep,ox+4,mid+sep,TRACE_BLUE,2);
        for(float yy:{mid-sep,mid+sep}) {
            line(ox+4,yy,ex,yy,TRACE_BLUE,2);
            const float rad=2.f+3.f*kn[0].norm;
            circle(ex,yy,rad+1,lip);circle(ex,yy,rad,TRACE_BLUE);
            circle(ex,yy,std::max(1.f,rad*.35f),WELL);
            for(int i=0;i<1+int(4*kn[0].norm);++i) {
                const float a=(yy<mid?-1.f:1.f)*kPi/2+(i-2)*.35f;
                circle(ex+(rad+3)*std::cos(a),yy+(rad+3)*std::sin(a),1.1f,TRACE_BLUE);
            }
        }
        meter(x+5,w.y+w.h-6,ww*.75f,kn[1].norm,TRACE_GREEN,"LEVEL");
        text(ex,w.y+w.h-6,8,TRACE_BLUE,kn[0].value,1,true);
        break; }
    case P::P_FX:
        head="REVERB + DELAY";
        roomDiagram(w.x,w.w*.48f,kn[0].norm,kn[1].norm,0,0,false);
        delayDiagram(w.x+w.w*.51f,w.w*.49f,kn[3].norm,.78f,kn[2].norm,false,false);
        break;
    case P::P_ENV: {   // the ADSR curve
        head = "ENVELOPE";
        const float sa = 0.08f + 0.5f * ts.a, sd = 0.08f + 0.5f * ts.d, ss = 0.3f, sr = 0.08f + 0.5f * ts.r, tot = sa + sd + ss + sr;
        const int n = std::max(2, (int)(w.w / 4)); std::vector<std::pair<float, float>> pts;
        const float x0 = w.x + 2, span = w.w - 4;
        for (int i = 0; i <= n; i++) {
            const float u = (float)i / n * tot; float v;
            if (u < sa) v = u / sa; else if (u < sa + sd) v = 1.f - (1.f - ts.s) * (u - sa) / sd; else if (u < sa + sd + ss) v = ts.s; else v = ts.s * (1.f - (u - sa - sd - ss) / sr);
            const float x = x0 + (float)i / n * span, y = base - clamp01(v) * (H - 10);
            vbar(x, base, y, GRID, 2.4f); pts.push_back({x, y});
        }
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const float u = (i + 0.5f) / n * tot;
            const int stage = u < sa ? 0 : u < sa + sd ? 1 : u < sa + sd + ss ? 2 : 3;
            line(pts[i].first, pts[i].second, pts[i + 1].first, pts[i + 1].second, encoderTrace(stage), 2.7f);
        }
        const float ub[4] = {0, sa, sa + sd, sa + sd + ss}; const char* nm[4] = {"A", "D", "S", "R"};
        for (int i = 0; i < 4; i++) { const float x = x0 + ub[i] / tot * span; if (i) vbar(x, w.y + 12, base, WELL_DIM, 1.2f); text(x + 5, w.y + 6, 9, encoderTrace(i), nm[i], 0, true); }
        break; }
    case P::P_LFO: {   // the shape over a few cycles (rate), its height is the depth; the orange bar on the right is the live value
        head = std::string("LFO > ") + kn[2].value;
        const float depth = 0.1f + 0.9f * ts.lfoDepth, cyc = 1.5f + 4.5f * ts.lfoRate, amp = (w.h / 2 - 6) * depth;
        for (float x = w.x; x < w.x + w.w - 14; x += 8) rrect({x, cy - 0.7f, 4, 1.4f}, 0, WELL_DIM);
        const int n = std::max(2, (int)((w.w - 20) / 3)); std::vector<std::pair<float, float>> pts;
        unsigned s = 11; float held = 0; int lastStep = -1;
        for (int i = 0; i <= n; i++) {
            const float ph = std::fmod(cyc * i / n, 1.f); float v;
            if (ts.lfoShape == 0) v = std::sin(2 * kPi * ph);
            else if (ts.lfoShape == 1) v = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph;
            else if (ts.lfoShape == 2) v = ph < 0.5f ? 1.f : -1.f;
            else { const int st = (int)(cyc * i / n * 4); if (st != lastStep) { lastStep = st; s = s * 1103515245u + 12345u; held = ((s >> 16 & 255) / 127.5f) - 1.f; } v = held; }
            pts.push_back({w.x + 2 + (float)i / n * (w.w - 20), cy - v * amp});
        }
        
        std::vector<std::pair<float,float>> shadow;
        for(const auto& p:pts)shadow.push_back({p.first+3,p.second-3});
        poly(shadow,GRID,1.2f);poly(pts, signal, 2.7f);
        const float lv = st_.lfoValue[t] * amp;
        vbar(w.x + w.w - 5, cy, cy - lv, encoderTrace(ts.lfoTarget), 4);
        for(int j=0;j<4;++j)rrect({w.x+w.w-12,w.y+3+j*7,4,4},0,j==ts.lfoTarget?encoderTrace(j):GRID);
        break; }
    case P::P_ARP: {   // the notes the arpeggiator plays: order from the pattern, count from the octaves, width from the length; dim while it is off
        head = std::string("ARP ") + kn[1].value;
        const int n = 3 * (ts.arpOct + 1); std::vector<int> seq;
        if (ts.arpPatt == 0) for (int i = 0; i < n; i++) seq.push_back(i);
        else if (ts.arpPatt == 1) for (int i = n - 1; i >= 0; i--) seq.push_back(i);
        else if (ts.arpPatt == 2) { for (int i = 0; i < n; i++) seq.push_back(i); for (int i = n - 2; i > 0; i--) seq.push_back(i); }
        else if (ts.arpPatt == 3) { for (int i = 0, j = n - 1; i <= j; i++, j--) { seq.push_back(i); if (i != j) seq.push_back(j); } }
        else { unsigned s = 5; for (int i = 0; i < std::max(8, n); i++) { s = s * 1103515245u + 12345u; seq.push_back((int)(s >> 16) % n); } }
        // Fixed-width overview: faster divisions repeat the pattern
        // more often, rather than squeezing a fixed number into the left.
        static constexpr int counts[]={6,6,8,10,12,15,18,22,26,32,38,48,64};
        const int steps=std::min(counts[std::clamp(ts.arpRate,0,12)],std::max(6,int(w.w/4)));
        const float pitch=(w.w-12)/steps,bw=std::max(1.f,pitch*(.12f+.82f*ts.arpLen));
        const uint32_t c = ts.arpRate == 0 ? GRID : signal;
        
        for(int i=0;i<steps;++i){
            const float height=(.18f+.66f*(seq[i%seq.size()]+1)/n)*H;
            prism(w.x+6+i*pitch,base-height,bw,height,std::min({5.f,pitch*.35f,H*.08f}),c);
        }
        break; }
    case P::P_SEQ:
        head="LOOP SPEED";
        line(w.x+8,cy,w.x+w.w-8,cy,lip,2);
        for(int i=0;i<=12;++i)line(w.x+8+i*(w.w-16)/12,cy-5,w.x+8+i*(w.w-16)/12,cy+5,GRID,1);
        circle(w.x+8+kn[0].norm*(w.w-16),cy,5,TRACE_AMBER);
        text(w.x+w.w/2,cy-17,12,TRACE_AMBER,kn[0].value,1,true);
        break;
    case P::P_MIX: {   // the last four seconds of the active channel: its level as a mirrored wave (dim = what it would be, ink = what is left after the sidechain),
                       // the level of the source as an envelope with the threshold line, orange where the source is over the threshold and the track is ducked
        const int src = ts.scSrc - 1; const bool sc = src >= 0 && src != t;
        head = "TRACK " + std::to_string(t + 1) + (sc ? "  DUCKED BY " + std::to_string(src + 1) : "  SIDECHAIN OFF");
        if (hTrack_ != t) { hTrack_ = t; for (int i = 0; i < kHist; i++) hOwn_[i] = hSrc_[i] = 0.f, hGain_[i] = 1.f; hAt_ = now_; }
        if (now_ - hAt_ > 400) hAt_ = now_ - 33;
        while (now_ - hAt_ >= 33) { hAt_ += 33; hOwn_[hPos_] = st_.scLevel[t]; hSrc_[hPos_] = sc ? st_.scLevel[src] : 0.f; hGain_[hPos_] = st_.scGain[t]; hPos_ = (hPos_ + 1) % kHist; }
        const float half = w.h / 2 - 6, pitch = w.w / kHist;
        auto f = [&](float a) { return std::pow(std::min(1.f, std::max(0.f, a)), 0.5f) * half; };
        const float thrLin = std::pow(10.f, (-40.f * (1.f - kn[2].norm)) / 20.f);
        for (int i = 0; i < kHist; i++) {
            const int k = (hPos_ + i) % kHist; const float x = w.x + i * pitch;
            const float full = f(hOwn_[k]), left = f(hOwn_[k] * hGain_[k]);
            if (full > 0.5f) rrect({x, cy - full, pitch * 0.62f, 2 * full}, 0, GRID);
            if (left > 0.5f) rrect({x, cy - left, pitch * 0.62f, 2 * left}, 0, trackTrace(t));
        }
        if (sc) {
            for (float x = w.x; x < w.x + w.w; x += 10) rrect({x, cy - f(thrLin) - 0.7f, std::min(5.f, w.x + w.w - x), 1.4f}, 0, TRACE_AMBER);   // the threshold
            for (int i = 0; i + 1 < kHist; i++) {
                const int k0 = (hPos_ + i) % kHist, k1 = (hPos_ + i + 1) % kHist;
                const uint32_t c = hSrc_[k0] > thrLin || hSrc_[k1] > thrLin ? TRACE_AMBER : WELL_DIM;
                line(w.x + 1.2f + i * (w.w - 2.4f) / (kHist - 1), cy - f(hSrc_[k0]), w.x + 1.2f + (i + 1) * (w.w - 2.4f) / (kHist - 1), cy - f(hSrc_[k1]), c, 2.2f);
            }
            const float gdb = 20.f * std::log10(std::max(0.01f, st_.scGain[t]));
            if (gdb < -0.5f) text(w.x + w.w - 4, w.y + 6, 11, TRACE_AMBER, num("%.0f DB", gdb), 2, true);
        }
        // A small persistent routing strip complements the live history.
        const float yy=w.y+w.h-4, ww=w.w*.22f;
        rrect({w.x+2,yy-2,ww,2},0,GRID);
        if(ts.vol>0)rrect({w.x+2,yy-2,ww*ts.vol,2},0,TRACE_BLUE);
        for(int j=0;j<8;++j)rrect({w.x+w.w*.31f+j*7,yy-5,5,5},0,sc&&j==src?trackTrace(j):GRID);
        const float ratioW=w.w*.23f, rx=w.x+w.w-ratioW-3;
        line(rx,yy-1,rx+ratioW*.45f,yy-11,TRACE_AMBER,1.2f);
        line(rx+ratioW*.45f,yy-11,rx+ratioW,yy-11-(1-ts.scRatio)*9,sc?TRACE_AMBER:WELL_DIM,1.2f);
        break; }
    case P::P_KEY: {
        const auto sc=app_.scale();
        head=std::string(P::noteName(sc.root))+" "+P::scaleName(sc.scale);
        const uint16_t ghosts=app_.chordGhostMask();
        const uint32_t chordColors[]={TRACE_BLUE,TRACE_GREEN,TRACE_GREEN,TRACE_VIOLET,TRACE_PINK,TRACE_VIOLET,TRACE_PINK,WELL_DIM};
        const uint32_t chord=chordColors[std::clamp(sc.scale,0,7)];
        const float pitch=(w.w-8)/12,kh=std::min(w.h*.51f,43.f),ky=cy-kh*.6f;
        for(int i=0;i<12;++i){
            const int rel=(i-sc.root+12)%12;
            const bool legal=P::inScale(sc.scale,rel),ghost=ghosts&(1u<<i);
            const uint32_t c=rel==0?DRIVE_TRACE:legal?chord:WELL_DIM;
            const float x=w.x+4+i*pitch;
            if(ghost) {
                // Static comic-book motion lines, offset from the key
                // silhouette. No time-dependent animation or ghost fill.
                const float cx=x+pitch*.5f,span=std::min(pitch*.37f,8.f);
                const float room=std::max(0.f,ky-w.y-9);
                for(int j=0;j<3;++j) {
                    const float lift=2.f+j*2.8f;
                    if(lift+3>room)break;
                    const float reach=span*(1.f-.12f*j);
                    poly({{cx-reach,ky-lift},{cx-reach*.65f,ky-lift-2.5f},
                          {cx,ky-lift-3.2f},{cx+reach*.7f,ky-lift-1.4f}},c,1.35f);
                }
                if(ky+kh+5<w.y+w.h-10)
                    poly({{cx-span*.8f,ky+kh+2},{cx-span*.3f,ky+kh+4},
                          {cx+span*.35f,ky+kh+4.5f},{cx+span*.8f,ky+kh+2}},c,1.3f);
            }
            rrect({x+2,ky,pitch-4,kh},2,legal?withAlpha(c,185):metal);
            stroke({x+2,ky,pitch-4,kh},2,legal?c:lip,1);
            if(legal)rrect({x+4,ky+kh-5,std::max(1.f,pitch-8),3},0,c);
            text(x+pitch/2,ky+kh+9,8,c,P::noteName(i),1,true);
        }
        const float ox=w.x+6+(sc.octave+3)*(w.w-12)/6;
        line(w.x+6,w.y+4,w.x+w.w-6,w.y+4,lip,1);
        for(int i=0;i<7;++i)circle(w.x+6+i*(w.w-12)/6,w.y+4,1.5f,lip);
        circle(ox,w.y+4,3,TRACE_BLUE);
        break; }
    case P::P_SAVE: {   // what a take records: the input level against the trigger line, the source in big dim letters
        head = std::string("SOURCE: ") + kn[1].value;
        
        const float thr = app_.micThreshold01(), lv = std::min(1.f, st_.inputPeak * 2.f), x0 = w.x + 4, ww = w.w - 8;
        rrect({x0, cy - 5, ww, 10}, 0, GRID);
        rrect({x0, cy - 5, std::max(2.f, ww * lv), 10}, 0, lv > std::min(1.f, 2.f * app_.micThreshold()) ? TRACE_RED : TRACE_GREEN);
        vbar(x0 + ww * thr, cy - 26, cy + 26, TRACE_AMBER, 2.7f);
        // Source selector: a capsule for mic, a small waveform for sound.
        const float sx=w.x+w.w*.92f, sy=w.y+12;
        if(kn[1].norm<.5f){
            rrect({sx-4,sy-7,8,13},3,TRACE_RED);line(sx-7,sy+5,sx+7,sy+5,TRACE_RED,1.5f);
            line(sx,sy+5,sx,sy+10,TRACE_RED,1.5f);
        }else{
            for(int j=0;j<7;++j)vbar(sx+(j-3)*3,sy-3-std::abs(std::sin(j*1.1f))*5,sy+3+std::abs(std::sin(j*1.1f))*5,TRACE_GREEN,2);
        }
        text(x0, w.y + w.h - 8, 9, WELL_DIM, "TRIGGER LEVEL " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), 0, true);
        break; }
    case P::P_TEMPO:
    case P::P_LEN: {   // the loop as bars: one block per bar with its beats, the orange playhead
        const int bars = std::max(1, st_.loopBars), shown = std::min(bars, 16);
        head = bars > 0 && st_.loopSamples > 0 ? std::to_string(bars) + (bars == 1 ? " BAR  " : " BARS  ") + num("%.0f BPM", st_.bpm) : "NO LOOP";
        
        const float gap = 4, bw = (w.w - gap * (shown - 1)) / shown, bh = std::min(44.f,w.h*.55f);
        for (int b = 0; b < shown; b++) {
            const float x = w.x + b * (bw + gap);
            prism(x,cy-bh/2,std::max(1.f,bw-2),bh,2,st_.loopSamples>0?signal:GRID);
            for (int q = 1; q < 4; q++) vbar(x + bw * q / 4, cy - bh / 2 + 4, cy + bh / 2 - 4, WELL, 1.2f);
        }
        if (st_.loopSamples > 0) {
            vbar(w.x+2+clamp01(loopPos())*(w.w-4),cy-34,cy+34,TRACE_AMBER,2.7f);
            const float beatPitch=8+26*(1-clamp01((st_.bpm-30.f)/270.f));
            for(float x=w.x+4;x<w.x+w.w-3;x+=beatPitch)rrect({x,base-2,2,2},0,TRACE_AMBER);
            if(st_.tempoStretch)line(w.x+4,w.y+5,w.x+w.w-4,w.y+5,TRACE_VIOLET,2);
        }
        break; }
    default: break;
    }
    endClip();
    const int nv = st_.voiceN[t];
    strip({r.x, r.y, r.w, 18}, pageColor(pg), fit(head, r.w - (nv > 0 ? 90 : 12), 11), 11);
    if (nv > 0) text(r.x + r.w - 4, r.y + 9, 10, contrastInk(pageColor(pg)), std::to_string(nv) + (nv == 1 ? " VOICE" : " VOICES"), 2, true);
}

// The sample of the selected track: its waveform (the playback window bright), the playback mode, and a line for every sounding voice.
void SamplerUI::sampleView(Rect r) {
    const int t = app_.track();
    const auto ts = app_.state(t);
    const int micS = app_.micState();

    if (app_.page() != SamplerApp::P_HOME && micS == 0) { pageView(r); return; }
    const uint32_t header = micS ? RED : ACCENT;
    const std::string mode = ts.algo == 4 ? std::string("GRANULAR / ") + SamplerApp::echoModeName(ts.echo.mode) : SamplerApp::algoName(ts.algo);
    const float headerSpace = st_.voiceN[t] > 0 ? (ts.algo <= 1 ? 100.f : 80.f) : ts.algo <= 1 ? 30.f : 10.f;
    strip({r.x, r.y, r.w, 18}, header, micS == 1 ? "MIC: WAITING FOR SOUND" : micS == 2 ? "MIC: RECORDING " + num("%.1f", st_.sampleSeconds) + " S" : micS == 3 ? "MIC: SAVING" : fit(mode, r.w - headerSpace, 11), 11);
    tap({r.x, r.y, 120, 18}, [this, ts] { app_.setAlgo((ts.algo + 1) % 6); });   // tap the mode: the next one
    if (ts.algo <= 1) {   // auto-tune (MELODIC, ONE-SHOT): a tuning fork at the right end of the header; orange while the sample is tuned
        const float ax = r.x + r.w - 10, ay = r.y + 8; const uint32_t ac = app_.tuned(t) ? TRACE_GREEN : contrastInk(header);
        line(ax - 4, ay - 5, ax - 4, ay, ac, 2.2f); line(ax + 4, ay - 5, ax + 4, ay, ac, 2.2f);
        line(ax - 4, ay, ax - 2, ay + 3, ac, 2.2f); line(ax - 2, ay + 3, ax + 2, ay + 3, ac, 2.2f); line(ax + 2, ay + 3, ax + 4, ay, ac, 2.2f);
        line(ax, ay + 3, ax, ay + 7, ac, 2.2f);
        tap({ax - 16, r.y - 6, 32, 32}, [this] { app_.autoTune(); });
    }
    const int nv = st_.voiceN[t];
    if (nv > 0) text(r.x + r.w - (ts.algo <= 1 ? 26 : 0), r.y + 8, 11, contrastInk(header), std::to_string(nv) + (ts.algo == 4 ? (nv == 1 ? " GRAIN" : " GRAINS") : (nv == 1 ? " VOICE" : " VOICES")), 2, true);
    const Rect panel{r.x, r.y + 22, r.w, r.h - 22};
    scope(panel);
    const Rect w{panel.x + 3, panel.y + 3, panel.w - 6, panel.h - 6};
    clip(w);
    if (micS == 1 || micS == 2) {   // the microphone take: the level and the trigger line while it waits, the growing take while it records; one big button ends it
        const float cy0 = w.y + w.h / 2, half = w.h / 2 - 3;
        Rect sb{w.x + w.w - 118, cy0 - 17, 118, 34};
        if (micS == 1) {
            const float thr = std::min(1.f, app_.micThreshold() * 2.f), lv = std::min(1.f, st_.inputPeak * 2.f);
            rrect({w.x, cy0 - lv * half, w.w, std::max(2.f, lv * half * 2)}, 0, lv > thr ? TRACE_RED : GRID);
            line(w.x, cy0 - thr * half, w.x + w.w, cy0 - thr * half, TRACE_RED, 1.5f);
            line(w.x, cy0 + thr * half, w.x + w.w, cy0 + thr * half, TRACE_RED, 1.5f);
            text(w.x + 4, w.y + w.h - 8, 9, WELL_DIM, fit("DRAG TO SET LEVEL  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), w.w - 8, 9), 0, true);
            auto setThr = [this, half, cy0](float, float y) { app_.setMicThreshold01(std::log10(std::max(0.0025f, std::fabs(y - cy0) / half / 2.f) / 0.005f) / 2.f); };
            hit({w.x, w.y, w.w - 124, w.h}, setThr, setThr);
        } else {
            float live[2048]; const int n = app_.session().livePeaks(live, 2048);
            const int cols = std::max(20, (int)(w.w / 3)), per = std::max(1, (n + cols - 1) / cols);
            const float bw0 = w.w / std::max(cols, (n + per - 1) / per);
            float lmax = 0.05f; for (int j = 0; j < n; j++) lmax = std::max(lmax, live[j]);   // normalised to the loudest part so far
            for (int i = 0; i * per < n; i++) {
                float v = 0; for (int j = i * per; j < std::min(n, (i + 1) * per); j++) v = std::max(v, live[j]);
                v = std::min(1.f, v / lmax);
                rrect({w.x + i * bw0, cy0 - std::max(1.f, v * half), std::max(1.f, bw0 - 1), std::max(2.f, v * half * 2)}, 0, TRACE_GREEN);
            }
        }
        rrect(sb, 6, WELL);
        rrect({sb.x + 12, sb.y + 11, 12, 12}, 1, TRACE_RED);
        text(sb.x + 32, sb.y + 17, 12, TRACE_RED, micS == 1 ? "CANCEL" : "STOP", 0, true);
        tap(sb, [this] { app_.micSample(); });
        endClip();
        return;
    }
    const int bins = std::max(20, (int)(w.w / 4.f));
    auto pk = app_.peaks(t, bins);
    if (pk.empty()) { text(w.x + w.w / 2, w.y + w.h / 2, 10, WELL_DIM, "NO SAMPLE", 1, true); endClip(); return; }
    const float cy = w.y + w.h / 2, bw = w.w / bins;
    const std::vector<float> sl = ts.algo == 3 ? app_.session().sliceFractions(t) : std::vector<float>();   // KIT: where the sample is cut
    for (int i = 0; i < (int)pk.size(); i++) {
        const float pos = (i + 0.5f) / bins, v = std::min(1.f, pk[(size_t)i] * 1.1f);
        int si = -1; for (size_t j = 0; j + 1 < sl.size(); j++) if (pos >= sl[j] && pos < sl[j + 1]) si = (int)j;
        const uint32_t on = si >= 0 ? encoderTrace(si) : TRACE_GREEN;   // neighbouring slices alternate ink and dim
        rrect({w.x + i * bw, cy - std::max(1.f, v * (w.h / 2 - 1)), std::max(1.f, bw * 0.6f), std::max(2.f, v * (w.h - 2))}, 0, pos >= ts.start && pos <= ts.end ? on : GRID);
    }
    for (size_t j = 0; j + 1 < sl.size(); j++) {   // the cuts, numbered like the keys that play them (C = 1)
        const float x = w.x + 1 + clamp01(sl[j]) * (w.w - 2);
        rrect({x - 1, w.y, 2, w.h}, 0, TRACE_BLUE);
        text(std::min(x + 4, w.x + w.w - 14), w.y + 7, 9, TRACE_BLUE, std::to_string(j + 1), 0, true);
    }
    for (int j = 0; j < nv; j++) {   // a voice: a line of constant brightness that gets shorter as its envelope dies away
        const float vx = w.x + 1.2f + clamp01(st_.voicePos[t][j]) * (w.w - 2.4f), lv = clamp01(st_.voiceLvl[t][j]);
        const float h = std::max(4.f, lv * (w.h - 2));
        line(vx, cy - h / 2, vx, cy + h / 2, encoderTrace(j), 2.2f);
    }
    if (ts.algo == 4 && micS == 0) echoPosition(w, 2.f);
    endClip();
}

// Position is relative to START..END, whereas the waveform spans the
// entire source. Only the bottom handle captures touch, leaving the
// editor's start and end handles usable even when they coincide.
void SamplerUI::echoPosition(Rect w, float inset) {
    const int t = app_.track();
    const auto ts = app_.state(t);
    const float left = w.x + inset, span = w.w - 2 * inset;
    const float x = left + clamp01(ts.start + ts.echo.position * (ts.end - ts.start)) * span;
    const float bottom = w.y + w.h - 3;
    line(x, w.y + 3, x, bottom - 7, TRACE_AMBER, 1.8f);
    tri(std::max(w.x + 1, x - 5), bottom, std::min(w.x + w.w - 1, x + 5), bottom, x, bottom - 8, TRACE_AMBER);
    const float rangeStart=left+ts.start*span,rangeEnd=left+ts.end*span;
    const float available=std::max(1.f,rangeEnd-rangeStart);
    // Parameter-specific overlays are symbolic guides, not exact grain positions.
    const auto pg=app_.page();
    if(pg==SamplerApp::P_ECHO || pg==SamplerApp::P_GRAIN || pg==SamplerApp::P_MOTION) {
        const auto k=app_.knobs(pg);
        const float top=w.y+22,mid=w.y+w.h*.5f;
        const float radius=available*(.025f+.24f*(pg==SamplerApp::P_GRAIN?k[0].norm:ts.echo.scatter));
        const float lo=std::max(rangeStart,x-radius),hi=std::min(rangeEnd,x+radius);
        line(lo,top,hi,top,encoderTrace(pg==SamplerApp::P_GRAIN?0:1),2);
        for(float xx:{lo,hi})line(xx,top-4,xx,top+5,encoderTrace(pg==SamplerApp::P_GRAIN?0:1),1.6f);
        const int n=3+int(13*(pg==SamplerApp::P_GRAIN?k[1].norm:ts.echo.texture));
        for(int i=0;i<n;++i) {
            const float xx=lo+(hi-lo)*(i+.5f)/n;
            const float h=3+std::min(9.f,w.h*.12f)*(pg==SamplerApp::P_ECHO?ts.echo.evolve:.5f);
            line(xx,mid-h,xx,mid+h,encoderTrace(pg==SamplerApp::P_GRAIN?1:0),1.4f);
            circle(xx,mid-h,1.7f,encoderTrace(pg==SamplerApp::P_GRAIN?1:0));
        }
        const float row=bottom-13;
        for(int i=0;i<4;++i) {
            const float bx=w.x+4+i*(w.w-8)/4, bw=(w.w-8)/4-4;
            if(bw>0) {
                line(bx,row,bx+bw,row,GRID,2);
                circle(bx+bw*clamp01(k[i].norm),row,2.5f,encoderTrace(i));
            }
        }
        if(pg==SamplerApp::P_MOTION) {
            const float target=std::clamp(x+(k[0].norm-.5f)*available,rangeStart,rangeEnd);
            line(x,mid-12,target,mid-12,encoderTrace(0),2);
            const float dir=target<x?-1.f:1.f;
            tri(target,mid-12,target-dir*5,mid-15,target-dir*5,mid-9,encoderTrace(0));
        }
    }
    const std::string label = "POS " + num("%.1f%%", ts.echo.position * 100.f);
    const float labelW = float(label.size()) * T_CAPTION * 0.61f + 6;
    const float lx = std::clamp(x + 5, w.x + 2, std::max(w.x + 2, w.x + w.w - labelW - 2));
    rrect({lx, w.y + 4, labelW, 14}, 0, WELL);
    text(lx + 3, w.y + 11, T_CAPTION, TRACE_AMBER, label, 0, true);
    auto move = [this, t, left, span](float px, float) {
        if (app_.track() != t || app_.algo() != 4) return;
        const auto& state = app_.state(t);
        const float p = (px - left) / span;
        app_.knobs(SamplerApp::P_HOME)[3].set(clamp01((p - state.start) / std::max(0.001f, state.end - state.start)));
    };
    hit({std::max(w.x, x - 14), bottom - 14, std::min(w.x + w.w, x + 14) - std::max(w.x, x - 14), 17},
        [](float, float) {}, move);
}

// Four horizontal parameter controls; choice/action parameters still use the native setters.
void SamplerUI::knob(float cx, float cy, SamplerApp::Page page, int idx, bool) {
    const auto p=app_.knobs(page)[idx];
    if(!p.valid)return;
    const float half=std::max(12.f,(W_-24-440)/8-3);
    const uint32_t c=encoderTrace(idx);
    const Rect cell{cx-half,cy-20,2*half,48};
    bevel(cell,SURFACE);
    text(cx,cy-12,T_CAPTION,encoderColor(idx),fit(p.name,cell.w-6,T_CAPTION),1,true);
    text(cx,cy+1,12,TEXT,fit(p.value,cell.w-6,12),1,true);
    const Rect track{cell.x+5,cy+11,cell.w-10,11};
    rrect(track,0,WELL);stroke(track,0,GRID,1);
    if(p.norm>0.f)rrect({track.x+1,track.y+1,(track.w-2)*clamp01(p.norm),track.h-2},0,c);
    using P=SamplerApp;
    if(page==P::P_LEN&&idx==3)return;
    if(page==P::P_LEN&&(idx==1||idx==2)){tap(cell,[this,page,idx]{auto q=app_.knobs(page)[idx];if(q.step)q.step(1);});return;}
    if(page==P::P_SAVE&&idx==0){tap(cell,[this,page,idx]{auto q=app_.knobs(page)[idx];if(q.step)q.step(q.norm>.5f?-1:1);});return;}
    auto change=[this,page,idx,track](float x,float){auto q=app_.knobs(page)[idx];if(q.set)q.set(clamp01((x-track.x)/track.w));};
    if(page==P::P_LEN&&idx==0)hit(cell,change);
    else hit(cell,change,change);
}

// The tape loop: one closed ribbon over two wheels; it runs once round per loop. Without a loop it stands still.
void SamplerUI::cassette(Rect wd) {
    const int t = app_.track();
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const bool recNow = st_.freeRecording || st_.track_clip_rec[t];
    float lp = hasLoop ? loopPos() : 0.f;
    // One closed ribbon around two reels. Common tangents and circular
    // arcs define both the drawing and the touch projection.
    struct Post { float x, y, r; int s; };   // s = -1: the post is inside the ribbon (it wraps round it), +1: outside
    const float W0 = wd.w, H0 = wd.h;
    const Post posts[2] = {
        {wd.x + 0.25f * W0, wd.y + 0.42f * H0, 0.31f * H0, -1},   // big wheel
        {wd.x + 0.73f * W0, wd.y + 0.52f * H0, 0.19f * H0, -1},   // small wheel
    };
    struct Seg { bool arc; float x0, y0, x1, y1, cx, cy, r, a0, da, len; };
    std::vector<Seg> segs;
    {
        const int N = 2; float phi[N], px[N][2], qx[N][2];   // exit point of post i, entry point of post i
        for (int i = 0; i < N; i++) {
            const Post &a = posts[i], &b = posts[(i + 1) % N];
            const float dx = b.x - a.x, dy = b.y - a.y, D = std::sqrt(dx * dx + dy * dy), k = b.s * b.r - a.s * a.r;
            phi[i] = std::atan2(dy, dx) - std::asin(std::max(-1.f, std::min(1.f, k / D)));
            const float nx = -std::sin(phi[i]), ny = std::cos(phi[i]);
            px[i][0] = a.x - a.s * a.r * nx; px[i][1] = a.y - a.s * a.r * ny;
            qx[(i + 1) % N][0] = b.x - b.s * b.r * nx; qx[(i + 1) % N][1] = b.y - b.s * b.r * ny;
        }
        for (int i = 0; i < N; i++) {
            const int j = (i + 1) % N; const Post& b = posts[j];
            Seg l{false, px[i][0], px[i][1], qx[j][0], qx[j][1], 0, 0, 0, 0, 0, 0}; l.len = std::hypot(l.x1 - l.x0, l.y1 - l.y0); segs.push_back(l);
            const float a0 = std::atan2(qx[j][1] - b.y, qx[j][0] - b.x), a1 = std::atan2(px[j][1] - b.y, px[j][0] - b.x);
            float da = a1 - a0;
            if (b.s > 0) { while (da < 0) da += 2 * kPi; while (da > 2 * kPi) da -= 2 * kPi; } else { while (da > 0) da -= 2 * kPi; while (da < -2 * kPi) da += 2 * kPi; }
            Seg ar{true, 0, 0, 0, 0, b.x, b.y, b.r, a0, da, b.r * std::fabs(da)}; segs.push_back(ar);
        }
    }
    float per = 0; for (const Seg& g : segs) per += g.len;
    auto at = [&](float sp, float& x, float& y) {
        sp = std::fmod(sp + per, per);
        for (const Seg& g : segs) {
            if (sp > g.len) { sp -= g.len; continue; }
            if (!g.arc) { const float f = g.len > 0 ? sp / g.len : 0; x = g.x0 + (g.x1 - g.x0) * f; y = g.y0 + (g.y1 - g.y0) * f; }
            else { const float an = g.a0 + g.da * (g.len > 0 ? sp / g.len : 0); x = g.cx + g.r * std::cos(an); y = g.cy + g.r * std::sin(an); }
            return;
        }
        x = segs[0].x0; y = segs[0].y0;
    };
    const float turn = hasLoop ? lp : 0.f;   // without a loop the tape stands still
    const uint32_t tapeCol = recNow ? TRACE_RED : hasLoop ? TRACE_AMBER : WELL_DIM;
    const int t0 = std::max(1, (int)std::lround(per / (2 * kPi * posts[0].r))), t1 = std::max(1, (int)std::lround(per / (2 * kPi * posts[1].r)));
    reel(posts[0].x, posts[0].y, posts[0].r, -turn * 2 * kPi * t0);
    reel(posts[1].x, posts[1].y, posts[1].r, -turn * 2 * kPi * t1);
    // Draw both edges of the ribbon on the original centerline, so the
    // splice and the scrub projection still coincide with the visible tape.
    const uint32_t tapeBody = theme::light ? 0xff57453c : 0xff60413b;
    auto ribbon = [&](uint32_t color, float width) {
        for (const Seg& g : segs) {
            if (!g.arc) { line(g.x0, g.y0, g.x1, g.y1, color, width); continue; }
            const int n = std::max(2, (int)std::ceil(std::fabs(g.da) / 0.12f));
            for (int i = 0; i < n; i++) {
                const float u0 = g.a0 + g.da * i / n, u1 = g.a0 + g.da * (i + 1) / n;
                line(g.cx + g.r * std::cos(u0), g.cy + g.r * std::sin(u0),
                     g.cx + g.r * std::cos(u1), g.cy + g.r * std::sin(u1), color, width);
            }
        }
    };
    ribbon(tapeBody, 5.0f);
    ribbon(tapeCol, 2.0f);
    float x, y; at(turn * per, x, y);
    circle(x, y, 8, WELL); circle(x, y, 5.5f, recNow ? TRACE_RED : TRACE_GREEN);
    if (!hasLoop || recNow) return;
    // Project touches onto the same directed path used for the splice marker.
    // The nearest point on each arc or tangent gives an arc-length coordinate.
    auto project = [segs, per](float px, float py, float& fraction) {
        float best = 1e30f, offset = 0;
        for (const Seg& g : segs) {
            float u = 0;
            if (g.arc) {
                const float a = std::atan2(py - g.cy, px - g.cx);
                const float sign = g.da < 0 ? -1.f : 1.f;
                float sweep = std::fmod(sign * (a - g.a0), 2 * kPi);
                if (sweep < 0) sweep += 2 * kPi;
                if (sweep <= std::fabs(g.da)) u = sweep / std::fabs(g.da);
                else {
                    const float a1 = g.a0 + g.da;
                    const float d0 = std::hypot(px - g.cx - g.r * std::cos(g.a0), py - g.cy - g.r * std::sin(g.a0));
                    const float d1 = std::hypot(px - g.cx - g.r * std::cos(a1), py - g.cy - g.r * std::sin(a1));
                    u = d0 <= d1 ? 0.f : 1.f;
                }
            } else {
                const float dx = g.x1 - g.x0, dy = g.y1 - g.y0;
                u = clamp01(((px - g.x0) * dx + (py - g.y0) * dy) / (g.len * g.len));
            }
            const float a = g.a0 + u * g.da;
            const float x = g.arc ? g.cx + g.r * std::cos(a) : g.x0 + u * (g.x1 - g.x0);
            const float y = g.arc ? g.cy + g.r * std::sin(a) : g.y0 + u * (g.y1 - g.y0);
            const float distance = std::hypot(px - x, py - y);
            if (distance < best) {
                best = distance;
                fraction = (offset + u * g.len) / per;
            }
            offset += g.len;
        }
        return best;
    };
    // Only presses near the ribbon grab the tape. Leaving its wider tracking
    // band pauses the gesture; returning reanchors it without a seek.
    constexpr float touchWidth = 18.f, trackingWidth = 24.f;
    hit(wd, [this, project, touchWidth](float px, float py) {
        tapeGrabbed_ = project(px, py, tapeLast_) <= touchWidth;
        tapeTracking_ = tapeGrabbed_;
        if (tapeGrabbed_) app_.session().scrubBegin(0.f, Session::ScrubStyle::Reel);
    }, [this, project, trackingWidth](float px, float py) {
        if (!tapeGrabbed_) return;
        float fraction = 0;
        if (project(px, py, fraction) > trackingWidth) {
            tapeTracking_ = false;
            return;
        }
        if (tapeTracking_) app_.session().scrubBy(std::remainder(fraction - tapeLast_, 1.f));
        tapeLast_ = fraction;
        tapeTracking_ = true;
    }, [this] {
        if (tapeGrabbed_) app_.session().scrubEnd();
        tapeGrabbed_ = tapeTracking_ = false;
    });
}

float SamplerUI::loopPos() const {
    const int t = app_.track();
    if (!(app_.loopExists() && st_.loopSamples > 0)) return 0.f;
    return st_.track_has_clip[t] ? st_.clipPos[t] : (float)(st_.pos / st_.loopSamples);
}

// Timeline of the selected track: a ruler with bars and seconds, the loop's waveform, the playhead. Drag anywhere on it to scrub.
void SamplerUI::timeline(Rect r) {
    scope(r, false);
    const int t = app_.track();
    const float tx = r.x + 3, tw = r.w - 6, rh = 18, wy = r.y + rh + 4, wh = r.h - rh - 8;
    auto phosphor=[&](float x,float bw,float cy,float v,uint32_t c){
        if(v<.005f)return;
        const float h=std::max(.5f,v*(wh/2-3));
        for(int j=0;j<4;++j){const float q=1.f-j*.23f;
            rrect({x,cy-h*q,std::max(.5f,bw+.10f),2*h*q},0,withAlpha(c,58+j*58));}
    };
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const float sr = (float)app_.session().sampleRate(), loopSec = hasLoop ? (float)(st_.loopSamples / sr) : 0.f;
    if (!hasLoop) {
        if (!st_.freeRecording) { text(tx + tw / 2, r.y + r.h / 2, 12, WELL_DIM, st_.freeArmed ? "WAITING FOR THE FIRST SOUND" : "NO LOOP: PRESS REC AND PLAY", 1, true); return; }
        // the first take: its waveform is drawn as it is recorded, squeezed into the width (so the whole take is always in view), seconds on the ruler
        static float fp[18500]; const int n = app_.session().freePeaks(fp, 18500);
        const float secs = std::max(1.f, st_.freeSeconds), pps = tw / secs;
        static const float steps[] = {0.5f, 1, 2, 5, 10, 15, 30, 60, 120};
        float step = 120; for (float sp : steps) if (sp * pps >= 44) { step = sp; break; }
        for (float sec = 0; sec <= secs + 0.001f; sec += step) {
            const float x = std::min(tx + tw - 1, tx + sec * pps);
            line(x, r.y + 4, x, r.y + 12, WELL_DIM, 1);
            char lb[16]; if (secs >= 60) snprintf(lb, sizeof lb, "%d:%02d", (int)sec / 60, (int)sec % 60); else snprintf(lb, sizeof lb, step < 1 ? "%.1fs" : "%.0fs", sec);
            if (x + 26 < tx + tw - 70) text(x + 4, r.y + 8, 9, WELL_DIM, lb, 0, true);
        }
        float mx = 0.08f; for (int i = 0; i < n; i++) mx = std::max(mx, fp[i]);
        const int cols = std::max(1, (int)(tw / 2)), per = std::max(1, (n + cols - 1) / cols), cnt = (n + per - 1) / per;
        const float cy = wy + wh / 2, bw = tw * ((float)n * 0.01f / secs) / std::max(1, cnt);
        for (int a = 0; a < cnt; a++) {
            float v = 0; for (int j = a * per; j < std::min(n, (a + 1) * per); j++) v = std::max(v, fp[j]);
            v = std::min(1.f, v / mx);
            phosphor(tx+a*bw,bw,cy,v,TRACE_RED);
        }
        const float hx = tx + clamp01(n * 0.01f / secs) * tw;
        rrect({hx - 1, r.y + 2, 2.2f, r.h - 4}, 0, TRACE_RED);
        text(tx + tw - 4, r.y + 8, 9, TRACE_RED, "REC " + num("%.1f", st_.freeSeconds) + " S", 2, true);
        return;
    }
    const float barSec = loopSec / std::max(1, st_.loopBars), pps = tw / loopSec;
    const float barPx = barSec * pps; const int every = barPx >= 26 ? 1 : barPx >= 13 ? 2 : barPx >= 7 ? 4 : 8;
    for (int b = 0; b <= st_.loopBars; b++) {   // bars: the number in the dim tone, a thin line through the waveform
        const float x = tx + b * barPx;
        if (b < st_.loopBars && b % every == 0) text(x + 4, r.y + 8, 9, WELL_DIM, std::to_string(b + 1), 0, true);
        if (b % every == 0 && b < st_.loopBars) rrect({x, wy, 1.5f, wh}, 0, GRID);
    }
    float pk[Session::kLoopBins]; const int n = app_.session().loopPeaks(t, pk, Session::kLoopBins);
    const bool rec = st_.track_clip_rec[t];
    if (n > 0 && st_.track_has_clip[t]) {
        float mx = 0.08f; for (int i = 0; i < n; i++) mx = std::max(mx, pk[i]);
        const float cy = wy + wh / 2, bw = tw / n;
        for (int i = 0; i < n; i++) phosphor(tx+i*bw,bw,cy,std::min(1.f,pk[i]/mx),rec?TRACE_RED:trackTrace(t));
    } else text(tx + tw / 2, wy + wh / 2, 10, WELL_DIM, "TRACK " + std::to_string(t + 1) + " HAS NO LOOP", 1, true);
    const float hx = tx + clamp01(loopPos()) * tw;
    rrect({hx - 1, r.y + 2, 2.2f, r.h - 4}, 0, TRACE_AMBER);
    hit({tx, r.y, tw, r.h}, [this, tx, tw](float x, float) { app_.scrub(0, clamp01((x - tx) / tw)); }, [this, tx, tw](float x, float) { app_.scrub(1, clamp01((x - tx) / tw)); }, [this] { app_.scrub(2, 0); });
}

void SamplerUI::key(Rect r, const std::string& label, int icon, bool on, std::function<void()> f, bool) {
    const uint32_t ink = on ? contrastInk(ACCENT) : TEXT;
    bevel(r, on ? ACCENT : SURFACE, on);
    const float cx = r.x + r.w / 2, cy = r.y + r.h / 2 - 5;
    if (icon == 1) tri(cx - 5, cy - 7, cx - 5, cy + 7, cx + 7, cy, ink);
    else if (icon == 2) { rrect({cx - 6, cy - 7, 4.5f, 14}, 0, ink); rrect({cx + 2, cy - 7, 4.5f, 14}, 0, ink); }
    else if (icon == 3) circle(cx, cy, 6.5f, kOrange);
    else if (icon == 4) rrect({cx - 6, cy - 6, 12, 12}, 1, ink);
    else if (icon == 5) { line(cx - 6, cy - 6, cx + 6, cy + 6, ink, 2.2f); line(cx - 6, cy + 6, cx + 6, cy - 6, ink, 2.2f); }
    text(cx, icon ? r.y + r.h - 9 : r.y + r.h / 2, 9, ink, label, 1, true);
    tap(r, std::move(f));
}

// Icons of the deck: solid shapes on one 20 px box, drawn from the centre.
void SamplerUI::icon(int kind, float cx, float cy, uint32_t c) {
    switch (kind) {
    case 0: tri(cx - 8, cy - 10.7f, cx - 8, cy + 10.7f, cx + 10.7f, cy, c); break;                      // play
    case 1: rrect({cx - 9.3f, cy - 9.3f, 18.6f, 18.6f}, 0, c); break;                                    // stop
    case 2: circle(cx, cy, 10, c); break;                                                                 // record
    case 3: line(cx - 9.3f, cy - 9.3f, cx + 9.3f, cy + 9.3f, c, 3.4f); line(cx + 9.3f, cy - 9.3f, cx - 9.3f, cy + 9.3f, c, 3.4f); break;   // clear
    case 4: { // granular controls: a cloud of windowed grains
        for(int i=0;i<5;++i){
            const float x=cx+(i-2)*4.3f,h=4+6*std::sin((i+1)*kPi/6);
            line(x,cy-h,x,cy+h,c,2.5f);
        }
        circle(cx-9,cy-9,1.4f,c);circle(cx+9,cy+9,1.4f,c);
        break; }
    case 5: {   // microphone
        rrect({cx - 5.3f, cy - 10, 10.6f, 14.7f}, 5.3f, c);
        for (int i = 0; i < 8; i++) { const float a0 = kPi * i / 8, a1 = kPi * (i + 1) / 8; line(cx + 9.3f * std::cos(a0), cy + 0.7f + 9.3f * std::sin(a0), cx + 9.3f * std::cos(a1), cy + 0.7f + 9.3f * std::sin(a1), c, 2.7f); }
        rrect({cx - 1.3f, cy + 10, 2.7f, 3}, 0, c);
        break; }
    case 6: rrect({cx - 10, cy - 8.7f, 7.3f, 5}, 0, c); rrect({cx - 10, cy - 4.7f, 20, 14}, 0, c); break;   // folder: a file from the disk
    case 7: rrect({cx - 10, cy - 9.3f, 20, 3.4f}, 0, c); rrect({cx - 10, cy - 1.7f, 20, 3.4f}, 0, c); rrect({cx - 10, cy + 6, 13.3f, 3.4f}, 0, c); break;   // project
    }
}

void SamplerUI::deck(Rect r) {
    rrect({0, 0, W_, H_}, 0, DECK_BG);
    const int t = app_.track();
    const bool recOn = st_.freeRecording || st_.track_clip_rec[t], armed = st_.freeArmed, hasLoop = app_.loopExists() && st_.loopSamples > 0;
    // One grid: margin M, three zones (tape and transport on the left, tools in the middle, sample and knobs on the right, timeline below).
    // Every icon sits on a 64 px pitch; the bottom icon of the tools column stands on the transport row.
    const float M = 24, zr = W_ - M, x0 = 440, w0 = zr - x0, mx = 351;
    if (!theme::light) {
        bevel({M - 10, 6, 304, 282}, SURFACE);
        bevel({mx - 31, 6, 62, 272}, SURFACE);
        bevel({x0 - 8, 6, w0 + 16, 282}, SURFACE);
    }
    // header: the track number (tap = next track), the sample name (tap = library), under it the position in the loop
    const uint32_t tc = trackColor(t);
    bevel({M - 5, 12, 29, 43}, tc, true);
    text(M + 9, 34, T_TITLE, contrastInk(tc), std::to_string(t + 1), 1, true);
    tap({M - 6, 8, 34, 52}, [this, t] { app_.selectTrack((t + 1) % SamplerApp::kTracks); });
    std::string title = app_.sampleLabel(t); if (title.empty()) title = "Empty";
    const int mic = app_.micState();
    if (mic == 1) title = "Listening"; else if (mic == 2) title = "Recording"; else if (mic == 3) title = "Saving";
    const Rect nameField{M + 28, 12, 260, 43};
    bevel(nameField, RAISED, true);
    text(nameField.x + 8, 34, T_TITLE, TEXT, fit(title, nameField.w - 30, T_TITLE), 0, true);
    const float ax = nameField.x + nameField.w - 13;
    tri(ax - 4, 32, ax + 4, 32, ax, 37, tc);
    tap({M + 24, 8, mx - M - 40, 52}, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    {
        float lsec = 0, psec = 0;
        if (hasLoop) { lsec = (float)(st_.loopSamples / app_.session().sampleRate()); psec = loopPos() * lsec; } else if (st_.freeRecording) psec = lsec = st_.freeSeconds;
        char cnt[48]; snprintf(cnt, sizeof cnt, "%d:%02d / %d:%02d", (int)psec / 60, (int)psec % 60, (int)lsec / 60, (int)lsec % 60);
        const std::string ts = app_.toast();
        bevel({M - 4, 57, 292, 20}, WELL, true);
        const std::string info = hasLoop ? std::to_string(st_.loopBars) + "B  " + num("%.0f", st_.bpm) + " BPM  " + cnt : st_.freeRecording ? std::string(cnt) : "NO LOOP";
        text(M + 2, 67, T_BODY, !ts.empty() ? TRACE_GREEN : TRACE_AMBER, fit(!ts.empty() ? ts : info, 280, T_BODY), 0, true);
    }
    scope({M - 4, 86, 292, 150});
    cassette({M - 4, 86, 292, 150});
    // transport: play, stop, record / overdub, clear the loop of this track; the active one is orange and underlined
    const float ty = 260;
    for (int i = 0; i < 4; i++) {
        const float cx = M + 20 + i * 64;
        const bool on = i == 0 ? st_.playing : i == 2 ? recOn : false;
        const uint32_t col = i == 0 ? GREEN : i >= 2 ? RED : ACCENT;
        bevel({cx - 26, ty - 22, 52, 44}, on ? col : SURFACE, on);
        icon(i, cx, ty - 4, on ? contrastInk(col) : col);
        static const char* names[] = {"PLAY", "STOP", "REC", "CLEAR"};
        text(cx, ty + 15, T_CAPTION, on ? contrastInk(col) : col, names[i], 1, true);
        if (i == 2 && armed && (now_ / 400) % 2 == 0) stroke({cx - 26, ty - 22, 52, 44}, 0, RED, 2);
        tap({cx - 32, ty - 28, 64, 58}, [this, i] { if (i == 0) app_.transportSet(true); else if (i == 1) app_.transportSet(false); else if (i == 2) app_.loopRecord(); else app_.loopClearTrack(); });
    }
    // tools in the middle: granular controls, microphone, file, project
    const uint32_t micCol = mic == 1 || mic == 2 ? RED : GREEN;
    for (int i = 0; i < 4; ++i) bevel({mx - 23, 34.f + i * 64, 46, 44}, SURFACE);
    const bool granular=app_.algo()==4;
    icon(4,mx,56,granular?VIOLET:DIM);
    tap({mx-32,28,64,56},[this]{
        if(app_.algo()!=4){app_.say("SELECT GRANULAR FIRST");return;}
        app_.gotoPage(app_.page()==SamplerApp::P_ECHO?SamplerApp::P_GRAIN:
            app_.page()==SamplerApp::P_GRAIN?SamplerApp::P_MOTION:SamplerApp::P_ECHO);
    });
    icon(5, mx, 120, micCol); tap({mx - 32, 92, 64, 56}, [this] { app_.micSample(); });
    icon(6, mx, 184, ACCENT); tap({mx - 32, 156, 64, 56}, [this] { app_.requestFile(); });
    icon(7, mx, 248, projectOpen_ ? AMBER : ACCENT); tap({mx - 32, 220, 64, 56}, [this] { projectOpen_ = true; });
    // the sample of this track, its page and the four knobs of the page
    sampleView({x0, 14, w0, 178});
    deckMenus({x0, 193, w0, 42});
    for (int i = 0; i < 4; i++) knob(x0 + (i + 0.5f) * w0 / 4, 258, app_.page(), i);
    timeline({M, 298, W_ - 2 * M, H_ - 298 - 8});
}

void SamplerUI::deckMenus(Rect r) {
    const int selected = activeMenu(app_.page());
    const auto& group = kMenus[selected];
    const int children = childCount(app_, group);
    const float py = r.y + 30, right = r.x + r.w;
    const float tabW = std::min(24.f, (r.w - 88.f) / kMenuCount);
    const float start = right - kMenuCount * tabW;
    strip({r.x, py-12, start-r.x-4, 24}, pageColor(app_.page()), SamplerApp::pageName(app_.page()), T_BODY);
    for (int i=0;i<kMenuCount;++i) {
        Rect nr{start+i*tabW, py-12, tabW, 24};
        const bool on = i==selected;
        const uint32_t col = pageColor(kMenus[i].pages[0]);
        bevel({nr.x+1, py-10, tabW-2, 20}, on?col:SURFACE, on);
        text(nr.x+tabW/2, py, 9, on?contrastInk(col):col, kMenus[i].deckLabel, 1, true);
        tap(nr,[this,i]{app_.gotoPage(nextMenuPage(app_,kMenus[i]));});
    }
    if(children<=1) return;
    // The first child shares half the parent's horizontal extent,
    // but remains entirely above it. Subsequent tabs follow the rail
    // rightward, or leftward for LENGTH/TEMPO.
    const bool leftward = group.pages[0] == Page::P_LEN;
    const float parentLeft = start + selected * tabW + 1;
    const float parentRight = parentLeft + tabW - 2;
    const float edge = (parentLeft + parentRight) * .5f;
    const float room = leftward ? edge - r.x : right - edge;
    const float gap = std::min(5.f, room / (children * 8.f));
    const float subW = std::min(49.f, (room - (children-1)*gap)/children);
    const float sy = r.y+1, sh=17, railY=sy+sh/2;
    const uint32_t accent = pageColor(group.pages[0]);
    const float last = edge + (leftward ? -1 : 1) * (children*subW+(children-1)*gap);
    // The rail endpoints lie inside the first and last tabs, so the
    // rounded line caps cannot extend beyond the tab edges.
    line(std::min(edge,last)+subW*.5f,railY,std::max(edge,last)-subW*.5f,railY,accent,1.4f);
    const float stemX = edge + (leftward ? -1.f : 1.f) * std::min((parentRight-parentLeft)*.25f,subW*.5f);
    line(stemX,sy+sh,stemX,py-10,accent,1.4f);
    for (int i=0,j=0;i<group.count;++i) {
        const Page p=group.pages[i];
        if (!app_.pageAvailable(p)) continue;
        const float offset = j*(subW+gap);
        const float x = leftward ? edge-offset-subW : edge+offset;
        const Rect c{x,sy,subW,sh};
        const bool on = p==app_.page();
        const uint32_t col = pageColor(p);
        // Identical heights keep the child and parent hit regions disjoint.
        bevel(c,on?col:SURFACE,on);
        const float font = subW < 35 ? 8.f : 8.5f;
        const char* label=group.labels[i];
        const char* compact=label;
        if (subW < 37) {
            switch(p) {
            case Page::P_HOME: compact="SMP"; break;
            case Page::P_EFILTER: compact="FILT"; break;
            case Page::P_EDELAY: compact="DLY"; break;
            case Page::P_EREVERB: compact="REV"; break;
            case Page::P_ETAPE: compact="E.TP"; break;
            case Page::P_EMIX: compact="E.MX"; break;
            case Page::P_MOTION: compact="MOT"; break;
            case Page::P_GRAIN: compact="GRN"; break;
            case Page::P_LEN: compact="LEN"; break;
            case Page::P_TEMPO: compact="BPM"; break;
            default: break;
            }
        }
        text(c.x+c.w/2,railY,font,on?contrastInk(col):col,fit(compact,c.w-4,font),1,true);
        tap(c,[this,p]{app_.gotoPage(p);});
        ++j;
    }
}

void SamplerUI::trackList(Rect r) {
    const float rh = (r.h - 7 * 4) / 8;
    for (int i = 0; i < SamplerApp::kTracks; i++) {
        Rect row{r.x, r.y + i * (rh + 4), r.w, rh};
        const bool sel = i == app_.track();
        const uint32_t col = trackColor(i);
        bevel(row, sel ? RAISED : SURFACE, sel);
        rrect({row.x + 3, row.y + 3, 22, rh - 6}, 0, sel ? col : TRACK);
        text(row.x + 14, row.y + rh / 2, T_TITLE, sel ? contrastInk(col) : TEXT, std::to_string(i + 1), 1, true);
        const std::string lab = app_.generativeDeck()?app_.genTrackSource(i):app_.sampleLabel(i);
        text(row.x + 30, row.y + rh / 2 - 6, 11, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY" : lab, r.w - 40, 11), 0, true);
        // level and loop mark
        const float lv = std::min(1.f, st_.track_peak[i] * 1.5f);
        rrect({row.x + 30, row.y + rh - 12, r.w - 44, 3}, 0, OUTLINE);
        rrect({row.x + 30, row.y + rh - 12, (r.w - 44) * lv, 3}, 0, col);
        if(app_.generativeDeck()){if(app_.genTrackRunning(i))circle(row.x+r.w-10,row.y+10,3.5f,GREEN);}
        else if (st_.track_clip_rec[i]) circle(row.x + r.w - 10, row.y + 10, 4, RED);
        else if (st_.track_has_clip[i]) circle(row.x + r.w - 10, row.y + 10, 3.5f, GREEN);
        tap(row, [this, i] { app_.selectTrack(i); });
    }
}

void SamplerUI::sampleArea(Rect r) {
    if(app_.generativeDeck()){generativeHub(r);return;}
    const int t = app_.track();
    const auto ts = app_.state(t);
    const std::string lab = app_.sampleLabel(t);
    strip({r.x, r.y, r.w, 24}, trackColor(t), fit(lab.empty() ? "EMPTY TRACK: PICK A SAMPLE" : lab, r.w - 100, T_TITLE), 16);
    text(r.x + r.w - 5, r.y + 12, T_CAPTION, contrastInk(trackColor(t)), "TRACK " + std::to_string(t + 1), 2, true);
    // algorithm
    const float cw = (r.w - 5 * 4) / 6;
    for (int a = 0; a < 6; a++) {
        Rect c{r.x + a * (cw + 4), r.y + 30, cw, 28};
        button(c, a == 5 ? "PAUL" : SamplerApp::algoName(a), ts.algo == a, [this, a] { app_.setAlgo(a); }, 9.5f);
    }
    // waveform with the playback window
    Rect w{r.x, r.y + 66, r.w, 118};
    scope(w);
    if (app_.micState() == 1 || app_.micState() == 2) {   // microphone: input level + trigger threshold while waiting, the growing take while recording
        const float cy0 = w.y + w.h / 2, half = w.h / 2 - 8;
        if (app_.micState() == 1) {
            const float thr = std::min(1.f, app_.micThreshold() * 2.f), lv = std::min(1.f, st_.inputPeak * 2.f);
            rrect({w.x + 8, cy0 - lv * half, w.w - 16, std::max(2.f, lv * half * 2)}, 0, lv > thr ? TRACE_RED : TRACE_GREEN);
            line(w.x + 8, cy0 - thr * half, w.x + w.w - 8, cy0 - thr * half, TRACE_RED, 1);
            line(w.x + 8, cy0 + thr * half, w.x + w.w - 8, cy0 + thr * half, TRACE_RED, 1);
            text(w.x + w.w / 2, w.y + w.h - 12, T_CAPTION, WELL_DIM, fit("DRAG TO SET TRIGGER  " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), w.w - 16, T_CAPTION), 1, false);
            auto setThr = [this, half, cy0](float, float y) { app_.setMicThreshold01(std::log10(std::max(0.0025f, std::fabs(y - cy0) / half / 2.f) / 0.005f) / 2.f); };
            hit(w, setThr, setThr);
        } else {
            float live[2048]; const int n = app_.session().livePeaks(live, 2048);
            const int cols = std::max(20, (int)(w.w / 3)), per = std::max(1, (n + cols - 1) / cols);   // longer takes are squeezed to fit
            const float bw0 = (w.w - 16) / std::max(cols, (n + per - 1) / per);
            for (int i = 0; i * per < n; i++) {
                float v = 0; for (int j = i * per; j < std::min(n, (i + 1) * per); j++) v = std::max(v, live[j]);
                v = std::min(1.f, v * 1.1f);
                rrect({w.x + 8 + i * bw0, cy0 - std::max(1.f, v * half), std::max(1.f, bw0 - 1), std::max(2.f, v * half * 2)}, 0, TRACE_RED);
            }
            text(w.x + w.w - 24, w.y + 16, T_CAPTION, TRACE_RED, "REC " + std::to_string(st_.sampleSeconds).substr(0, 4) + "S", 2, true);
        }
        goto after_wave;
    }
    waveform(w,true);
    after_wave:
    // loop strip under the waveform
    Rect ls{r.x, w.y + w.h + 6, r.w, 34};
    scope(ls, false);
    {   // the whole loop: every track mixed (dim) and the selected track's own loop on top
        float mixp[400], trp[400]; const int ln = app_.session().loopPeaks(-1, mixp, 400); app_.session().loopPeaks(t, trp, 400);
        if (ln > 0) {
            float mx = 0.08f; for (int i = 0; i < ln; i++) mx = std::max(mx, mixp[i]);
            const float lcy = ls.y + ls.h / 2, lh = ls.h / 2 - 3, lbw = (ls.w - 4) / ln;
            for (int i = 0; i < ln; i++) {
                const float v = std::min(1.f, mixp[i] / mx), u = std::min(1.f, trp[i] / mx);
                if (v > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - v * lh, std::max(1.f, lbw), std::max(1.f, 2 * v * lh)}, 0, GRID);
                if (u > 0.01f) rrect({ls.x + 2 + i * lbw, lcy - u * lh, std::max(1.f, lbw), std::max(1.f, 2 * u * lh)}, 0, trackTrace(t));
            }
        } else text(ls.x + ls.w / 2, ls.y + ls.h / 2, T_CAPTION, WELL_DIM, "NO LOOP: PRESS REC", 1, false);
    }
    const bool hasLoop = app_.loopExists() && st_.loopSamples > 0;
    const float lp = loopPos(), x0 = ls.x + 3.5f, span = ls.w - 7.f;
    if (hasLoop || st_.freeRecording) {
        const float hx = x0 + clamp01(lp) * span;
        const uint32_t col = st_.freeRecording || st_.track_clip_rec[t] ? TRACE_RED : TRACE_AMBER;
        rrect({hx - 1.5f, ls.y + 2, 3, ls.h - 4}, 0, col);
        rrect({hx - 2.5f, ls.y + 2, 5, 4}, 0, col);
        rrect({hx - 2.5f, ls.y + ls.h - 6, 5, 4}, 0, col);
    }
    // The editor uses an absolute shuttle with a lighter response than
    // the tape deck. The sample-window handles above remain independent.
    if (hasLoop && !app_.recordingNow()) {
        hit(ls, [this, x0, span](float x, float) {
            app_.scrub(0, clamp01((x - x0) / span), Session::ScrubStyle::Digital);
        }, [this, x0, span](float x, float) {
            app_.scrub(1, clamp01((x - x0) / span));
        }, [this] { app_.scrub(2, 0); });
    }
    // Tools occupy the freed space, source and record/load align with the bottom of track 8.
    Rect lr{r.x, r.y+r.h-80, r.w, 34};
    if(app_.micState()==0)waveTools({r.x,ls.y+ls.h+5,r.w,std::max(0.f,lr.y-ls.y-ls.h-10)},true);
    const auto& lib = app_.library();
    const int cur = app_.libraryCursor();
    button({lr.x, lr.y, 34, lr.h}, "<", false, [this] { app_.browse(-1); }, 15);
    button({lr.x + lr.w - 34 - 0, lr.y, 34, lr.h}, ">", false, [this] { app_.browse(1); }, 15);
    Rect nm{lr.x + 40, lr.y, lr.w - 80, lr.h};
    bevel(nm, RAISED, true);
    if (!lib.empty()) {
        text(nm.x + 10, nm.y + nm.h / 2 - 6, 12, TEXT, fit(lib[(size_t)cur].label, nm.w - 80, 12), 0, true);
        text(nm.x + 10, nm.y + nm.h / 2 + 9, 9, DIM, fit(lib[(size_t)cur].group + "  " + std::to_string(cur + 1) + "/" + std::to_string(lib.size()), nm.w - 70, 9), 0, false);
    }
    text(nm.x + nm.w - 10, nm.y + nm.h / 2, 9.5f, ACCENT, "LIBRARY", 2, true);
    tap(nm, [this] { sheetOpen_ = true; sheetScroll_ = std::max(0.f, (float)app_.libraryCursor() * 34.f - 100.f); app_.refreshLibrary(); });
    // microphone and disk
    Rect br{r.x, lr.y + lr.h + 8, r.w, 38};
    const int ms = app_.micState();
    static const char* micLab[] = {"RECORD FROM MIC", "WAITING FOR SOUND...", "STOP AND KEEP", "SAVING..."};
    button({br.x, br.y, br.w * 0.6f - 3, br.h}, micLab[ms], ms == 2, [this] { app_.micSample(); }, 12, ms != 0, RED);
    button({br.x + br.w * 0.6f + 3, br.y, br.w * 0.4f - 3, br.h}, "LOAD FILE FROM DISK", false, [this] { app_.requestFile(); }, 11);
    const std::string ts_ = app_.toast();
    if (!ts_.empty()&&app_.micState()!=0) text(r.x, ls.y+ls.h+14, 10, ACCENT, fit(ts_, r.w, 10), 0, true);
}

void SamplerUI::propertyTab(Rect r, const std::string& label, bool on,
                            uint32_t col, float font) {
    // Property-sheet tabs: stepped corners, raised top and left edges,
    // dark right edge. The active tab has no bottom seam.
    const uint32_t fill=on?RAISED:SURFACE;
    rrect({r.x,r.y+3,r.w,r.h-3},0,fill);
    rrect({r.x+3,r.y,r.w-6,3},0,fill);
    line(r.x+3,r.y+.5f,r.x+r.w-3,r.y+.5f,HI);
    line(r.x+.5f,r.y+3,r.x+3,r.y+.5f,HI);
    line(r.x+.5f,r.y+3,r.x+.5f,r.y+r.h-1,HI);
    line(r.x+r.w-3,r.y+.5f,r.x+r.w-.5f,r.y+3,SHADOW);
    line(r.x+r.w-.5f,r.y+3,r.x+r.w-.5f,r.y+r.h-1,SHADOW);
    if(!on) line(r.x+1,r.y+r.h-1,r.x+r.w-1,r.y+r.h-1,SHADOW);
    else {
        rrect({r.x+3,r.y+1,r.w-6,3},0,col);
        tri(r.x+3,r.y+1,r.x+3,r.y+4,r.x+1,r.y+4,col);
        tri(r.x+r.w-3,r.y+1,r.x+r.w-1,r.y+4,r.x+r.w-3,r.y+4,col);
    }
    text(r.x+r.w/2,r.y+r.h/2,font,on?col:TEXT,fit(label,r.w-10,font),1,on);
}

void SamplerUI::pagePanel(Rect r) {
    constexpr int cols=5;
    const int selected=activeMenu(app_.page());
    const auto& group=kMenus[selected];
    const int children=childCount(app_,group);
    // The primary tabs form two slightly offset, overlapping tiers.
    // Paint the selected property sheet last so its edges sit in front.
    const float pitch=(r.w-9)/cols;
    auto primaryRect=[&](int i) {
        const int tier=i/cols;
        const bool on=i==selected;
        return Rect{r.x+2+(i%cols)*pitch+tier*5,
            r.y+tier*27+(on?0.f:3.f),pitch+2,on?31.f:28.f};
    };
    for(int pass=0;pass<2;++pass) for(int i=0;i<kMenuCount;++i) {
        if((i==selected)!=(pass==1)) continue;
        const Rect c=primaryRect(i);
        if(app_.generativeDeck()&&i==9){propertyTab(c,"--",false,DIM,9.5f);continue;}
        propertyTab(c,app_.generativeDeck()&&i==4?"GLOBAL":kMenus[i].editorLabel,i==selected,pageColor(kMenus[i].pages[0]),9.5f);
        tap(c,[this,i]{app_.gotoPage(nextMenuPage(app_,kMenus[i]));});
    }
    // The contextual tabs sit on the raised property-sheet body.
    // Neighboring tabs overlap by four pixels; the selected one is
    // taller and erases the body's top border where it joins the sheet.
    const float sheetY=r.y+91;
    bevel({r.x,sheetY,r.w,r.y+r.h-sheetY},RAISED);
    if(children>1) {
        const float pitch=(r.w-8)/children;
        for(int pass=0;pass<2;++pass) for(int i=0,j=0;i<group.count;++i) {
            const Page p=group.pages[i];
            if(!app_.pageAvailable(p)) continue;
            const bool on=app_.page()==p;
            const float x=r.x+3+j*pitch; ++j;
            if(on!=(pass==1)) continue;
            const Rect c{x,sheetY-(on?28.f:24.f),pitch+4,on?30.f:24.f};
            propertyTab(c,app_.generativeDeck()&&selected==0?app_.contextualPageName(p):group.labels[i],on,pageColor(p),9.f);
            tap(c,[this,p]{app_.gotoPage(p);});
        }
    } else {
        const Rect c{r.x+3,sheetY-28,std::min(r.w-6,112.f),30};
        propertyTab(c,app_.contextualPageName(app_.page()),true,pageColor(app_.page()),9.f);
    }
    auto k=app_.knobs();
    const float tabsH=104, top=r.y+tabsH, rh=(r.h-tabsH)/4;
    for (int i = 0; i < 4; i++) {
        const auto& p = k[(size_t)i];
        const float y = top + i * rh;
        if (!p.valid) continue;
        const uint32_t col = encoderColor(i);
        bevel({r.x, y, r.w, rh - 5}, SURFACE);
        const float valueW = std::min(r.w * 0.52f, std::max(60.f, (float)p.value.size() * T_VALUE * 0.61f + 8));
        text(r.x + 4, y + 10, T_CAPTION, col, fit("K" + std::to_string(i + 1) + "  " + p.name, r.w - valueW - 12, T_CAPTION), 0, true);
        text(r.x + r.w - 4, y + 10, T_VALUE, col, fit(p.value, valueW, T_VALUE), 2, true);
        Rect s{r.x + 4, y + 26, r.w - 8, 14};
        bevel(s, WELL, true);
        const Rect inner{s.x + 1, s.y + 1, s.w - 2, s.h - 2};
        const float norm = clamp01(p.norm);
        if (p.isChoice) {
            rrect({inner.x + norm * (inner.w - 8), inner.y, 8, inner.h}, 0, encoderTrace(i));
        } else if (norm > 0.f) {
            rrect({inner.x, inner.y, inner.w * norm, inner.h}, 0, encoderTrace(i));
        }
        Rect hr{inner.x, y + 14, inner.w, 40};
        hit(hr, [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); },
            [this, i, hr](float x, float) { app_.knobs()[(size_t)i].set(clamp01((x - hr.x) / hr.w)); });
    }
}

void SamplerUI::librarySheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(BG, 200));
    tap({0, 0, W_, H_}, [this] { sheetOpen_ = false; });
    const Rect box{W_ * 0.22f, 24, W_ * 0.56f, H_ - 48};
    bevel(box, SURFACE);
    tap(box, [] {});
    strip({box.x + 6, box.y + 6, box.w - 82, 28}, ACCENT, "SAMPLE LIBRARY", 15);
    button({box.x + box.w - 70, box.y + 6, 60, 28}, "CLOSE", false, [this] { sheetOpen_ = false; }, 10);
    sheetList_ = {box.x + 6, box.y + 44, box.w - 12, box.h - 52};
    bevel(sheetList_, RAISED, true);
    const auto& lib = app_.library();
    // rows: a header per group, then its entries
    struct Row { bool head; std::string text; int entry; };
    std::vector<Row> rows;
    std::string g;
    for (int i = 0; i < (int)lib.size(); i++) { if (lib[(size_t)i].group != g) { g = lib[(size_t)i].group; rows.push_back({true, g, -1}); } rows.push_back({false, lib[(size_t)i].label, i}); }
    const float rh = 34;
    sheetScroll_ = std::min(std::max(0.f, sheetScroll_), std::max(0.f, rows.size() * rh - sheetList_.h));
    clip(sheetList_);
    for (size_t i = 0; i < rows.size(); i++) {
        const float y = sheetList_.y + i * rh - sheetScroll_;
        if (y + rh < sheetList_.y || y > sheetList_.y + sheetList_.h) continue;
        if (rows[i].head) { strip({sheetList_.x + 2, y + 4, sheetList_.w - 4, rh - 8}, VIOLET, rows[i].text, T_CAPTION); continue; }
        const bool cur = rows[i].entry == app_.libraryCursor();
        if (cur) rrect({sheetList_.x + 2, y + 1, sheetList_.w - 4, rh - 2}, 0, ACCENT);
        text(sheetList_.x + 14, y + rh / 2, T_BODY + 1, cur ? contrastInk(ACCENT) : TEXT, fit(rows[i].text, sheetList_.w - 30, T_BODY + 1), 0, cur);
    }
    endClip();
    hit(sheetList_, [this](float, float y) { sheetLastY_ = y; sheetMoved_ = 0; },
        [this](float, float y) { sheetScroll_ -= y - sheetLastY_; sheetMoved_ += std::fabs(y - sheetLastY_); sheetLastY_ = y; },
        [this, rows, rh]() {
            if (sheetMoved_ > 8) return;
            const int idx = (int)((sheetLastY_ - sheetList_.y + sheetScroll_) / rh);
            if (idx >= 0 && idx < (int)rows.size() && !rows[(size_t)idx].head) { app_.loadEntry(rows[(size_t)idx].entry); sheetOpen_ = false; }
        });
}

void SamplerUI::saveNotice(){
    const int state=app_.saveNoticeState();if(!state)return;
    waveRect_={};fieldRect_={};
    const float w=std::min(340.f,W_-32),h=100;
    Rect r{(W_-w)/2,(H_-h)/2,w,h};
    // Consume taps during the brief save feedback; repeated presses cannot
    // enqueue another save while the writer is busy or the notice is visible.
    rrect({0,0,W_,H_},0,theme::withAlpha(BG,155));
    hits_.clear();tap({0,0,W_,H_},[]{});
    bevel(r,SURFACE);
    const uint32_t col=state==3?RED:state==2?GREEN:ACCENT;
    strip({r.x+6,r.y+6,r.w-12,25},col,state==1?"SAVING":state==2?"SAVED":"SAVE FAILED",14);
    text(r.x+10,r.y+47,11,TEXT,fit(app_.saveNoticeLabel(),r.w-20,11),0,true);
    Rect bar{r.x+10,r.y+63,r.w-20,14};bevel(bar,WELL,true);
    if(state==1){
        float span=bar.w*.27f,x=bar.x+2+(bar.w-span-4)*float(now_%850)/850.f;
        rrect({x,bar.y+2,span,bar.h-4},0,col);
    }else if(state==2)rrect({bar.x+2,bar.y+2,bar.w-4,bar.h-4},0,col);
    text(r.x+10,r.y+88,9,DIM,state==1?"WRITING TO STORAGE":state==2?"FILE WRITTEN":"CHECK STORAGE / RECORDING");
}

// PROJECT menu: what used to be the SAVE / CLEAR buttons of the FM-1 (and more) on the screen.
void SamplerUI::projectSheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(kBg, 235));
    tap({0, 0, W_, H_}, [this] { projectOpen_ = false; });
    const float bw = std::min(460.f, W_ - 40), bh = 30, gap = 3;
    const int t = app_.track();
    const bool hasClip = st_.track_has_clip[t], any = app_.loopExists();
    const Rect box{(W_ - bw) / 2, 22, bw, H_ - 44};
    bevel(box, SURFACE);
    tap(box, [] {});
    strip({box.x + 8, box.y + 8, box.w - 100, 30}, AMBER, "PROJECT", 15);
    key({box.x + box.w - 84, box.y + 8, 72, 30}, "CLOSE", 0, false, [this] { projectOpen_ = false; }, false);
    const std::string ts = app_.toast();
    text(box.x + 18, box.y + 48, 10, kPaper, !ts.empty() ? ts : app_.loopText(), 0, true);
    float y = box.y + 66;
    auto row = [&](const std::string& label, const std::string& hint, bool enabled, bool hot, std::function<void()> f) {
        Rect r{box.x + 18, y, box.w - 36, bh};
        const bool destructive = label.find("CLEAR") != std::string::npos;
        const uint32_t col = destructive ? RED : label.find("LOOP LENGTH") != std::string::npos ? AMBER : ACCENT;
        bevel(r, hot ? RED : SURFACE, hot);
        rrect({r.x + 2, r.y + 3, 3, r.h - 6}, 0, enabled ? col : TRACK);
        text(r.x + 10, r.y + bh / 2 - 6, 11.5f, hot ? contrastInk(RED) : enabled ? col : DIM, fit(label, r.w - 20, 11.5f), 0, true);
        text(r.x + 10, r.y + bh / 2 + 9, 8.5f, hot ? contrastInk(RED) : DIM, fit(hint, r.w - 20, 8.5f), 0, false);
        if(enabled)tap(r, std::move(f));
        y += bh + gap;
    };
    row("SAVE PROJECT", "all tracks, loops and deck settings", true, false, [this] { projectOpen_=false;app_.requestProjectSave(); });
    row("FILES / PROJECTS", "open, rename or delete saved items", true, false, [this] { projectOpen_=false;app_.requestFileManager(); });
    const std::string tn = "TRACK " + std::to_string(t + 1);
    row("SAVE LOOP OF " + tn, "into the sample library (USER)", hasClip, false, [this] { app_.loopSave(); });
    row("CLEAR LOOP OF " + tn, "the other tracks keep theirs", hasClip, false, [this] { app_.loopClearTrack(); });
    const bool armed = clearArmed_ && now_ < clearArmed_;
    row(armed ? "TAP AGAIN: CLEAR ALL LOOPS" : "CLEAR ALL LOOPS", "the looper is clean again, loop length is free", any, armed, [this, armed] {
        if (armed) { app_.loopClear(); clearArmed_ = 0; } else clearArmed_ = now_ + 3000;
    });
    row("OPEN THE EDITOR", "the older sample / page editor", true, false, [this] { app_.setEditor(true); projectOpen_ = false; });
    row("FIELD RECORDING", "examine, restore and extract one recording", true, false, [this] { app_.setFieldMode(true); projectOpen_ = false; });
    row(std::string("LOOP SOURCE: ") + app_.knobs(SamplerApp::P_SAVE)[1].value, "what a take records: the track's sound or the microphone", true, false,
        [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
}

#include "GenerativeDeck.inc"

#include "WaveformGestures.inc"
