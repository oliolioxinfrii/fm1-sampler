#include "SamplerUI.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace theme;

// Shared shell colors; signal displays use the high-contrast trace palette.
namespace {
constexpr float kPi = 3.14159265f;
uint32_t kBg, kPaper, kOrange, kInk, kLine, kDim;
void syncColors() { kBg = theme::BG; kPaper = theme::TEXT; kOrange = theme::AMBER; kInk = theme::BG; kLine = theme::TRACK; kDim = theme::DIM; }
}

namespace {
float clamp01(float v) { return std::min(1.f, std::max(0.f, v)); }
std::string num(const char* f, double v) { char b[48]; snprintf(b, sizeof b, f, v); return b; }
const SamplerApp::Page kTabs[] = {SamplerApp::P_HOME, SamplerApp::P_ECHO, SamplerApp::P_GRAIN, SamplerApp::P_MOTION, SamplerApp::P_FX, SamplerApp::P_EFILTER, SamplerApp::P_EDELAY, SamplerApp::P_EREVERB, SamplerApp::P_ENV, SamplerApp::P_LFO, SamplerApp::P_ARP, SamplerApp::P_SEQ, SamplerApp::P_ETAPE, SamplerApp::P_EMIX, SamplerApp::P_MIX, SamplerApp::P_KEY, SamplerApp::P_SAVE, SamplerApp::P_LEN, SamplerApp::P_TEMPO};
const char* kTabName[] = {"SAMPLE", "ECHO", "GRN", "MOT", "FX", "FILT", "DLY", "REV", "ENV", "LFO", "ARP", "TAPE", "E.TP", "E.MX", "MIX", "KEY", "LOOP", "LEN", "BPM"};
const char* kShortTab[] = {"S", "G", "g", "m", "FX", "F", "D", "R", "E", "L", "A", "T", "W", "X", "M", "K", "R", "N", "B"};
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

SamplerUI::SamplerUI(SamplerApp& app) : app_(app) {
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
void SamplerUI::frame(uint32_t now) {
    now_ = now;
    st_ = app_.status();
    syncColors();
    ops_.clear(); hits_.clear();
    rrect({0, 0, W_, H_}, 0, BG);
    const float tb = 44, left = 176, right = 252;
    if (!app_.editor()) {   // the cassette deck fills the screen; the old sample / page editor stays one tap away (EDIT)
        deck({0, 0, W_, H_});
        if (sheetOpen_) librarySheet();
        if (projectOpen_) projectSheet();
        return;
    }
    topBar({0, 0, W_, tb});
    trackList({MARGIN, tb + MARGIN, left - MARGIN, H_ - tb - 2 * MARGIN});
    sampleArea({left + MARGIN, tb + MARGIN, W_ - left - right - 2 * MARGIN, H_ - tb - 2 * MARGIN});
    pagePanel({W_ - right, tb + MARGIN, right - MARGIN, H_ - tb - 2 * MARGIN});
    if (sheetOpen_) librarySheet();
}

void SamplerUI::topBar(Rect r) {
    bevel(r, SURFACE);
    rrect({0, 0, W_, 3}, 0, ACCENT);
    const float cy = r.h / 2;
    float x = MARGIN;
    // PLAY / STOP
    Rect pl{x, 6, 44, r.h - 12};
    bevel(pl, st_.playing ? GREEN : RAISED, st_.playing);
    if (st_.playing) { rrect({pl.x + 15, cy - 8, 5, 16}, 0, contrastInk(GREEN)); rrect({pl.x + 24, cy - 8, 5, 16}, 0, contrastInk(GREEN)); }
    else tri(pl.x + 16, cy - 9, pl.x + 16, cy + 9, pl.x + 30, cy, GREEN);
    tap(pl, [this] { app_.transportPlay(); });
    x += 52;
    // REC: arms the first take on a clean looper, overdub afterwards
    Rect rc{x, 6, 86, r.h - 12};
    const bool recOn = st_.freeRecording || st_.track_clip_rec[app_.track()], armed = st_.freeArmed;
    bevel(rc, recOn ? RED : RAISED, recOn);
    if (armed) stroke(rc, 0, RED, 2);
    circle(rc.x + 18, cy, 6, recOn ? contrastInk(RED) : RED);
    text(rc.x + 28, cy, 9.5f, recOn ? contrastInk(RED) : TEXT, app_.loopExists() ? "OVERDUB" : "REC", 0, true);
    tap(rc, [this] { app_.loopRecord(); });
    x += 94;
    button({x, 6, 56, r.h - 12}, "SAVE", false, [this] { app_.loopSave(); }, 11, true, GREEN);
    x += 62;
    button({x, 6, 56, r.h - 12}, "CLEAR", false, [this] { app_.loopClear(); }, 11, true, RED);
    x += 62;
    button({x, 6, 56, r.h - 12}, "DECK", false, [this] { app_.setEditor(false); }, 11);
    x += 70;
    text(x, cy - 6, 12, st_.freeRecording || recOn || armed ? RED : AMBER, app_.loopText(), 0, true);
    text(x, cy + 11, 9.5f, DIM, std::string("LOOP SOURCE: ") + (app_.knobs(SamplerApp::P_SAVE)[1].value), 0, false);
    tap({x - 4, 0, 200, r.h}, [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
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
    std::string head;
    switch (pg) {
    case P::P_EFILTER: {
        head = ts.echoInsert.filterMix > 0.001f ? "ECHOFIELD FILTER" : "FILTER / BYPASSED";
        const float cutoff=kn[1].norm, resonance=kn[2].norm;
        std::vector<std::pair<float,float>> points;
        for(int i=0;i<=80;++i) {
            float f=float(i)/80, delta=(f-cutoff)*16, low=1/(1+std::exp(delta));
            float band=std::exp(-delta*delta*.14f);
            float v=ts.echoInsert.filter==0?low:ts.echoInsert.filter==1?band:1-low;
            v=std::clamp(v+resonance*.35f*band,0.f,1.f);
            points.push_back({w.x+2+f*(w.w-4),w.y+w.h-4-v*(w.h-8)});
        }
        poly(points,ts.echoInsert.filterMix>0.001f?signal:WELL_DIM,1.8f);
        break; }
    case P::P_EDELAY: {
        head="ECHOFIELD DELAY";
        const float span=w.w-16, feedback=kn[1].norm;
        for(int j=0;j<6;++j){
            const float x=w.x+8+span*j/5, rad=std::max(2.f,std::min(13.f,w.h*.19f)*std::pow(std::max(.15f,feedback),float(j)*.5f));
            const float yy=cy+(kn[3].norm>.5f?(j%2?1:-1)*w.h*.18f:0);
            circle(x,yy,rad,j?signal:TRACE_AMBER);
            if(rad>3)circle(x,yy,rad-1.7f,WELL);
        }
        break; }
    case P::P_EREVERB: {
        head="ECHOFIELD FDN + SHIMMER";
        for(int j=0;j<8;++j){
            float x=w.x+(j+.5f)*w.w/8, h=(w.h-12)*(.25f+.75f*kn[1].norm)*(j+5)/12.f;
            vbar(x,cy-h/2,cy+h/2,signal,4);
            if(kn[3].norm>0) vbar(x+5,cy-h/2,cy-h/2+h*kn[3].norm,TRACE_AMBER,2);
        }
        break; }
    case P::P_ETAPE: {
        head=ts.echoInsert.tape>.001f?"ECHOFIELD TAPE":"TAPE / BYPASSED";
        std::vector<std::pair<float,float>> points;
        for(int i=0;i<=100;++i){
            float f=float(i)/100, v=.55f*kn[0].norm*std::sin(f*2*kPi)+.16f*kn[1].norm*std::sin(f*14*kPi);
            points.push_back({w.x+2+f*(w.w-4),cy+v*(w.h/2-4)});
        }
        poly(points,ts.echoInsert.tape>.001f?signal:WELL_DIM,1.8f);
        break; }
    case P::P_EMIX: {
        head="ECHOFIELD INSERT MIX";
        for(int j=0;j<4;++j){float x=w.x+(j+.5f)*w.w/4;vbar(x,w.y+w.h-4-(w.h-8)*kn[j].norm,w.y+w.h-4,encoderTrace(j),10);}
        break; }
    case P::P_FX: {   // reverb = a room, a wire cube: its size is the size, the dim rays from the source to the walls are the wet part; delay = an echo: a circle and its repeats, spacing = time, shrinking = feedback
        head = "REVERB + DELAY";
        const float labelY = w.y + w.h - 7;
        const float plotH = w.h - 19;
        const float ccy = w.y + plotH / 2;
        const float s = (0.43f + 0.57f * kn[1].norm) * std::min(plotH / 1.38f - 3.f, w.w * 0.22f), d = s * 0.38f, ccx = w.x + w.w * 0.25f;   // kn: reverb send, reverb size, delay send, delay time
        const float fx = ccx - (s + d) / 2, fy = ccy - (s + d) / 2 + d;   // front square, top left
        const float v[8][2] = {{fx, fy}, {fx + s, fy}, {fx + s, fy + s}, {fx, fy + s}, {fx + d, fy - d}, {fx + s + d, fy - d}, {fx + s + d, fy + s - d}, {fx + d, fy + s - d}};
        const float sx = fx + (s + d) / 2, sy = fy + (s - d) / 2;   // the source in the middle of the room
        const int rays = (int)std::lround(kn[0].norm * 8);
        for (int i = 0; i < rays; i++) line(sx, sy, v[i][0], v[i][1], TRACE_BLUE, 1.5f);
        auto edge = [&](int a, int b, uint32_t c) { line(v[a][0], v[a][1], v[b][0], v[b][1], c, 2.7f); circle(v[a][0], v[a][1], 1.35f, c); circle(v[b][0], v[b][1], 1.35f, c); };
        edge(7, 4, WELL_DIM); edge(7, 6, WELL_DIM); edge(3, 7, WELL_DIM);   // hidden edges
        edge(0, 1, signal); edge(1, 2, signal); edge(2, 3, signal); edge(3, 0, signal);
        edge(4, 5, signal); edge(5, 6, signal); edge(0, 4, signal); edge(1, 5, signal); edge(2, 6, signal);
        circle(sx, sy, 4, signal);
        const float fb = 0.4f + 0.5f * kn[2].norm, sp = (0.06f + 0.075f * kn[3].norm) * w.w, ex = w.x + w.w * 0.54f;
        float rr = std::min(13.f, plotH / 3), x = ex;
        for (int k = 0; k < 9 && rr >= 2.5f && x + rr <= w.x + w.w - 2; k++, x += sp, rr *= fb) {
            const uint32_t c = k == 0 ? TRACE_AMBER : k == 1 ? TRACE_VIOLET : WELL_DIM;
            circle(x, ccy, rr, c);
            if (k > 0 && rr > 5) circle(x, ccy, rr - 2.7f, WELL);
        }
        text(ccx, labelY, 9, WELL_DIM, "REVERB", 1, true);
        text(w.x + w.w * 0.75f, labelY, 9, WELL_DIM, "DELAY", 1, true);
        break; }
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
        poly(pts, signal, 2.7f);
        const float lv = st_.lfoValue[t] * amp;
        vbar(w.x + w.w - 5, cy, cy - lv, TRACE_GREEN, 4);
        break; }
    case P::P_ARP: {   // the notes the arpeggiator plays: order from the pattern, count from the octaves, width from the length; dim while it is off
        head = std::string("ARP ") + kn[1].value;
        const int n = 3 * (ts.arpOct + 1); std::vector<int> seq;
        if (ts.arpPatt == 0) for (int i = 0; i < n; i++) seq.push_back(i);
        else if (ts.arpPatt == 1) for (int i = n - 1; i >= 0; i--) seq.push_back(i);
        else if (ts.arpPatt == 2) { for (int i = 0; i < n; i++) seq.push_back(i); for (int i = n - 2; i > 0; i--) seq.push_back(i); }
        else if (ts.arpPatt == 3) { for (int i = 0, j = n - 1; i <= j; i++, j--) { seq.push_back(i); if (i != j) seq.push_back(j); } }
        else { unsigned s = 5; for (int i = 0; i < std::max(8, n); i++) { s = s * 1103515245u + 12345u; seq.push_back((int)(s >> 16) % n); } }
        { const std::vector<int> one = seq; while (seq.size() < 12) seq.insert(seq.end(), one.begin(), one.end()); }   // short patterns repeat to fill the picture
        const float pitch = (w.w - 12) / std::max<float>(12, (float)seq.size()), bw = pitch * (0.25f + 0.65f * ts.arpLen);
        const uint32_t c = ts.arpRate == 0 ? GRID : signal;
        for (size_t i = 0; i < seq.size(); i++) rrect({w.x + 6 + i * pitch, base - (0.18f + 0.82f * (seq[i] + 1) / n) * H, bw, (0.18f + 0.82f * (seq[i] + 1) / n) * H}, 0, c);
        break; }
    case P::P_SEQ: {   // the tape: a wave that wobbles (wobble), saturates (exciter), turns into stairs (lo-fi); the speed sets how many cycles fit
        head = "TAPE";
        const float cyc = 2.f + 6.f * kn[0].norm, g = 1.f + 4.f * kn[1].norm, lo = kn[2].norm, wob = kn[3].norm;
        const float steps = std::max(24.f, 150.f - lo * 126.f), lev = std::max(3.f, 40.f - lo * 37.f), amp = w.h / 2 - 6;
        std::vector<std::pair<float, float>> pts; const int n = 150;
        for (int i = 0; i <= n; i++) {
            const float tt = std::floor((float)i / n * steps) / steps, ph = 2 * kPi * (cyc * tt + 0.12f * wob * std::sin(2 * kPi * 1.7f * tt));
            float v = std::tanh(g * std::sin(ph)) / std::tanh(g); v = std::round(v * lev) / lev;
            pts.push_back({w.x + 4 + i * (w.w - 8) / n, cy - v * amp});
        }
        poly(pts, signal, 2.7f);
        break; }
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
        break; }
    case P::P_KEY: {   // two octaves of semitones: the scale in ink, the root in orange, the rest dim
        const auto sc = app_.scale();
        head = std::string(P::noteName(sc.root)) + " " + P::scaleName(sc.scale);
        const float pitch = w.w / 24, kh = 44;
        for (int i = 0; i < 24; i++) {
            const int pc = i % 12; const bool in = P::inScale(sc.scale, pc);
            rrect({w.x + i * pitch + 2, cy - kh / 2 - 4, pitch - 4, kh}, 0, pc == 0 ? TRACE_AMBER : in ? signal : GRID);
            if (pc == 0) text(w.x + i * pitch + pitch / 2, cy + kh / 2 + 12, 9, WELL_DIM, P::noteName((sc.root + pc) % 12), 1, true);
        }
        break; }
    case P::P_SAVE: {   // what a take records: the input level against the trigger line, the source in big dim letters
        head = std::string("SOURCE: ") + kn[1].value;
        const float thr = app_.micThreshold01(), lv = std::min(1.f, st_.inputPeak * 2.f), x0 = w.x + 4, ww = w.w - 8;
        rrect({x0, cy - 5, ww, 10}, 0, GRID);
        rrect({x0, cy - 5, std::max(2.f, ww * lv), 10}, 0, lv > std::min(1.f, 2.f * app_.micThreshold()) ? TRACE_RED : TRACE_GREEN);
        vbar(x0 + ww * thr, cy - 26, cy + 26, TRACE_AMBER, 2.7f);
        text(x0, w.y + w.h - 8, 9, WELL_DIM, "TRIGGER LEVEL " + num("%.0f DB", 20 * std::log10(app_.micThreshold())), 0, true);
        break; }
    case P::P_TEMPO:
    case P::P_LEN: {   // the loop as bars: one block per bar with its beats, the orange playhead
        const int bars = std::max(1, st_.loopBars), shown = std::min(bars, 16);
        head = bars > 0 && st_.loopSamples > 0 ? std::to_string(bars) + (bars == 1 ? " BAR  " : " BARS  ") + num("%.0f BPM", st_.bpm) : "NO LOOP";
        const float gap = 4, bw = (w.w - gap * (shown - 1)) / shown, bh = 44;
        for (int b = 0; b < shown; b++) {
            const float x = w.x + b * (bw + gap);
            rrect({x, cy - bh / 2, bw, bh}, 0, st_.loopSamples > 0 ? signal : GRID);
            for (int q = 1; q < 4; q++) vbar(x + bw * q / 4, cy - bh / 2 + 4, cy + bh / 2 - 4, WELL, 1.2f);
        }
        if (st_.loopSamples > 0) vbar(w.x + 2 + clamp01(loopPos()) * (w.w - 4), cy - 34, cy + 34, TRACE_AMBER, 2.7f);
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
    const bool echo = app_.page() == SamplerApp::P_ECHO || app_.page() == SamplerApp::P_GRAIN || app_.page() == SamplerApp::P_MOTION;
    if (app_.page() != SamplerApp::P_HOME && !echo && micS == 0) { pageView(r); return; }
    const uint32_t header = micS ? RED : echo ? VIOLET : ACCENT;
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

// A knob of the app (drag up / down to turn): a ring, an orange arc for the value, a pointer, the value under it and its name.
void SamplerUI::knob(float cx, float cy, SamplerApp::Page page, int idx, bool) {
    auto k = app_.knobs(page);
    const auto& p = k[(size_t)idx];
    if (!p.valid) return;
    const float half = std::max(12.f, std::min(43.f, (W_ - 24 - 440) / 8 - 3));
    const float R = std::min(25.f, half - 5), n = clamp01(p.norm);
    const uint32_t col = encoderColor(idx);
    bevel({cx - half, cy - 33, 2 * half, 99}, SURFACE);
    auto pt = [&](float rad, float u, float& x, float& y) { const float a = (-135.f + u * 270.f) * kPi / 180; x = cx + std::sin(a) * rad; y = cy - std::cos(a) * rad; };
    auto arc = [&](float u1, uint32_t c, float w) {
        const int N = std::max(1, (int)std::ceil(u1 * 28)); float x0, y0; pt(R, 0, x0, y0); circle(x0, y0, w / 2, c);
        for (int i = 1; i <= N; i++) { float x1, y1; pt(R, u1 * i / N, x1, y1); line(x0, y0, x1, y1, c, w); circle(x1, y1, w / 2, c); x0 = x1; y0 = y1; }
    };
    arc(1.f, kLine, 2.7f);
    if (n > 0.004f) arc(n, col, 4.f);
    float x, y; pt(R - 8, n, x, y);
    line(cx, cy, x, y, kPaper, 3.3f); circle(x, y, 1.65f, kPaper); circle(cx, cy, 1.65f, kPaper);
    rrect({cx - half + 4, cy + 29, 2 * half - 8, 20}, 0, WELL);
    text(cx, cy + 39, T_VALUE, encoderTrace(idx), fit(p.value, 2 * half - 12, T_VALUE), 1, true);
    text(cx, cy + 58, T_CAPTION, col, fit(p.name, 2 * half - 4, T_CAPTION), 1, true);
    Rect hr{cx - half, cy - 33, 2 * half, 99};
    hit(hr, [this, n](float, float y) { dragY_ = y; dragV_ = n; },
        [this, page, idx](float, float y) { app_.knobs(page)[(size_t)idx].set(clamp01(dragV_ + (dragY_ - y) / 110.f)); });
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
            rrect({tx + a * bw, cy - std::max(0.5f, v * (wh / 2 - 3)), std::max(1.f, bw * 0.62f), std::max(1.f, v * (wh - 6))}, 0, TRACE_RED);
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
        const float cy = wy + wh / 2, bw = tw / n, hx0 = tx + std::min(1.f, loopPos()) * tw;   // played = ink, still to come = dim
        for (int i = 0; i < n; i++) { const float v = std::min(1.f, pk[i] / mx); if (v > 0.01f) rrect({tx + i * bw, cy - v * (wh / 2 - 3), std::max(1.f, bw * 0.62f), std::max(1.f, v * (wh - 6))}, 0, rec ? TRACE_RED : tx + i * bw < hx0 ? TRACE_GREEN : trackTrace(t)); }
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
    case 4: {   // theme: a ring with the right half filled
        circle(cx, cy, 9.5f, c); circle(cx, cy, 6.8f, SURFACE);
        for (int i = 0; i < 12; i++) { const float a0 = -kPi / 2 + kPi * i / 12, a1 = -kPi / 2 + kPi * (i + 1) / 12; tri(cx, cy, cx + 7.6f * std::cos(a0), cy + 7.6f * std::sin(a0), cx + 7.6f * std::cos(a1), cy + 7.6f * std::sin(a1), c); }
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
        bevel({M - 10, 6, 304, 272}, SURFACE);
        bevel({mx - 31, 6, 62, 272}, SURFACE);
        bevel({x0 - 8, 6, w0 + 16, 272}, SURFACE);
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
    scope({M - 4, 86, 292, 133});
    cassette({M - 4, 86, 292, 150});
    // transport: play, stop, record / overdub, clear the loop of this track; the active one is orange and underlined
    const float ty = 248;
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
    // tools in the middle: theme, microphone, file from disk, project
    const uint32_t micCol = mic == 1 || mic == 2 ? RED : GREEN;
    for (int i = 0; i < 4; ++i) bevel({mx - 23, 34.f + i * 64, 46, 44}, SURFACE);
    icon(4, mx, 56, VIOLET);  tap({mx - 32, 28, 64, 56}, [this] { setLight(!theme::light); });
    icon(5, mx, 120, micCol); tap({mx - 32, 92, 64, 56}, [this] { app_.micSample(); });
    icon(6, mx, 184, ACCENT); tap({mx - 32, 156, 64, 56}, [this] { app_.requestFile(); });
    icon(7, mx, 248, projectOpen_ ? AMBER : ACCENT); tap({mx - 32, 220, 64, 56}, [this] { projectOpen_ = true; });
    // the sample of this track, its page and the four knobs of the page
    sampleView({x0, 14, w0, 118});
    const float py = 163;
    const float tabW = std::min(19.f, std::max(5.f, (w0 - 90.f) / float(SamplerApp::kPages)));
    strip({x0, py - 12, w0 - float(SamplerApp::kPages) * tabW - 4, 24}, pageColor(app_.page()), SamplerApp::pageName(app_.page()), T_BODY);
    for (int i = 0; i < SamplerApp::kPages; i++) {   // the pages as notches: tap one to go there (the FM-1 buttons do the same)
        Rect nr{zr - (SamplerApp::kPages - i) * tabW, py - 14, tabW, 28};
        const bool on = kTabs[i] == app_.page(); const uint32_t col = app_.pageAvailable(kTabs[i]) ? pageColor(kTabs[i]) : DIM;
        bevel({nr.x + 1, py - 10, tabW - 2, 20}, on ? col : SURFACE, on);
        text(nr.x + tabW / 2, py, 8, on ? contrastInk(col) : col, kShortTab[i], 1, true);
        if (app_.pageAvailable(kTabs[i])) tap(nr, [this, i] { app_.gotoPage(kTabs[i]); });
    }
    for (int i = 0; i < 4; i++) knob(x0 + (i + 0.5f) * w0 / 4, 212, app_.page(), i);
    timeline({M, 288, W_ - 2 * M, H_ - 288 - 8});
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
        const std::string lab = app_.sampleLabel(i);
        text(row.x + 30, row.y + rh / 2 - 6, 11, lab.empty() ? DIM : TEXT, fit(lab.empty() ? "EMPTY" : lab, r.w - 40, 11), 0, true);
        // level and loop mark
        const float lv = std::min(1.f, st_.track_peak[i] * 1.5f);
        rrect({row.x + 30, row.y + rh - 12, r.w - 44, 3}, 0, OUTLINE);
        rrect({row.x + 30, row.y + rh - 12, (r.w - 44) * lv, 3}, 0, col);
        if (st_.track_clip_rec[i]) circle(row.x + r.w - 10, row.y + 10, 4, RED);
        else if (st_.track_has_clip[i]) circle(row.x + r.w - 10, row.y + 10, 3.5f, GREEN);
        tap(row, [this, i] { app_.selectTrack(i); });
    }
}

void SamplerUI::sampleArea(Rect r) {
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
    const int bins = std::max(20, (int)(w.w / 3));
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
    {
    auto pk = app_.peaks(t, bins);
    const float cy = w.y + w.h / 2, bw = w.w / bins;
    for (int i = 0; i < (int)pk.size(); i++) {
        const float pos = (i + 0.5f) / bins, v = std::min(1.f, pk[(size_t)i] * 1.1f);
        const bool in = pos >= ts.start && pos <= ts.end;
        rrect({w.x + i * bw + 0.5f, cy - std::max(1.f, v * (w.h / 2 - 8)), std::max(1.f, bw - 1), std::max(2.f, v * (w.h - 16))}, 0, in ? TRACE_GREEN : WELL_DIM);
    }
    if (pk.empty()) text(w.x + w.w / 2, cy, T_BODY, WELL_DIM, "NO SAMPLE", 1, false);
    for (int j = 0; j < st_.voiceN[t]; j++) {   // where the sounding voices are reading
        const float vx = w.x + 4 + clamp01(st_.voicePos[t][j]) * (w.w - 8);
        line(vx, w.y + 22, vx, w.y + w.h - 10, encoderTrace(j), 1.5f);
        circle(vx, w.y + w.h - 10, 3.5f, encoderTrace(j));
    }
    }
    after_wave:
    // start / end handles (not while the microphone take is shown)
    for (int h = 0; h < (app_.micState() == 1 || app_.micState() == 2 ? 0 : 2); h++) {
        const float v = h ? ts.end : ts.start, hx = w.x + 2 + v * (w.w - 4);
        line(hx, w.y + 4, hx, w.y + w.h - 4, encoderTrace(h), 2);
        rrect({hx - (h ? 10 : 0), w.y + 4, 10, 14}, 0, encoderTrace(h));
        hit({hx - 18, w.y, 36, w.h},
            [](float, float) {},
            [this, h, w](float x, float) { auto k = app_.knobs(SamplerApp::P_HOME); k[(size_t)h].set(clamp01((x - w.x - 2) / (w.w - 4))); });
    }
    if (ts.algo == 4 && app_.micState() == 0 && app_.hasSample(t)) echoPosition(w, 2.f);
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
    // library row
    Rect lr{r.x, ls.y + ls.h + 12, r.w, 34};
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
    if (!ts_.empty()) text(r.x, std::min(br.y + br.h + 14, r.y + r.h - 7), 11, ACCENT, fit(ts_, r.w, 11), 0, true);
}

void SamplerUI::pagePanel(Rect r) {
    constexpr int cols=7;
    const float tw = (r.w - (cols-1)*4) / cols;
    for (int i = 0; i < SamplerApp::kPages; i++) {
        Rect c{r.x + (i % cols) * (tw + 4), r.y + (i / cols) * 32, tw, 28};
        button(c, kTabName[i], app_.page() == kTabs[i], [this, i] { app_.gotoPage(kTabs[i]); }, 9.5f, true, app_.pageAvailable(kTabs[i]) ? pageColor(kTabs[i]) : DIM);
    }
    auto k = app_.knobs();
    const float tabsH = ((SamplerApp::kPages + cols-1) / cols) * 32 + 8;
    const float top = r.y + tabsH, rh = (r.h - tabsH) / 4;
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

// PROJECT menu: what used to be the SAVE / CLEAR buttons of the FM-1 (and more) on the screen.
void SamplerUI::projectSheet() {
    rrect({0, 0, W_, H_}, 0, theme::withAlpha(kBg, 235));
    tap({0, 0, W_, H_}, [this] { projectOpen_ = false; });
    const float bw = std::min(460.f, W_ - 40), bh = 34, gap = 4;
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
        tap(r, std::move(f));
        y += bh + gap;
    };
    const std::string tn = "TRACK " + std::to_string(t + 1);
    row("SAVE LOOP OF " + tn, "into the sample library (USER)", hasClip, false, [this] { app_.loopSave(); });
    row("CLEAR LOOP OF " + tn, "the other tracks keep theirs", hasClip, false, [this] { app_.loopClearTrack(); });
    const bool armed = clearArmed_ && now_ < clearArmed_;
    row(armed ? "TAP AGAIN: CLEAR ALL LOOPS" : "CLEAR ALL LOOPS", "the looper is clean again, loop length is free", any, armed, [this, armed] {
        if (armed) { app_.loopClear(); clearArmed_ = 0; } else clearArmed_ = now_ + 3000;
    });
    row("OPEN THE EDITOR", "the older sample / page editor", true, false, [this] { app_.setEditor(true); projectOpen_ = false; });
    row("LOOP LENGTH x2", "the loop doubled, its content repeated", any, false, [this] { app_.lenAction(true); });
    row("LOOP LENGTH /2", "half as long, the second half is dropped", any, false, [this] { app_.lenAction(false); });
    row(std::string("LOOP SOURCE: ") + app_.knobs(SamplerApp::P_SAVE)[1].value, "what a take records: the track's sound or the microphone", true, false,
        [this] { auto k = app_.knobs(SamplerApp::P_SAVE); k[1].step(k[1].norm > 0.5f ? -1 : 1); });
}
