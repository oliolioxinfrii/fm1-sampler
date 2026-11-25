#include "SamplerApp.h"
#include "../ui/Theme.h"
#include "../ui/SamplerUI.h"
#include "../ui/SurfaceArt.h"
#include <Common.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr uint16_t rgb(int r, int g, int b) { return (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)); }
// Convert the phone's shared theme to the display's native RGB565.
uint16_t rgb565(uint32_t c) { return rgb((c >> 16) & 255, (c >> 8) & 255, c & 255); }
uint16_t BLACK, PAPER, DIM, ACCENT, BAR, PANEL, WELL, GRID, TRACE, TIME, RECORD, LIVE, INK;
void setPalette(bool) {
    BLACK = rgb565(theme::BG); PAPER = rgb565(theme::TEXT); DIM = rgb565(theme::DIM);
    ACCENT = rgb565(theme::ACCENT); BAR = rgb565(theme::TRACK); PANEL = rgb565(theme::SURFACE);
    WELL = rgb565(theme::WELL); GRID = rgb565(theme::GRID); TRACE = rgb565(theme::TRACE_GREEN);
    TIME = rgb565(theme::TRACE_AMBER); RECORD = rgb565(theme::RED); LIVE = rgb565(theme::GREEN);
    INK = rgb565(theme::WELL_TEXT);
}
uint32_t pageAccent(SamplerApp::Page p) {
    using P = SamplerApp;
    switch (p) {
    case P::P_ETAPE: case P::P_EMIX: return theme::AMBER;
    case P::P_GRAIN: case P::P_MOTION: case P::P_EFILTER: case P::P_EDELAY: case P::P_EREVERB:
    case P::P_FX: case P::P_LFO: case P::P_ECHO: return theme::VIOLET;
    case P::P_ENV: case P::P_ARP: case P::P_KEY: return theme::GREEN;
    case P::P_SEQ: case P::P_LEN: case P::P_TEMPO: return theme::AMBER;
    case P::P_SAVE: return theme::RED;
    case P::P_MIX: return theme::PINK;
    default: return theme::ACCENT;
    }
}
const char* kPage[] = {"SAMPLE", "FX", "ENVELOPE", "LFO", "ARP", "TAPE", "MIXER", "KEY", "LOOP", "LENGTH", "TEMPO", "ECHOFIELD", "FILTER", "DELAY", "REVERB", "EF TAPE", "EF MIX", "GRAIN", "MOTION"};
const char* kEchoMode[] = {"CLOUD", "DUST", "TAPE", "BLOOM", "PULSE", "DRIFT"};
const char* kAlgo[] = {"MELODIC", "ONE-SHOT", "LOOPED", "KIT/SLICE", "GRANULAR", "PAULSTRETCH"};
const char* kScaleName[] = {"CHROM", "MIX", "MAJ", "MIN", "HMIN", "PHRYG", "MIN+", "OFF"};
const char* kNote[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* kArpRate[] = {"OFF", "1", "2", "2T", "4", "4T", "8", "8T", "16", "16T", "32", "32T", "64"};
const char* kArpPatt[] = {"UP", "DOWN", "UPDN", "CONV", "RAND"};
const char* kLfoTarget[] = {"PITCH", "FILTER", "VOLUME", "PAN"};
const char* kLfoShape[] = {"SINE", "TRI", "SQUARE", "S&H"};
std::vector<uint8_t> n14(int v) { return {(uint8_t)(v >> 7 & 127), (uint8_t)(v & 127)}; }
std::vector<uint8_t> col(uint16_t c) { return {(uint8_t)(c >> 14 & 3), (uint8_t)(c >> 7 & 127), (uint8_t)(c & 127)}; }
void cat(std::vector<uint8_t>& a, const std::vector<uint8_t>& b) { a.insert(a.end(), b.begin(), b.end()); }
std::string pad(std::string s, size_t n) { if (s.size() > n) s.resize(n); while (s.size() < n) s += ' '; return s; }
float cl(float v, float lo = 0.f, float hi = 1.f) { return std::min(hi, std::max(lo, v)); }
std::string fmt(const char* f, double v) { char b[48]; snprintf(b, sizeof b, f, v); return b; }
std::string pct(float v) { return fmt("%.0f%%", v * 100); }
// Tonality. The firmware is kept on a plain chromatic keyboard (scale OFF, notes 53+key) so keys outside the scale stay playable; the app only
// lights the keys that belong to the scale, shifts the notes by the octave and builds chords itself.
const int kScaleN[7] = {12, 7, 7, 7, 7, 7, 8};
const int kScaleStep[7][12] = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10},
                               {0, 2, 3, 5, 7, 8, 11}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 10, 11}};
int scaleIndex(int pc, int scale) { for (int i = 0; i < kScaleN[scale]; i++) if (kScaleStep[scale][i] == pc) return i; return -1; }   // degree of a pitch class, -1 outside
int degreeNote(int d, int scale, int root) { const int n = kScaleN[scale], o = d >= 0 ? d / n : -((-d + n - 1) / n); return root + 12 * o + kScaleStep[scale][d - o * n]; }   // degree 0 = the root of octave 0
float speedOf(float n) { return 0.25f * std::pow(16.f, n); }          // 0.25..4
float micThrOf(float v) { return 0.005f * std::pow(100.f, v); }       // -46..-6 dBFS
float paulFactor(float n) { return 2.f * std::pow(32.f, n); }                     // slow-down 2x .. 64x
float scThrDb(float n) { return -40.f * (1.f - n); }                              // 0 .. -40 dB
float scRatioOf(float n) { return 1.f + 19.f * n * n; }                          // 1:1 .. 20:1
float loopSpeedOf(float n) { return 0.25f * std::pow(8.f, n); }       // 0.25..2
float lfoHz(float n) { return 0.1f * std::pow(200.f, n); }            // 0.1..20
}  // namespace

const char* SamplerApp::pageName(Page p) { return kPage[p]; }
bool SamplerApp::inScale(int scale, int pc) { return scale < 7 ? scaleIndex(((pc % 12) + 12) % 12, scale) >= 0 : ((pc % 12) + 12) % 12 == 0; }
const char* SamplerApp::echoModeName(int i) { return kEchoMode[std::clamp(i, 0, int(EchoParams::ModeCount) - 1)]; }
const char* SamplerApp::algoName(int i) { return kAlgo[std::min(5, std::max(0, i))]; }
const char* SamplerApp::scaleName(int i) { return kScaleName[std::min(7, std::max(0, i))]; }
const char* SamplerApp::noteName(int r) { return kNote[((r % 12) + 12) % 12]; }

SamplerApp::SamplerApp(Session& s, Send send) : s_(s), send_(std::move(send)) {
    cells_.resize(400); rects_.assign(420, ~0ull); setPalette(theme::light);
    for (int& h : heldNote_) h = -1;
    for (int i = 0; i < kTracks; i++) { ts_[i].vol = 0.8f; }
    loadEcho();
    refreshLibrary();
    applyFx();
    s_.setRetLevel(1, 0.7f);
    s_.setLoopSource(loopSrc_);
    // a first session to play with: keys on 1, a drum kit on 2, bass on 3
    auto first = [&](const char* group, int track) {
        for (size_t i = 0; i < lib_.size(); i++) if (lib_[i].group == group) { ts_[track].entry = (int)i; if (group == std::string("Drums")) ts_[track].algo = 3; s_.loadSamplePath(lib_[i].path, lib_[i].kit, track); return; }
    };
    // the default project: Tape Key, Acoustic bass, Shiny, Trap Kit
    auto byLabel = [&](const char* label, int track) {
        for (size_t i = 0; i < lib_.size(); i++) if (lib_[i].label == label) { ts_[track].entry = (int)i; if (lib_[i].kit) ts_[track].algo = 3; s_.loadSamplePath(lib_[i].path, lib_[i].kit, track); return; }
    };
    byLabel("Tape Key", 0); byLabel("Acoustic", 1); byLabel("Shiny", 2); byLabel("TrapKit", 3); (void)first;
    for (int t = 0; t < kTracks; t++) { applyMix(t); editEchoFx(t); }
    echoSaveAt_ = 0;
    if (ts_[0].entry >= 0) libCursor_ = ts_[0].entry;
}

// ---- library ----
void SamplerApp::refreshLibrary() {
    lib_.clear();
    const std::string dir = s_.sampleDir();
    for (auto& f : s_.sampleFolders())
        for (auto& file : s_.sampleFiles(f)) {
            std::string stem = file.substr(0, file.rfind('.'));
            lib_.push_back({stem, f, dir + "/" + f + "/" + file, f == "Drums"});
        }
    for (auto& p : s_.sampleLibrary()) {
        std::string nm = p.substr(p.find_last_of('/') + 1); nm = nm.substr(0, nm.rfind('.'));
        if (nm.rfind("sample_", 0) == 0) nm = nm.substr(7);
        lib_.push_back({nm, "User", p, false});
    }
    if (libCursor_ >= (int)lib_.size()) libCursor_ = 0;
}
void SamplerApp::browse(int d) {
    if (lib_.empty()) return;
    libCursor_ = ((libCursor_ + (d > 0 ? 1 : -1)) % (int)lib_.size() + (int)lib_.size()) % (int)lib_.size();
    auditionAt_ = nowMs_ + 250;
}
void SamplerApp::loadEntry(int i) {
    if (i < 0 || i >= (int)lib_.size()) return;
    libCursor_ = i; auditionAt_ = 0;
    const Entry& e = lib_[(size_t)i];
    TrackState& t = ts_[track_];
    t.entry = i; t.start = 0.f; t.end = 1.f;
    if (e.kit) t.algo = 3; else if (t.algo == 3) t.algo = 0;
    if (!pageAvailable(page_)) page_ = P_HOME;
    previewAfterLoad_ = true;
    s_.loadSamplePath(e.path, e.kit, track_);
}
void SamplerApp::importFile(const std::string& wav) {
    refreshLibrary();
    for (size_t i = 0; i < lib_.size(); i++) if (lib_[i].path == wav) { loadEntry((int)i); say("LOADED FROM DISK"); return; }
    say("FILE NOT READABLE");
}

// ---- parameters ----
void SamplerApp::applyAll(int t) {
    applyAlgo(t); applyRoot(t); applyEnv(t); applyLfo(t); applyHome(t); applyArp(t); applySeq(t); applyMix(t);
}
void SamplerApp::applyRoot(int t) { s_.setRoot(t, ts_[t].tuned ? ts_[t].root : 60.f); }
void SamplerApp::autoTune() {
    const int t = track_;
    if (ts_[t].algo > 1) { say("AUTOTUNE: MELODIC OR ONE-SHOT"); return; }
    if (ts_[t].tuned) { ts_[t].tuned = false; applyRoot(t); say("TUNING RESET"); return; }
    const float m = s_.detectRoot(t);
    if (m < 0) { say("NO CLEAR PITCH IN THIS SAMPLE"); return; }
    const int n = (int)std::lround(m), cents = (int)std::lround((m - n) * 100.f);
    ts_[t].root = m; ts_[t].tuned = true; applyRoot(t);
    say(std::string("TUNED: ") + noteName(n) + std::to_string(n / 12 - 1) + (cents >= 0 ? " +" : " ") + std::to_string(cents) + " CT");
}
void SamplerApp::applyAlgo(int t) { s_.setMode(t, ts_[t].algo); }
void SamplerApp::applyEnv(int t) {
    const TrackState& x = ts_[t];
    s_.setEnv(t, 3.f * x.a * x.a, x.d, x.s, 5.f * x.r * x.r);   // seconds (as shown on the screens), sustain level
}
void SamplerApp::applyLfo(int t) {
    const TrackState& x = ts_[t];
    Session::Lfo l; l.rate = lfoHz(x.lfoRate); l.depth = x.lfoDepth; l.target = x.lfoTarget; l.shape = x.lfoShape; l.base = 0.5f;
    s_.setLfo(t, l);   // the engine runs the low-pass only while the LFO sweeps the filter
}
void SamplerApp::applyHome(int t, bool commit) {
    const TrackState& x = ts_[t];
    s_.setSampleRange(t, x.start, x.end);
    const float semis = std::round((x.pitch - 0.5f) * 48.f);
    if (x.algo == 3) { s_.setStretch(t, 1.f, 0.f, commit); s_.setSliceThreshold(t, x.kitThr); s_.setKitPitch(t, semis); }   // KIT: threshold instead of speed, the pitch moves every slice
    else if (x.algo == 4) { s_.setStretch(t, 1.f, 0.f, commit); s_.setEchoParams(t, x.echo); s_.setEchoLatch(t,x.echoLatch!=0); }
    else if (x.algo == 5) { s_.setStretch(t, 1.f, 0.f, commit); s_.setPaul(t, paulFactor(x.paulStretch), x.paulWidth * 2.f); }
    else s_.setStretch(t, speedOf(x.speed), semis, commit);
}
void SamplerApp::applyArp(int t) {
    const TrackState& x = ts_[t];
    s_.setArp(t, x.arpRate > 0, std::max(0, x.arpRate - 1), x.arpPatt, x.arpOct + 1, x.arpLen);
}
void SamplerApp::applySeq(int) {   // loop speed and tape colour are global: every track gets them
    s_.setLoopSpeed(loopSpeedOf(loopSpeed_));
    for (int i = 0; i < kTracks; i++)
        s_.setTape(i, exciter_, 1.f - 0.4f * lofi_, wobble_, 0.25f * lofi_, cl(std::max(exciter_ * 3.f, std::max(wobble_, lofi_))), 1.f - 0.6f * lofi_);
}
void SamplerApp::applyMix(int t) {
    const TrackState& x = ts_[t];
    s_.setVolume(t, x.vol * 1.2f); s_.setPan(t, x.pan * 2.f - 1.f); s_.setSend(t, 0, x.sendR); s_.setSend(t, 1, x.sendD);
    s_.setSidechain(t, x.scSrc - 1, scThrDb(x.scThr), scRatioOf(x.scRatio));
}
void SamplerApp::applyFx() {
    s_.setEchoReturns(echoReturns_);
    s_.setReverb(reverbSize_);
    s_.setRetLevel(0, 0.8f); s_.setRetLevel(1, 0.7f);   // global send effects: how much of a track goes in is set per track (the sends)
    s_.setDelay(0.05f + delayTime_ * 1.45f, delayFb_ * 0.95f);
}
void SamplerApp::queueCommit(int t) { commitAt_ = nowMs_ + 350; commitTrack_ = t; }   // the offline stretch / pitch runs once the knob rests
void SamplerApp::setAlgo(int a) {
    motion_={};
    ts_[track_].algo = ((a % 6) + 6) % 6;
    if (!pageAvailable(page_)) page_ = P_HOME;
    applyAlgo(track_); applyHome(track_, true);
}
void SamplerApp::editEchoFx(int t) {
    const auto& x = ts_[t];
    s_.setEchoInsert(t, x.echoInsert);
    s_.setEchoSend(t, 0, x.echoDelay);
    s_.setEchoSend(t, 1, x.echoRoom);
    s_.setEchoReturns(echoReturns_);
    echoSaveAt_ = nowMs_ + 350;
}
void SamplerApp::editEcho(int t) {
    s_.setEchoParams(t, ts_[t].echo);
    s_.setEchoLatch(t, ts_[t].echoLatch != 0);
    echoSaveAt_ = nowMs_ + 350;
}
void SamplerApp::loadEcho() {
    const auto dir = s_.dataDir();
    if (dir.empty()) return;
    FILE* f = fopen((dir + "/echofield-params").c_str(), "r");
    if (!f) return;
    int version;
    if (fscanf(f, "%d", &version) == 1 && version == 1) {
        for (auto& t : ts_) {
            EchoParams p;
            if (fscanf(f, "%d %f %f %f %f %f", &p.mode, &p.position, &p.texture, &p.scatter, &p.drift, &p.evolve) != 6) break;
            if (p.mode >= 0 && p.mode < EchoParams::ModeCount &&
                std::isfinite(p.position) && std::isfinite(p.texture) && std::isfinite(p.scatter) &&
                std::isfinite(p.drift) && std::isfinite(p.evolve)) {
                p.position = cl(p.position); p.texture = cl(p.texture); p.scatter = cl(p.scatter);
                p.drift = cl(p.drift); p.evolve = cl(p.evolve); t.echo = p;
            }
        }
    }
    int fxVersion = 0;
    if (fscanf(f, "%d", &fxVersion) == 1 && fxVersion == 1) {
        for (auto& t : ts_) {
            EchoInsertParams p; float d, r;
            if (fscanf(f, "%d %f %f %f %f %f %f %f %f %f %f %f %f %f",
                    &p.filter,&p.cutoff,&p.resonance,&p.drive,&p.filterMix,&p.tape,&p.wow,&p.flutter,&p.age,&p.hiss,&p.width,&p.level,&d,&r) != 14) break;
            float* fields[] = {&p.cutoff,&p.resonance,&p.drive,&p.filterMix,&p.tape,&p.wow,&p.flutter,&p.age,&p.hiss,&p.width,&p.level,&d,&r};
            bool ok = p.filter >= 0 && p.filter <= 2;
            for (auto v : fields) { ok &= std::isfinite(*v); *v = cl(*v); }
            if (ok) { t.echoInsert=p; t.echoDelay=d; t.echoRoom=r; }
        }
        EchoReturnParams p;
        if (fscanf(f, "%f %f %f %f %f %d",&p.time,&p.feedback,&p.size,&p.damp,&p.shimmer,&p.ping)==6 &&
                std::isfinite(p.time)&&std::isfinite(p.feedback)&&std::isfinite(p.size)&&std::isfinite(p.damp)&&std::isfinite(p.shimmer)) {
            p.time=cl(p.time,.02f,1.f);p.feedback=cl(p.feedback,0,.92f);p.size=cl(p.size);p.damp=cl(p.damp);p.shimmer=cl(p.shimmer);p.ping=p.ping!=0;
            echoReturns_=p;
        }
    }
    int grainVersion=0;
    if(fscanf(f,"%d",&grainVersion)==1 && grainVersion==1) for(auto& t:ts_){
        int window;float size,density,scan,reverse,pitch;
        if(fscanf(f,"%d %f %f %f %f %f",&window,&size,&density,&scan,&reverse,&pitch)!=6) break;
        if(window>=0&&window<=5&&std::isfinite(size)&&std::isfinite(density)&&std::isfinite(scan)&&std::isfinite(reverse)&&std::isfinite(pitch)){
            t.echo.window=window;t.echo.size=cl(size);t.echo.density=cl(density);t.echo.scan=cl(scan);t.echo.reverse=cl(reverse);t.echo.pitchRandom=cl(pitch);
        }
    }
    fclose(f);
}
void SamplerApp::saveEcho() {
    const auto dir = s_.dataDir();
    if (dir.empty()) return;
    const std::string path = dir + "/echofield-params", tmp = path + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) { say("ECHOFIELD SETTINGS NOT SAVED"); return; }
    bool ok = fprintf(f, "1\n") > 0;
    for (const auto& t : ts_) {
        const auto& p = t.echo;
        ok &= fprintf(f, "%d %.9g %.9g %.9g %.9g %.9g\n", p.mode, p.position, p.texture, p.scatter, p.drift, p.evolve) > 0;
    }
    ok &= fprintf(f,"1\n")>0;
    for (const auto& t : ts_) {
        const auto& p=t.echoInsert;
        ok &= fprintf(f,"%d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
            p.filter,p.cutoff,p.resonance,p.drive,p.filterMix,p.tape,p.wow,p.flutter,p.age,p.hiss,p.width,p.level,t.echoDelay,t.echoRoom)>0;
    }
    const auto& p=echoReturns_;
    ok &= fprintf(f,"%.9g %.9g %.9g %.9g %.9g %d\n",p.time,p.feedback,p.size,p.damp,p.shimmer,p.ping)>0;
    ok &= fprintf(f,"1\n")>0;
    for(const auto& t:ts_) {const auto& g=t.echo;ok &= fprintf(f,"%d %.9g %.9g %.9g %.9g %.9g\n",g.window,g.size,g.density,g.scan,g.reverse,g.pitchRandom)>0;}
    const int closed = fclose(f);
    if (!ok || closed || rename(tmp.c_str(), path.c_str())) {
        remove(tmp.c_str()); say("ECHOFIELD SETTINGS NOT SAVED");
    }
}
void SamplerApp::setVolume(int t, float v) { ts_[t].vol = cl(v); applyMix(t); }
void SamplerApp::setPan01(int t, float v) { ts_[t].pan = cl(v); applyMix(t); }
void SamplerApp::setSend(int t, int w, float v) { (w ? ts_[t].sendD : ts_[t].sendR) = cl(v); applyMix(t); }
void SamplerApp::setScale(int scale, int root, int octave, int mode) {
    scale_ = {std::min(7, std::max(0, scale)), ((root % 12) + 12) % 12, std::min(3, std::max(-3, octave)), mode & 1};
    sysex(6, {7, (uint8_t)scale_.root, (uint8_t)(scale_.octave + 3), (uint8_t)scale_.mode}, true);
}
void SamplerApp::selectTrack(int t) {
    t = std::min(kTracks - 1, std::max(0, t));
    if (t == track_) return;
    motion_={};
    if (st_.track_clip_rec[track_]) s_.clipRecord(track_, false);   // an overdub belongs to the track it was started on
    track_ = t;
    if (!pageAvailable(page_)) page_ = P_HOME;
    if (ts_[t].entry >= 0) libCursor_ = ts_[t].entry;   // PRESETS starts from the sample this track plays
}
void SamplerApp::gotoPage(Page p) { if (p >= P_HOME && p < kPages && pageAvailable(p)) { if(page_!=p)motion_={}; page_ = p; } }

SamplerApp::Knob SamplerApp::cont(const char* name, float* v, std::function<std::string(float)> f, std::function<void()> apply, float stepSize, bool commit) {
    Knob k; k.name = name; k.valid = true; k.norm = *v; k.value = f(*v);
    const int t = track_;
    k.step = [=](int d) { *v = cl(*v + d * stepSize); apply(); if (commit) queueCommit(t); };
    k.set = [=](float n) { *v = cl(n); apply(); if (commit) queueCommit(t); };
    return k;
}
SamplerApp::Knob SamplerApp::choice(const char* name, int* v, std::vector<std::string> opts, std::function<void()> apply) {
    Knob k; k.name = name; k.valid = true; k.isChoice = true;
    const int n = (int)opts.size();
    *v = std::min(n - 1, std::max(0, *v));
    k.value = opts[(size_t)*v]; k.norm = n > 1 ? *v / (float)(n - 1) : 0.f;
    k.step = [=](int d) { *v = std::min(n - 1, std::max(0, *v + (d > 0 ? 1 : -1))); apply(); };
    k.set = [=](float x) { *v = std::min(n - 1, std::max(0, (int)std::lround(x * (n - 1)))); apply(); };
    return k;
}

SamplerApp::Knobs SamplerApp::knobs() { return knobs(page_); }
SamplerApp::Knobs SamplerApp::knobs(Page p) {
    Knobs k;
    TrackState& x = ts_[track_];
    const int t = track_;
    auto sec = [](float s) { return fmt(s < 1 ? "%.2fS" : "%.1fS", s); };
    switch (p) {
    case P_HOME:
        k[0] = cont("START", &x.start, [](float v) { return pct(v); }, [=] { if (ts_[t].start > ts_[t].end - 0.02f) ts_[t].start = ts_[t].end - 0.02f; applyHome(t); }, 0.01f, true);
        k[1] = cont("END", &x.end, [](float v) { return pct(v); }, [=] { if (ts_[t].end < ts_[t].start + 0.02f) ts_[t].end = ts_[t].start + 0.02f; applyHome(t); }, 0.01f, true);
        if (x.algo == 3) {   // KIT: the threshold the sample is cut at, the pitch of every slice
            k[2] = cont("THRESHOLD", &x.kitThr, [](float v) { return pct(v); }, [=] { applyHome(t); }, 0.01f);
            k[3] = cont("PITCH", &x.pitch, [](float v) { return fmt("%+.0f ST", std::round((v - 0.5f) * 48.f)); }, [=] { applyHome(t); }, 1.f / 48.f);
        } else if (x.algo == 4) {
            k[2] = choice("MODE", &x.echo.mode, std::vector<std::string>(kEchoMode, kEchoMode + EchoParams::ModeCount), [=] { editEcho(t); });
            k[3] = cont("POSITION", &x.echo.position, [](float v) { return fmt("%.1f%%", v * 100); }, [=] { editEcho(t); }, 0.005f);
        } else if (x.algo == 5) {   // PAULSTRETCH: how much slower, how wide
            k[2] = cont("STRETCH", &x.paulStretch, [](float v) { return fmt("%.0fX", paulFactor(v)); }, [=] { applyHome(t); }, 0.02f);
            k[3] = cont("WIDTH", &x.paulWidth, [](float v) { return pct(v * 2.f); }, [=] { applyHome(t); }, 0.02f);
        } else {
            k[2] = cont("SPEED", &x.speed, [](float v) { return fmt("%.2fX", speedOf(v)); }, [=] { applyHome(t); }, 0.01f, true);
            k[3] = cont("PITCH", &x.pitch, [](float v) { return fmt("%+.0f ST", std::round((v - 0.5f) * 48.f)); }, [=] { applyHome(t); }, 1.f / 48.f, true);
        }
        break;
    case P_ECHO:
        if (x.algo != 4) break;
        k[0] = cont("TEXTURE", &x.echo.texture, [](float v) { return pct(v); }, [=] { editEcho(t); }, 0.01f);
        k[1] = cont("SCATTER", &x.echo.scatter, [](float v) { return pct(v); }, [=] { editEcho(t); }, 0.01f);
        k[2] = cont("DRIFT", &x.echo.drift, [](float v) { return pct(v); }, [=] { editEcho(t); }, 0.01f);
        k[3] = cont("EVOLVE", &x.echo.evolve, [](float v) { return pct(v); }, [=] { editEcho(t); }, 0.01f);
        break;
    case P_GRAIN:
        if(x.algo!=4) break;
        k[0]=cont("SIZE",&x.echo.size,[](float v){return fmt("%.2fX",std::exp2(4*v-2));},[=]{editEcho(t);});
        k[1]=cont("DENSITY",&x.echo.density,[](float v){return fmt("%.2fX",std::exp2(4*v-2));},[=]{editEcho(t);});
        k[2]=choice("WINDOW",&x.echo.window,{"AUTO","HANN","TUKEY","PERC","SWELL","GAUSS"},[=]{editEcho(t);});
        k[3]=choice("FREEZE",&x.echo.freeze,{"OFF","ON"},[=]{editEcho(t);});
        break;
    case P_MOTION:
        if(x.algo!=4) break;
        k[0]=cont("SCAN",&x.echo.scan,[](float v){return fmt("%+.0f%%",200*v-100);},[=]{editEcho(t);},.01f);
        k[1]=cont("REVERSE",&x.echo.reverse,[](float v){return pct(v);},[=]{editEcho(t);});
        k[2]=cont("PITCH RND",&x.echo.pitchRandom,[](float v){return fmt("%.1f ST",12*v);},[=]{editEcho(t);});
        k[3]=choice("LATCH",&x.echoLatch,{"OFF","ON"},[=]{editEcho(t);});
        break;
    case P_EFILTER:
        k[0] = choice("FILTER", &x.echoInsert.filter, {"LOW PASS","BAND PASS","HIGH PASS"}, [=, this] { editEchoFx(t); });
        k[1] = cont("CUTOFF", &x.echoInsert.cutoff, [](float v) { return fmt("%.0f HZ", 30*std::pow(600.f,v)); }, [=, this] { editEchoFx(t); }, .01f);
        k[2] = cont("RESONANCE", &x.echoInsert.resonance, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[3] = cont("DRIVE", &x.echoInsert.drive, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        break;
    case P_EDELAY:
        k[0] = cont("TIME", &echoReturns_.time, [](float v) { return fmt("%.0f MS", std::max(.02f,v)*1000); }, [=, this] { editEchoFx(t); }, .01f);
        k[1] = cont("FEEDBACK", &echoReturns_.feedback, [](float v) { return pct(std::min(.92f,v)); }, [=, this] { editEchoFx(t); }, .01f);
        k[2] = cont("ECHO SEND", &x.echoDelay, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[3] = choice("PINGPONG", &echoReturns_.ping, {"OFF","ON"}, [=, this] { editEchoFx(t); });
        break;
    case P_EREVERB:
        k[0] = cont("ROOM SEND", &x.echoRoom, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[1] = cont("SIZE", &echoReturns_.size, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[2] = cont("DAMP", &echoReturns_.damp, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[3] = cont("SHIMMER", &echoReturns_.shimmer, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        break;
    case P_ETAPE:
        k[0] = cont("WOW", &x.echoInsert.wow, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[1] = cont("FLUTTER", &x.echoInsert.flutter, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[2] = cont("AGE", &x.echoInsert.age, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[3] = cont("HISS", &x.echoInsert.hiss, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        break;
    case P_EMIX:
        k[0] = cont("WIDTH", &x.echoInsert.width, [](float v) { return pct(.4f+.8f*v); }, [=, this] { editEchoFx(t); });
        k[1] = cont("LEVEL", &x.echoInsert.level, [](float v) { return pct(1.6f*v*v); }, [=, this] { editEchoFx(t); });
        k[2] = cont("TAPE MIX", &x.echoInsert.tape, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        k[3] = cont("FILTER MIX", &x.echoInsert.filterMix, [](float v) { return pct(v); }, [=, this] { editEchoFx(t); });
        break;
    case P_FX:   // two global send effects; the sends are per track (the selected one), size and time are global
        k[0] = cont("REVERB SEND", &x.sendR, [](float v) { return pct(v); }, [=] { applyMix(t); });
        k[1] = cont("REVERB SIZE", &reverbSize_, [](float v) { return pct(v); }, [=] { applyFx(); });
        k[2] = cont("DELAY SEND", &x.sendD, [](float v) { return pct(v); }, [=] { applyMix(t); });
        k[3] = cont("DELAY TIME", &delayTime_, [](float v) { return fmt("%.2fS", 0.05 + v * 1.45); }, [=] { applyFx(); });
        break;
    case P_ENV:
        k[0] = cont("ATTACK", &x.a, [=](float v) { return sec(3 * v * v); }, [=] { applyEnv(t); });
        k[1] = cont("DECAY", &x.d, [=](float v) { return sec(v); }, [=] { applyEnv(t); });
        k[2] = cont("SUSTAIN", &x.s, [](float v) { return pct(v); }, [=] { applyEnv(t); });
        k[3] = cont("RELEASE", &x.r, [=](float v) { return sec(5 * v * v); }, [=] { applyEnv(t); });
        break;
    case P_LFO:
        k[0] = cont("RATE", &x.lfoRate, [](float v) { return fmt("%.1fHZ", lfoHz(v)); }, [=] { applyLfo(t); });
        k[1] = cont("DEPTH", &x.lfoDepth, [](float v) { return pct(v); }, [=] { applyLfo(t); });
        k[2] = choice("TARGET", &x.lfoTarget, {kLfoTarget[0], kLfoTarget[1], kLfoTarget[2], kLfoTarget[3]}, [=] { applyLfo(t); });
        k[3] = choice("SHAPE", &x.lfoShape, {kLfoShape[0], kLfoShape[1], kLfoShape[2], kLfoShape[3]}, [=] { applyLfo(t); });
        break;
    case P_ARP: {
        std::vector<std::string> rates(kArpRate, kArpRate + 13), patt(kArpPatt, kArpPatt + 5);
        k[0] = choice("RATE", &x.arpRate, rates, [=] { applyArp(t); });
        k[1] = choice("PATTERN", &x.arpPatt, patt, [=] { applyArp(t); });
        k[2] = choice("OCTAVES", &x.arpOct, {"1", "2", "3", "4"}, [=] { applyArp(t); });
        k[3] = cont("LENGTH", &x.arpLen, [](float v) { return pct(v); }, [=] { applyArp(t); });
        break;
    }
    case P_SEQ:
        k[0] = cont("LOOP SPEED", &loopSpeed_, [](float v) { return fmt("%.2fX", loopSpeedOf(v)); }, [=] { applySeq(t); }, 0.02f);
        k[1] = cont("EXCITER", &exciter_, [](float v) { return pct(v); }, [=] { applySeq(t); });
        k[2] = cont("LO-FI", &lofi_, [](float v) { return pct(v); }, [=] { applySeq(t); });
        k[3] = cont("WOBBLE", &wobble_, [](float v) { return pct(v); }, [=] { applySeq(t); });
        break;
    case P_MIX: {
        k[0] = cont("VOLUME", &x.vol, [](float v) { return pct(v); }, [=] { applyMix(t); });
        std::vector<std::string> src = {"OFF"}; for (int i = 1; i <= kTracks; i++) src.push_back("TRACK " + std::to_string(i));
        k[1] = choice("SC SOURCE", &x.scSrc, src, [=] { applyMix(t); });
        k[2] = cont("SC THRESHOLD", &x.scThr, [](float v) { return fmt("%.0f DB", scThrDb(v)); }, [=] { applyMix(t); });
        k[3] = cont("SC RATIO", &x.scRatio, [](float v) { return fmt("%.1f:1", scRatioOf(v)); }, [=] { applyMix(t); });
        break;
    }
    case P_KEY: {
        auto mk = [this](const char* name, std::vector<std::string> opts, int cur, std::function<void(int)> setter) {
            Knob b; b.name = name; b.valid = true; b.isChoice = true;
            const int n = (int)opts.size(); cur = std::min(n - 1, std::max(0, cur));
            b.value = opts[(size_t)cur]; b.norm = cur / (float)(n - 1);
            b.step = [=](int d) { setter(std::min(n - 1, std::max(0, cur + (d > 0 ? 1 : -1)))); };
            b.set = [=](float v) { setter(std::min(n - 1, std::max(0, (int)std::lround(v * (n - 1))))); };
            return b;
        };
        const ScaleState sc = scale_;
        k[0] = mk("SCALE", std::vector<std::string>(kScaleName, kScaleName + 8), sc.scale, [=](int v) { setScale(v, sc.root, sc.octave, sc.mode); });
        k[1] = mk("ROOT", std::vector<std::string>(kNote, kNote + 12), sc.root, [=](int v) { setScale(sc.scale, v, sc.octave, sc.mode); });
        k[2] = mk("OCTAVE", {"-3", "-2", "-1", "0", "+1", "+2", "+3"}, sc.octave + 3, [=](int v) { setScale(sc.scale, sc.root, v - 3, sc.mode); });
        k[3] = mk("MODE", {"KEY", "CHORD"}, sc.mode, [=](int v) { setScale(sc.scale, sc.root, sc.octave, v); });
        break;
    }
    case P_SAVE: {
        static const char* micTxt[] = {"READY", "WAITING", "RECORDING", "SAVING"};
        Knob m; m.name = "MIC SAMPLE"; m.valid = true; m.isChoice = true; m.value = micTxt[mic_]; m.norm = mic_ == 0 ? 0.f : 1.f;
        m.step = [this](int d) { if ((d > 0) == (mic_ == 0)) micSample(); };
        m.set = [this](float v) { if ((v > 0.5f) == (mic_ == 0)) micSample(); };
        k[0] = m;
        k[1] = choice("LOOP SOURCE", &loopSrc_, {"MIC", "SOUND"}, [this] { s_.setLoopSource(loopSrc_); });
        k[3] = cont("MIC THRESHOLD", &micThr_, [](float v) { return fmt("%.0f DB", 20 * std::log10(micThrOf(v))); }, [this] { s_.setMicThreshold(micThrOf(micThr_)); }, 0.02f);
        break;
    }
    case P_TEMPO: {
        if (st_.loopSamples <= 0 || recordingNow()) break;
        Knob bars; bars.name = "BARS"; bars.valid = true; bars.isChoice = true;
        bars.value = std::to_string(st_.loopBars); bars.norm = (st_.loopBars - 1) / 31.f;
        bars.step = [this](int d) { s_.setLoopBars(d, true); };
        bars.set = [this](float v) { s_.setLoopBars(1 + (int)std::lround(cl(v) * 31)); };
        k[0] = bars;
        Knob bpm; bpm.name = "BPM"; bpm.valid = true;
        bpm.value = fmt("%.0f", st_.bpm); bpm.norm = cl((st_.bpm - 30.f) / 270.f);
        bpm.step = [this](int d) { s_.setLoopBpm((float)d, true); };
        bpm.set = [this](float v) { s_.setLoopBpm(std::round(30.f + cl(v) * 270.f)); };
        k[1] = bpm;
        Knob mode; mode.name = "MODE"; mode.valid = true; mode.isChoice = true;
        mode.value = st_.tempoStretch ? "STRETCH" : "TAPE"; mode.norm = st_.tempoStretch ? 1.f : 0.f;
        mode.step = [this](int d) { s_.setTempoStretch(d > 0); };
        mode.set = [this](float v) { s_.setTempoStretch(v >= 0.5f); };
        k[2] = mode;
        break;
    }
    case P_LEN: {
        const double sec = st_.loopSamples > 0 ? st_.loopSamples / (double)s_.sampleRate() : 0.0;
        const float cur = st_.loopSamples > 0 ? cl((float)(std::log2(std::max(1.0, (double)st_.loopBars)) / 5.0)) : 0.f;
        Knob len; len.name = "LENGTH"; len.valid = true; len.isChoice = true;
        len.value = st_.loopSamples > 0 ? fmt("%.1fS", sec) : "NO LOOP"; len.norm = cur;
        len.step = [this](int d) { lenAction(d > 0); };
        len.set = [this, cur](float v) { if (v > cur + 0.05f) lenAction(true); else if (v < cur - 0.05f) lenAction(false); };
        auto act = [this](const char* name, const char* what, bool dbl) {
            Knob a; a.name = name; a.valid = true; a.isChoice = true; a.value = st_.loopSamples > 0 ? what : "NO LOOP";
            a.step = [this, dbl](int) { lenAction(dbl); }; a.set = [this, dbl](float) { lenAction(dbl); };
            return a;
        };
        k[0] = len;
        k[1] = act("DUPLICATE", "TURN: X2", true);
        k[2] = act("HALVE", "TURN: /2", false);
        Knob b; b.name = "BARS / BPM"; b.valid = true; b.isChoice = true; b.value = fmt("%.0f", st_.loopBars) + " / " + fmt("%.0f", st_.bpm); b.step = [](int) {}; b.set = [](float) {};
        k[3] = b;
        break;
    }
    default: break;
    }
    return k;
}

// ---- looper, recorder ----
void SamplerApp::endRecording() {
    if (st_.freeArmed || st_.freeRecording) { s_.freeRecord(track_, false); say(st_.freeArmed && !st_.freeRecording ? "CANCELLED" : "LOOP SET"); }
    for (int t = 0; t < kTracks; t++) if (st_.track_clip_rec[t]) s_.clipRecord(t, false);
}
void SamplerApp::transportPlay() {   // while recording the first press ends the recording (the loop plays on), the next one stops the tape
    if (recordingNow()) { endRecording(); return; }
    s_.play(!st_.playing, editor_);
}
void SamplerApp::transportSet(bool play) {
    if (recordingNow()) { if (!play) endRecording(); return; }
    s_.play(play, editor_);
}
void SamplerApp::loopRecord() {
    if (st_.freeArmed || st_.freeRecording) { s_.freeRecord(track_, false); say("LOOP SET"); return; }
    if (!loopAny_ && !st_.track_clip_rec[track_]) {   // a clean looper: arm the first take, it starts with the first note (or sound) and sets the loop length
        s_.freeRecord(track_, true);
        say(loopSrc_ ? "PLAY A KEY TO START" : "WAITING FOR SOUND");
        return;
    }
    if (!st_.track_clip_rec[track_] && st_.tempoStretch && std::fabs(st_.tempoRate - 1.0) > 1e-5) {
        say("USE TAPE MODE TO OVERDUB"); return;
    }
    if (!st_.playing) s_.play(true);
    s_.clipRecord(track_, !st_.track_clip_rec[track_]);
    say(st_.track_clip_rec[track_] ? "OVERDUB OFF" : "OVERDUB");
}
void SamplerApp::lenAction(bool dbl) {   // loop x2 (the content repeats) or /2; guarded so a dragged slider does not double it again and again
    if (st_.loopSamples <= 0 || nowMs_ - lenAt_ < 300) return;
    lenAt_ = nowMs_ ? nowMs_ : 1;
    if (dbl) { s_.loopDouble(); say("LOOP DOUBLED"); } else { s_.loopHalve(); say("LOOP HALVED"); }
}
void SamplerApp::loopSave() {
    if (!s_.saveClip(track_)) { say("NO LOOP TO SAVE"); return; }
    say("LOOP SAVED TO LIBRARY");
    previewAfterLoad_ = false;
    auditionAt_ = 0;
    commitAt_ = nowMs_ + 1;   // refresh the library shortly, once the file is written
    commitTrack_ = -2;
}
void SamplerApp::loopClearTrack() {
    if (!st_.track_has_clip[track_]) { say("NO LOOP ON THIS TRACK"); return; }
    s_.clipClear(track_);
    say("LOOP OF TRACK " + std::to_string(track_ + 1) + " CLEARED");
}
void SamplerApp::loopClear() {   // PROJECT menu: every loop of every track, so the looper is clean again
    s_.clipClearAll();
    say("ALL LOOPS CLEARED");
}
void SamplerApp::micSample() {
    if (mic_ == 0) {
        s_.recorderWait(true); s_.recorderStart();
        mic_ = 1; micTrack_ = track_; say("SAMPLING: MAKE A SOUND");
    } else if (mic_ == 1) { s_.recorderStop(); mic_ = 0; say("SAMPLING CANCELLED"); }
    else if (mic_ == 2) { s_.recorderStop(); mic_ = 3; say("SAVING"); }
}
std::string SamplerApp::loopText() const {
    if (st_.freeArmed) return loopSrc_ ? "PLAY A KEY TO START" : "WAITING FOR SOUND";
    if (st_.freeRecording) return fmt("RECORDING %.1fS", st_.freeSeconds);
    if (st_.track_clip_rec[track_]) return "OVERDUB";
    if (!loopAny_) return "LOOP EMPTY";
    return fmt("LOOP %.0fBPM ", st_.bpm) + std::to_string(st_.loopBars) + (st_.loopBars == 1 ? " BAR" : " BARS");
}
void SamplerApp::keyEvent(int raw, bool down) {   // a key of the FM-1 (chromatic note 53+key): octave shift, chords in CHORD mode
    if (raw < 0 || raw > 127) return;
    if (down) {
        if (keyCount_[raw]) keyEvent(raw, false);
        const bool kit = ts_[track_].algo == 3;   // KIT: every key is a pad (note 60 + key), whatever the tonality or octave
        const int base = kit ? 60 + (raw - 53) : raw + 12 * scale_.octave;
        int cnt = 0; keyNotes_[raw][cnt++] = base;
        if (!kit && scale_.scale < 7 && scale_.mode) {
            const int rel = base - scale_.root, o = rel >= 0 ? rel / 12 : -((-rel + 11) / 12), i = scaleIndex(rel - 12 * o, scale_.scale);
            if (i >= 0) { const int d = o * kScaleN[scale_.scale] + i; keyNotes_[raw][cnt++] = scale_.root + degreeNote(d + 2, scale_.scale, 0); keyNotes_[raw][cnt++] = scale_.root + degreeNote(d + 4, scale_.scale, 0); }
        }
        keyCount_[raw] = cnt;
        for (int j = 0; j < cnt; j++) playNote(keyNotes_[raw][j], true);
    } else {
        for (int j = 0; j < keyCount_[raw]; j++) playNote(keyNotes_[raw][j], false);
        keyCount_[raw] = 0;
    }
}
void SamplerApp::playNote(int note, bool down) {
    if (note < 0 || note > 127) return;
    if (down) { heldNote_[note] = track_; s_.noteOn(track_, note, 100); }
    else if (heldNote_[note] >= 0) { s_.noteOff(heldNote_[note], note); heldNote_[note] = -1; }
}
std::vector<float> SamplerApp::peaks(int t, int bins) {
    const std::string lab = s_.trackLabel(t);
    if (peaksLabel_[t] != lab || nowMs_ - peaksAt_[t] > 700 || (int)peaks_[t].size() != bins) {
        peaksLabel_[t] = lab; peaksAt_[t] = nowMs_; peaks_[t] = s_.samplePeaksOf(t, bins);
    }
    return peaks_[t];
}

// ---- input ----
void SamplerApp::onMidi(const uint8_t* m, size_t n) {
    if (n < 3) return;
    linkSeen_ = true;
    const int type = m[0] & 0xf0, ch = m[0] & 15, a = m[1], b = m[2];
    if (ch == 0 && (type == 0x90 || type == 0x80)) keyEvent(a, type == 0x90 && b > 0);
    else if (ch == 1 && (type == 0x90 || type == 0x80)) button(a, type == 0x90 && b > 0);
    else if (ch == 0 && type == 0xb0) {
        if (a >= 16 && a < 23) encoder(a - 16, b - 64);
        else if (a == 7) knobMsb_ = b;
        else if (a == 39) s_.setMasterVolume(((knobMsb_ << 3) | (b >> 4)) / 1023.f * 1.2f);
    }
}
void SamplerApp::onSysex(const uint8_t* m, size_t n) {
    if (n < 8 || m[1] != 0x7d || m[2] != 'D' || m[3] != 'A' || m[4] != 'W') return;
    linkSeen_ = true;
    if (m[6] == 0x41 && n >= 12) {   // the firmware's own SEL screen / OCT buttons: its scale index is the tonality unless it is the OFF we keep it on
        if ((m[7] & 7) != 7) { scale_.scale = m[7] & 7; fwDirty_ = true; }
        scale_.root = m[8] % 12; scale_.octave = (int)m[9] - 3; scale_.mode = m[10] & 1;
    }
    else if (m[6] == 0x40 && n >= 30) {   // PONG: frames the FM-1 had to drop
        uint32_t d = 0; for (int i = 0; i < 5; i++) d = d << 7 | (m[24 + i] & 127);
        if (haveDropBase_ && d != dropped_) { gap_ = std::min<uint32_t>(250, gap_ * 16 / 10 + 1); lastLoss_ = nowMs_; needClear_ = true; pending_.clear(); pendingBytes_ = 0; }
        if (!haveDropBase_) { needClear_ = true; repaintAt_ = nowMs_ + 1500; }   // first contact: whatever the firmware drew (its splash) goes
        if (!haveDropBase_) fwDirty_ = true;
        dropped_ = d; haveDropBase_ = true;
    }
    else if (m[6] == 0x42) { needClear_ = fwDirty_ = true; }
    else if (m[6] == 0x43) resetSurface(); // firmware resumed after a link interruption
}
void SamplerApp::setLight(bool on) {
    theme::apply(on);
    FILE* f = fopen((s_.dataDir() + "/theme").c_str(), "w");
    if (f) { fputc(on ? 'l' : 'd', f); fclose(f); }
}
void SamplerApp::button(int b, bool down) {
    if (b < 0 || b > REC) return;
    const bool wasHeld = (held_ & (1u << b)) != 0;
    lastLed_ = ~0ull;   // the firmware's own LED handling must not win: the active page stays lit, the others go dark
    if (down) { held_ |= 1u << b; if(!wasHeld) motion_={}; } else held_ &= ~(1u << b);
    if (b == HOME) {
        if (down && !wasHeld) { homeAt_ = nowMs_; homeLong_ = false; }
        else if (!down && wasHeld && !homeLong_) page_ = algo()!=4 ? P_HOME : page_==P_HOME ? P_ECHO : page_==P_ECHO ? P_GRAIN : page_==P_GRAIN ? P_MOTION : P_HOME;
        return;
    }
    if (b == GLO) {
        if (down && !wasHeld) { gloAt_ = nowMs_; gloLong_ = false; }
        else if (!down && wasHeld && !gloLong_) page_ = P_KEY;
        return;
    }
    if (b == SAVE) {   // only the LOOP page: saving and clearing loops lives in the phone's PROJECT menu
        if (down) page_ = P_SAVE;
        return;
    }
    if (!down) return;
    switch (b) {
    case FX: if (!wasHeld) page_ = page_ == P_FX ? P_EFILTER : page_ >= P_EFILTER && page_ < P_EREVERB ? static_cast<Page>(page_ + 1) : P_FX; break;
    case ENV: page_ = P_ENV; break;
    case LFO: page_ = P_LFO; break;
    case ARP: page_ = P_ARP; break;
    case SEQ: if (!wasHeld) page_ = page_ == P_SEQ ? P_ETAPE : page_ == P_ETAPE ? P_EMIX : P_SEQ; break;
    case EDIT: page_ = P_MIX; break;
    case SEL: if (!wasHeld) page_ = page_ == P_LEN ? P_TEMPO : P_LEN; break;
    case PLAY: transportPlay(); break;
    case REC: loopRecord(); break;
    default: break;
    }
}
SamplerApp::~SamplerApp() = default;

void SamplerApp::encoder(int i, int delta) {
    if (!delta || i < 0 || i > 6) return;
    if (i == 0) { selectTrack(track_ + (delta > 0 ? 1 : -1)); return; }
    if (i == 6) { browse(delta); return; }
    if (i == 1) { setAlgo(ts_[track_].algo + (delta > 0 ? 1 : -1)); return; }
    if (i == 2 && mic_ == 1 && page_ != P_SAVE) { setMicThreshold01(micThr_ + (delta > 0 ? 0.03f : -0.03f)); touchedKnob_ = 0; touchedAt_ = nowMs_; return; }   // K1 = trigger level while the microphone listens
    Knobs k = knobs();
    const int idx = i - 2;
    if (idx < 4 && k[(size_t)idx].valid && k[(size_t)idx].step) {
        auto& m=motion_[idx];
        const int sign=delta>0?1:-1;
        const uint32_t elapsed=nowMs_-m.at;
        const bool same=m.sign==sign&&m.track==track_&&m.page==page_&&elapsed<=120;
        // A burst of single detents ramps up gradually. Preserve
        // coalesced MIDI deltas, and never accelerate enum/action knobs.
        m.streak=same?std::min(12,m.streak+1):0;
        m.at=nowMs_;m.sign=sign;m.track=track_;m.page=page_;
        if(!k[idx].isChoice&&std::abs(delta)<=4&&same&&elapsed<=65) {
            const int multiplier=m.streak>=6?5:m.streak>=3?3:m.streak>=1?2:1;
            delta*=multiplier;
        }
        k[idx].step(delta); touchedKnob_=idx; touchedAt_=nowMs_;
    }
}

// ---- tick ----
void SamplerApp::releaseSurfaceKeys() {
    for (int raw = 0; raw < 128; ++raw) if (keyCount_[raw]) keyEvent(raw, false);
    held_ = 0; homeLong_ = gloLong_ = false;
}
void SamplerApp::resetSurface() {
    releaseSurfaceKeys();
    motion_={};
    pending_.clear(); pendingBytes_ = 0;
    haveDropBase_ = false; dropped_ = 0; linkSeen_ = false;
    gap_ = 50; lastLoss_ = lastRaise_ = nowMs_;
    lastPing_ = lastSend_ = nowMs_;
    repaintAt_ = 0;
    needClear_ = fwDirty_ = true;
    lastLed_ = ~0ull;
    held_ = 0; homeLong_ = gloLong_ = false;
}
void SamplerApp::tick(uint32_t now) {
    nowMs_ = now;
    if (echoSaveAt_ && now >= echoSaveAt_) { echoSaveAt_ = 0; saveEcho(); }
    if ((held_ & (1u << HOME)) && !homeLong_ && now - homeAt_ >= 600) {
        editor_ = !editor_; homeLong_ = true;
    }
    if ((held_ & (1u << GLO)) && !gloLong_ && now - gloAt_ >= 600) {
        setLight(!theme::light); gloLong_ = true;
    }
    if (fwDirty_) { fwDirty_ = false; sysex(6, {7, (uint8_t)scale_.root, (uint8_t)(scale_.octave + 3), (uint8_t)scale_.mode}, true); }
    if (repaintAt_ && now >= repaintAt_) { repaintAt_ = 0; needClear_ = true; }
    st_ = s_.status();
    loopAny_ = false;
    for (int t = 0; t < kTracks; t++) loopAny_ |= st_.track_has_clip[t];
    if (now - lastLoss_ > 4000 && now - lastRaise_ > 2000 && gap_ > 16) { gap_ = std::max<uint32_t>(16, gap_ * 9 / 10); lastRaise_ = now; }
    if (now - lastPing_ >= 500) { lastPing_ = now; pingId_ = (pingId_ + 1) & 0x3fff; sysex(5, n14(pingId_), true, 901); }
    // audition: the cursor rested, load the entry on the selected track and play it
    if (auditionAt_ && now >= auditionAt_) loadEntry(libCursor_);
    if (st_.sampleVersion != lastSampleVersion_) {   // a sample landed on a track: give it this track's settings
        lastSampleVersion_ = st_.sampleVersion;
        if (st_.sampleTrack >= 0 && st_.sampleTrack < kTracks) ts_[st_.sampleTrack].tuned = false;   // a new sample: its pitch is unknown again
        for (int i = 0; i < kTracks; i++) if (hasSample(i)) applyAll(i);   // several samples can land between two ticks (start-up): every track gets its settings, not only the last
        if (previewAfterLoad_ && st_.sampleTrack == track_) { previewAfterLoad_ = false; previewNote_ = 60; previewTrack_ = track_; s_.noteOn(track_, 60, 100); previewOffAt_ = now + 900; }
    }
    if (st_.sampleFails != lastSampleFails_) { lastSampleFails_ = st_.sampleFails; say("COULD NOT LOAD THE FILE"); }
    if (previewNote_ >= 0 && now >= previewOffAt_) { s_.noteOff(previewTrack_, previewNote_); previewNote_ = -1; }
    if (commitAt_ && now >= commitAt_) {
        commitAt_ = 0;
        if (commitTrack_ == -2) refreshLibrary();
        else if (commitTrack_ >= 0) applyHome(commitTrack_, true);   // the knob rested: build the stretched / pitched copy
    }
    // microphone sample: wait for sound -> recording -> saved -> load into the track it was started on
    if (mic_ == 1 && st_.sampleState == 1) mic_ = 2;
    if (mic_ == 3 && st_.samplePending) {
        const std::string path = s_.lastSamplePath();
        s_.recorderDismiss();
        refreshLibrary();
        for (size_t i = 0; i < lib_.size(); i++) if (lib_[i].path == path) { const int keep = track_; track_ = micTrack_; loadEntry((int)i); track_ = keep; break; }
        mic_ = 0; say("SAMPLE LOADED");
    }
    if (mic_ == 3 && st_.sampleState == 0 && !st_.samplePending && now - lastDraw_ > 3000) mic_ = 0;   // too short: nothing was kept
    if (now - lastDraw_ >= 33) {
        lastDraw_ = now;
        if (now - healAt_ >= 150 && pending_.size() < 4) { healAt_ = now; if (!needClear_) heal(); }
        draw(now);
        leds();
    }
    flush(now);
}

// ---- FM-1 link ----
void SamplerApp::sysex(int cmd, const std::vector<uint8_t>& p, bool urgent, int key) {
    std::vector<uint8_t> m = {0xf0, 0x7d, 'D', 'A', 'W', 1, (uint8_t)cmd};
    cat(m, p); m.push_back(0xf7);
    if (cmd == 2) { pending_.clear(); pendingBytes_ = 0; }
    if (key >= 0) for (auto& f : pending_) if (f.key == key) { pendingBytes_ += m.size() - f.bytes.size(); f.bytes = std::move(m); return; }
    pendingBytes_ += m.size();
    if (urgent) pending_.push_front({key, std::move(m)}); else pending_.push_back({key, std::move(m)});
}
void SamplerApp::clear(uint16_t c) { sysex(2, col(c)); }
void SamplerApp::discardPending(int firstKey, int lastKey) {
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (it->key >= firstKey && it->key <= lastKey) {
            pendingBytes_ -= it->bytes.size();
            it = pending_.erase(it);
        } else ++it;
    }
}
void SamplerApp::rect(int id, int x, int y, int w, int h, uint16_t c) {
    uint64_t sig = (uint64_t)c | (uint64_t)(w & 0xfff) << 16 | (uint64_t)(h & 0xfff) << 28 | (uint64_t)(x & 0xfff) << 40 | (uint64_t)(y & 0xfff) << 52;
    if (!needClear_ && rects_[id] == sig) return;
    rects_[id] = sig;
    std::vector<uint8_t> p; cat(p, n14(x)); cat(p, n14(y)); cat(p, n14(std::max(1, w))); cat(p, n14(std::max(1, h))); cat(p, col(c));
    sysex(3, p, false, id);
}
void SamplerApp::drawText(int id, int x, int y, int scale, uint16_t fg, uint16_t bg, const std::string& text, int width) {
    std::string t = pad(text, (size_t)width);
    SurfaceCell& c = cells_[id];
    if (!needClear_ && c.known && c.text == t && c.fg == fg && c.bg == bg && c.x == x && c.y == y && c.scale == scale) return;
    c.text = t; c.fg = fg; c.bg = bg; c.x = x; c.y = y; c.scale = scale; c.known = true;
    std::vector<uint8_t> p; cat(p, n14(x)); cat(p, n14(y)); p.push_back((uint8_t)scale); cat(p, col(fg)); cat(p, col(bg));
    for (char ch : t) p.push_back((uint8_t)(ch & 127));
    sysex(4, p, false, 1000 + id);
}
void SamplerApp::flush(uint32_t now) {
    // ONE frame per call, at least gap_ ms apart: the FM-1 has a 4-frame queue. Small commands are packed into one batch frame.
    if (pending_.empty() || now - lastSend_ < gap_) return;
    lastSend_ = now;
    std::vector<uint8_t> batch = {0xf0, 0x7d, 'D', 'A', 'W', 1, 10};
    int n = 0;
    while (!pending_.empty()) {
        const auto& f = pending_.front().bytes;
        // A full-screen fill can be expensive; send it as its own frame.
        if (f[6] == 2) break;
        const size_t sub = f.size() - 7;
        if (n > 0 && (batch.size() + 2 + sub + 1 > 290 || sub > 127)) break;
        if (n == 0 && sub > 127) break;
        batch.push_back((uint8_t)(sub >> 7 & 127)); batch.push_back((uint8_t)(sub & 127));
        batch.insert(batch.end(), f.begin() + 6, f.end() - 1);
        pendingBytes_ -= f.size(); pending_.pop_front(); n++;
    }
    if (n == 0) { auto& f = pending_.front().bytes; pendingBytes_ -= f.size(); send_(f.data(), f.size()); pending_.pop_front(); return; }
    batch.push_back(0xf7);
    send_(batch.data(), batch.size());
}
void SamplerApp::heal() {   // a lost frame leaves a hole nothing would redraw: one element is re-sent in the background on every pass
    healCursor_ = (healCursor_ + 1) % (int)cells_.size();
    cells_[(size_t)healCursor_].known = false; rects_[(size_t)healCursor_] = ~0ull;
    const int c2 = 100 + healCursor_ % 160; rects_[(size_t)c2] = ~0ull;
    tileSig_[healCursor_%tileSig_.size()]=~0ull;
    rects_[380+healCursor_%40]=~0ull;
}

void SamplerApp::leds() {
    uint64_t m = 0;
    const bool blinkFast = (nowMs_ / 250) & 1, blinkSlow = (nowMs_ / 500) & 1;
    static const int pageBtn[] = {HOME, FX, ENV, LFO, ARP, SEQ, EDIT, GLO, SAVE, SEL, SEL, HOME, FX, FX, FX, SEQ, SEQ, HOME, HOME};
    m |= 1ull << pageBtn[page_];
    if (st_.playing) m |= 1ull << PLAY;
    if (st_.freeRecording || st_.track_clip_rec[track_] || mic_ == 2) { if (blinkFast) m |= 1ull << REC; }
    else if (st_.freeArmed || mic_ == 1) { if (blinkSlow) m |= 1ull << REC; }
    else if (loopAny_) m |= 1ull << REC;
    // keys follow the tonality: the keyboard stays chromatic, only the keys that belong to the scale light, the root of each octave blinks slowly;
    // with the scale OFF only the root keys light. A sounding key goes dark.
    for (int k = 0; k < 27; k++) {
        const int raw = 53 + k, note = raw + 12 * scale_.octave, pc = ((note - scale_.root) % 12 + 12) % 12;
        bool on = ts_[track_].algo == 3 ? true : scale_.scale < 7 ? scaleIndex(pc, scale_.scale) >= 0 : pc == 0;   // KIT: every key is a pad
        if (on && pc == 0 && scale_.scale < 7 && ts_[track_].algo != 3) on = blinkSlow;
        if (keyCount_[raw]) on = false;
        if (note < 0 || note > 127) on = false;
        if (on) m |= 1ull << (14 + k);
    }
    if (m == lastLed_ && nowMs_ - ledAt_ < 400) return;
    lastLed_ = m; ledAt_ = nowMs_;
    std::vector<uint8_t> p(6);
    for (int i = 0; i < 6; i++) p[i] = (uint8_t)(m >> (7 * i) & 127);
    sysex(1, p, true, 900);   // keyed: a newer state replaces an older one still waiting in the queue
}

// ---- screen ----
// Layout (240x240): track tags, page and state, sample name, a 60 px picture that depends on the page, the loop strip, four knobs, a hint.
void SamplerApp::draw(uint32_t now) {
    if ((int)theme::light != lastLight_) { lastLight_ = theme::light; setPalette(theme::light); needClear_ = true; }
    const bool waveform=mic_==1||mic_==2||page_==P_HOME||page_==P_ECHO||page_==P_GRAIN||page_==P_MOTION;
    const int layout=waveform?0:1;
    if (needClear_) {
        fwDirty_ = true; clear(BLACK);
        std::fill(cells_.begin(), cells_.end(), SurfaceCell());
        std::fill(rects_.begin(), rects_.end(), ~0ull);
        heroSig_.fill(~0u); tileSig_.fill(~0ull); picturePage_=-1;
        std::vector<uint8_t> pal;
        for(auto c:surfaceArt::palette())cat(pal,col(rgb565(c)));
        sysex(8,pal,false,920);
    } else if (layout != lastHeroLayout_) {
        // Commands queued for the previous plot must not be replayed
        // before or after the replacement plot's background.
        discardPending(100, 299);
        discardPending(1100, 1101);
        discardPending(2000,2041);
        tileSig_.fill(~0ull);picturePage_=-1;
        discardPending(361, 361);
        std::fill(rects_.begin() + 100, rects_.begin() + 300, ~0ull);
        cells_[100].known = cells_[101].known = false;
        heroSig_.fill(~0u);
        if(layout==0){
            rects_[361]=~0ull;
            rect(361,9,52,222,49,WELL);
        }
    }
    lastHeroLayout_ = layout;
    if (needClear_) {
        rect(360, 6, 5, 228, 30, PANEL);
        // Inset sample name field, with the same chrome as the phone selector.
        rect(366, 33, 7, 174, 26, rgb565(theme::RAISED));
        rect(367, 33, 7, 174, 1, rgb565(theme::SHADOW));
        rect(368, 33, 8, 1, 24, rgb565(theme::SHADOW));
        rect(369, 33, 32, 174, 1, rgb565(theme::HI));
        rect(370, 206, 8, 1, 24, rgb565(theme::HI));
        rect(361, 9, 52, 222, 49, WELL);
        for (int i = 0; i < 4; ++i) rect(362 + i, 8 + (i % 2) * 114, 121 + (i / 2) * 48, 110, 46, PANEL);
    }
    char b[96];
    const Knobs k = knobs();
    // header: the track number and the sample name, the transport state as the same pictogram as on the phone
    const bool recNow = st_.freeRecording || st_.track_clip_rec[track_];
    {
        const std::string lab = s_.trackLabel(track_);
        const uint32_t tc = theme::trackColor(track_);
        // Side strips leave the text cell independent of cache repairs.
        rect(371, 8, 7, 22, 1, rgb565(tc));
        rect(372, 8, 32, 22, 1, rgb565(tc));
        rect(373, 8, 8, 4, 24, rgb565(tc));
        rect(374, 26, 8, 4, 24, rgb565(tc));
        drawText(0, 12, 8, 3, rgb565(theme::contrastInk(tc)), rgb565(tc), std::to_string(track_ + 1), 1);
        drawText(1, 38, 12, 2, lab.empty() ? DIM : PAPER, rgb565(theme::RAISED), lab.empty() ? "EMPTY" : lab, 16);
        const int stt = recNow ? 2 : st_.playing ? 1 : 0;
        static int lastSt = -1; if (stt != lastSt || needClear_) { lastSt = stt; rects_[26] = ~0ull; for (int i = 0; i < 16; i++) rects_[300 + i] = ~0ull; }
        rect(26, 212, 12, 16, 16, PANEL);
        for (int i = 0; i < 16; i++) {
            int w = 16, x = 212; uint16_t c = PAPER;
            if (stt == 1) { w = std::max(1, (int)std::lround(16 * (1 - std::fabs(i - 7.5f) / 8))); c = LIVE; }
            else if (stt == 2) { const float d = (i - 7.5f) / 8; w = std::max(1, (int)std::lround(16 * std::sqrt(std::max(0.f, 1 - d * d)))); x = 212 + (16 - w) / 2; c = RECORD; }
            rect(300 + i, x, 12 + i, w, 1, c);
        }
    }
    const uint32_t pc = pageAccent(page_);
    drawText(20, 12, 36, 2, rgb565(theme::contrastInk(pc)), rgb565(pc), kPage[page_], 9);
    menuMarkers();
    if(waveform) hero(now,k); else picture(now);
    // the loop: a thin bar with the position, under it the state (mic / toast / audition, else the sample mode and the loop)
    std::string line;
    if (mic_ == 1) line = "MIC: WAITING FOR SOUND";
    else if (mic_ == 2) line = fmt("MIC: RECORDING %.1fS", st_.sampleSeconds);
    else if (!toast().empty()) line = toast();
    else if (auditionAt_) { const Entry& e = lib_[(size_t)libCursor_]; line = ">" + e.label + "  " + e.group; }
    else line = (algo() == 4 ? std::string("GRANULAR/") + echoModeName(ts_[track_].echo.mode) : std::string(kAlgo[ts_[track_].algo])) + "  " + loopText();
    const bool hot = !toast().empty() || mic_ || auditionAt_ || st_.freeRecording || recNow || st_.freeArmed;
    const bool hasLoopBar = loopAny_ && st_.loopSamples > 0;
    const int px = std::min(215, std::max(0, (int)(cl(st_.track_has_clip[track_] ? st_.clipPos[track_] : (hasLoopBar ? st_.pos / st_.loopSamples : 0)) * 215)));
    const uint16_t loopCol = recNow ? rgb565(theme::TRACE_RED) : TIME;
    if (needClear_) rect(24, 12, 103, 216, 4, WELL);
    if (hasLoopBar || recNow) {
        rect(25, 12, 103, std::max(1, px), 4, px ? loopCol : WELL);
        rect(27, 12 + px, 103, 216 - px, 4, WELL);
    } else { rect(25, 12, 103, 216, 4, WELL); rects_[27] = ~0ull; }
    drawText(23, 12, 110, 1, hot ? RECORD : DIM, BLACK, line, 38);
    // knobs: 2 x 2: label, value, bar
    for (int i = 0; i < 4; i++) {
        const int cx = 12 + (i % 2) * 114, cy = 124 + (i / 2) * 48;
        const Knob& p = k[(size_t)i];
        const uint16_t kc = rgb565(theme::encoderColor(i)), kt = rgb565(theme::encoderTrace(i));
        snprintf(b, sizeof b, "K%d %s", i + 1, p.name.c_str());
        drawText(40 + i * 2, cx, cy, 1, kc, PANEL, p.valid ? b : "", 17);
        const bool big = p.value.size() <= 7;
        const int kind = !p.valid ? 0 : big ? 3 : 2;
        static int lastKind[4] = {-1, -1, -1, -1};
        if (needClear_ || lastKind[i] != kind) {
            discardPending(350 + i, 350 + i);
            discardPending(1041 + i * 2, 1041 + i * 2);
            rects_[350 + i] = ~0ull;
            rect(350 + i, cx, cy + 10, 108, 24, WELL);
            cells_[41 + i * 2].known = false;
            lastKind[i] = kind;
        }
        if (p.valid) {
            if (big) drawText(41 + i * 2, cx, cy + 10, 3, kt, WELL, p.value, 7);
            else drawText(41 + i * 2, cx, cy + 14, 2, kt, WELL, p.value, 10);
        }
        // Two adjacent nonoverlapping rectangles cover the entire slider.
        // Reserve a single background pixel at full scale for the boundary.
        const int bw = p.valid ? (int)(cl(p.norm) * 102) : 0;
        rect(60 + i, cx, cy + 37, std::max(1, bw), 5, bw ? kc : BAR);
        rect(70 + i, cx + std::min(101, bw), cy + 37, std::max(1, 102 - bw), 5, bw == 102 ? kc : BAR);
    }
    const char* hint = mic_ == 1 ? (page_ == P_SAVE ? "K4 = TRIGGER LEVEL  LINE = THRESHOLD" : "K1 = TRIGGER LEVEL  LINE = THRESHOLD") : mic_ == 2 ? "RECORDING  K1 ON LOOP PAGE STOPS"  : page_ == P_LEN ? "K1 X2 OR /2  SEL AGAIN FOR TEMPO" : page_ == P_SAVE ? "SAVE AND CLEAR LOOPS: PROJECT MENU ON THE PHONE" : page_ == P_TEMPO ? "K1 BARS  K2 BPM  K3 MODE" : page_ == P_KEY ? "KEYS FOLLOW THE KEY  CHANGE ANY TIME"
                     : page_ == P_ECHO ? "HOME AGAIN: GRAIN" : page_ == P_HOME && algo() == 4 ? "HOME AGAIN: ECHOFIELD  K3 MODE K4 POS" : page_ == P_EFILTER ? "FILTER MIX ON SEQ: EF MIX" : page_ == P_ETAPE ? "SEQ AGAIN  ENABLE TAPE ON EF MIX" : page_ == P_EMIX || page_ == P_SEQ ? "SEQ AGAIN: NEXT TAPE/MIX PAGE" : page_ == P_GRAIN || page_ == P_MOTION ? "HOME AGAIN: NEXT GRANULAR PAGE" : fxPage(page_) ? "FX AGAIN: NEXT EFFECT PAGE" : "SEL TRACK  PRE BROWSE  ALG ALGORITHM";
    drawText(90, 12, 228, 1, DIM, BLACK, hint, 38);
    needClear_ = false;
}

void SamplerApp::menuMarkers() {
    const Page parent=menuParent(page_);
    int anchor=0;
    for(int i=0;i<kPrimaryMenus;++i)if(primaryMenuPage(i)==parent)anchor=i;
    std::array<Page,kPrimaryMenus> child{};
    child.fill(kPages);
    int count=0;
    for(int i=0;i<kPages;++i) {
        Page p=menuPage(i);
        if(menuParent(p)!=parent||!pageAvailable(p))continue;
        const int slot=anchor+(parent==P_LEN?-count:count);
        if(slot>=0&&slot<kPrimaryMenus)child[slot]=p;
        ++count;
    }
    const uint16_t accent=rgb565(pageAccent(parent));
    for(int i=0;i<kPrimaryMenus;++i) {
        const int x=kMenuX+i*kMenuStep;
        const bool visible=count>1&&child[i]!=kPages;
        rect(380+i,x,44,kMenuWidth,6,i==anchor?accent:BAR);
        rect(390+i,x,36,kMenuWidth,5,visible?(child[i]==page_?rgb565(pageAccent(page_)):BAR):BLACK);
        rect(410+i,x+3,41,2,3,count>1&&i==anchor?accent:BLACK);
        if(i+1<kPrimaryMenus)rect(400+i,x+kMenuWidth,38,kMenuStep-kMenuWidth,1,
            visible&&count>1&&child[i+1]!=kPages?accent:BLACK);
    }
}

void SamplerApp::picture(uint32_t now) {
    // Coalesce changes while the USB display catches up. Static tiles
    // are never resent merely because a frame was drawn on the phone.
    if(!needClear_&&picturePage_==page_&&now-pictureAt_<100)return;
    pictureAt_=now;picturePage_=page_;
    if(!graphic_)graphic_=std::make_unique<SamplerUI>(*this,false);
    const auto& ops=graphic_->surfaceGraphic(now);
    surfaceArt::Raster plot(ops);
    const auto pal=surfaceArt::palette();
    int idx=0;
    for(int y=0;y<46;y+=7)for(int x=0;x<216;x+=36,++idx) {
        const int w=36,h=std::min(7,46-y);
        std::array<uint8_t,252> pix{};
        int n=0;uint64_t hash=1469598103934665603ull;
        for(int j=0;j<h;++j)for(int i=0;i<w;++i){
            const auto c=surfaceArt::nearest(plot.pixel(x+i,y+j),pal);
            pix[n++]=c;hash=(hash^c)*1099511628211ull;
        }
        if(!needClear_&&tileSig_[idx]==hash)continue;
        tileSig_[idx]=hash;
        std::vector<uint8_t> p;
        cat(p,n14(12+x));cat(p,n14(54+y));cat(p,n14(w));cat(p,n14(h));
        for(int i=0;i<n;){
            int run=1;while(i+run<n&&pix[i+run]==pix[i]&&run<135)++run;
            p.push_back(uint8_t((pix[i]<<3)|std::min(7,run-1)));
            if(run>=8)p.push_back(uint8_t(run-8));
            i+=run;
        }
        // 252 worst-case tokens + header < firmware's 300-byte frame.
        sysex(9,p,false,2000+idx);
    }
}

void SamplerApp::hero(uint32_t now, const Knobs& k) {
    const int x0 = 12, y0 = 54, W = 216, H = 46;
    // one column: background (cleared only when the height changed) + bar
    auto& sig = heroSig_;
    auto column = [&](int idx, int x, int w, int h, uint16_t fg, bool fromBottom) {
        h = std::min(H, std::max(1, h));
        const uint32_t s = (uint32_t)h | (uint32_t)fg << 8 | (uint32_t)w << 24 | (uint32_t)fromBottom << 31;
        if (sig[idx] == s && !needClear_) return;
        sig[idx] = s;
        // Replace pending background/bar as a pair. If the older
        // background already reached the screen, never enqueue its
        // replacement behind a still-pending foreground.
        const bool queuedBg = std::any_of(pending_.begin(), pending_.end(), [=](const Frame& f) { return f.key == 100 + idx; });
        if (!queuedBg) discardPending(200 + idx, 200 + idx);
        rects_[100 + idx] = ~0ull; rects_[200 + idx] = ~0ull;
        rect(100 + idx, x, y0, w, H, WELL);
        rect(200 + idx, x, fromBottom ? y0 + H - h : y0 + (H - h) / 2, w, h, fg);
    };
    const TrackState& x = ts_[track_];
    if (mic_ == 1 || mic_ == 2) {   // microphone: input level against the trigger line (K1 sets it), then the growing take
        const int n = 54; const float thr = std::min(1.f, micThreshold() * 2.f), lv = std::min(1.f, st_.inputPeak * 2.f);
        if (mic_ == 1) {
            const int ti = (int)(thr * (n - 1));
            for (int i = 0; i < n; i++) column(i, x0 + i * 4, 3, i == ti ? H : (int)(lv * H * (i < (int)(lv * n) ? 1.f : 0.f)) + 1, i == ti || lv > thr ? rgb565(theme::TRACE_RED) : TRACE, i == ti);
        } else {
            float live[2048]; const int ln = s_.livePeaks(live, 2048), per = std::max(1, (ln + n - 1) / n);
            for (int i = 0; i < n; i++) { float v = 0; for (int j = i * per; j < std::min(ln, (i + 1) * per); j++) v = std::max(v, live[j]); column(i, x0 + i * 4, 3, (int)(std::min(1.f, v * 1.1f) * H) + 1, i * per < ln ? TRACE : GRID, false); }
        }
    } else if (page_ == P_ECHO || page_ == P_GRAIN || page_ == P_MOTION || (page_ == P_HOME && algo() == 4)) {
        // Actual grain positions and envelopes on the sample's waveform.
        auto pk = peaks(track_, 54);
        float grains[54] = {};
        for (int j = 0; j < st_.voiceN[track_]; ++j) {
            const int bin = std::clamp(int(st_.voicePos[track_][j] * 54), 0, 53);
            grains[bin] = std::max(grains[bin], st_.voiceLvl[track_][j]);
        }
        const int center = std::clamp(int((x.start + x.echo.position * (x.end - x.start)) * 54), 0, 53);
        for (int i = 0; i < 54; ++i) {
            const float pos = (i + 0.5f) / 54.f;
            const bool in = pos >= x.start && pos <= x.end;
            const float v = i < (int)pk.size() ? pk[i] : 0.f;
            const int h = int(std::max(std::min(1.f, v * 1.1f), grains[i]) * H) + 1;
            column(i, x0 + i * 4, 3, i == center ? H : h,
                   i == center ? TIME : grains[i] > 0 ? rgb565(theme::TRACE_VIOLET) : in ? TRACE : GRID, false);
        }
    } else {   // the waveform with the playback window
        auto pk = peaks(track_, 54);
        for (int i = 0; i < 54; i++) {
            const float pos = (i + 0.5f) / 54.f; const bool in = pos >= x.start && pos <= x.end;
            const float v = i < (int)pk.size() ? pk[(size_t)i] : 0.f;
            column(i, x0 + i * 4, 3, (int)(std::min(1.f, v * 1.1f) * H) + 1, in ? TRACE : GRID, false);
        }
    }
    (void)k; (void)now;
}
