// SamplerApp: the whole sampler + audio looper on top of Session, and its FM-1 side.
// Bytes in (MIDI of the FM-1), bytes out (SysEx draw commands for its 240x240 screen and LEDs), no platform code: runs unchanged on the host
// (tools/app_sim) and in the Android app. The phone UI (ui/SamplerUI) reads the same knobs and edits them by touch.
//
// FM-1 map:  encoders  SELECT = track (1..8) | PRESETS = sample library with audition | ALGORITHM = sampler algorithm | K1..K4 = page knobs
//            buttons   HOME sample (start, end, speed, pitch) | FX | ENV | LFO | ARP | SEQ Echofield tape / insert mix | EDIT mixer
//                      GLO key and scale | SAVE loop save (tap) / clear (hold) | PLAY | REC loop record / overdub
#pragma once
#include "../session/Session.h"
#include "../../shared/surface_v2.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class SamplerUI;

struct SurfaceCell { std::string text; uint16_t fg = 0, bg = 0; int x = 0, y = 0, scale = 0; bool known = false; };

class SamplerApp {
public:
    using Send = std::function<void(const uint8_t*, size_t)>;
    static constexpr int kTracks = 8;
    enum Button { OCT_DOWN, OCT_UP, FX, SEL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC };
    enum Page { P_HOME, P_FX, P_ENV, P_LFO, P_ARP, P_SEQ, P_MIX, P_KEY, P_SAVE, P_LEN, P_TEMPO, P_ECHO, P_EFILTER, P_EDELAY, P_EREVERB, P_ETAPE, P_EMIX, P_GRAIN, P_MOTION, kPages };
    struct Knob {
        std::string name, value;
        float norm = 0;
        bool valid = false, isChoice = false;
        float surfaceStep = 0; // normalized detent; zero = action/nonpredictable
        std::function<void(int)> step;       // encoder detents
        std::function<void(float)> set;      // absolute 0..1 (touch)
    };
    using Knobs = std::array<Knob, 4>;
    struct Entry { std::string label, group, path; bool kit; };
    struct ScaleState { int scale = 3, root = 0, octave = 0, mode = 0; };

    SamplerApp(Session& s, Send send);
    ~SamplerApp();

    void onMidi(const uint8_t* msg, size_t n);
    void onMidi(const uint8_t* msg, size_t n, uint32_t eventMs);
    void onSysex(const uint8_t* msg, size_t n);
    void tick(uint32_t nowMs);                    // ~30 Hz
    void resetSurface();
    void releaseSurfaceKeys();

    // ---- state for the phone UI (all of it can also be edited from there) ----
    Page page() const { return page_; }
    bool editor() const { return editor_; }
    // Field examination is a separate single-source workspace. Android owns its offline DSP/view.
    bool fieldMode() const { return fieldMode_; }
    void setFieldMode(bool on);
    std::vector<int> takeFieldEvents() { auto result=std::move(fieldEvents_); fieldEvents_.clear(); return result; }
    void fieldFeedback(const std::string& summary, uint32_t leds,const std::string& pixels);
    bool lightTheme() const;
    // Generative deck has no dependency on tape-page parameters.
    bool generativeDeck() const {return genDeck_;}
    void setGenerativeDeck(bool on);
    bool genTrackRunning(int t)const{return t>=0&&t<kTracks&&genTracks_[t].running;}
    std::string genTrackSource(int t)const{return t>=0&&t<kTracks?genTracks_[t].source:"";}
    int outputRecordState()const{return s_.outputRecordState();}
    float outputRecordSeconds()const{return s_.outputRecordSeconds();}
    void outputRecordToggle();
    const char* contextualPageName(Page p)const;
    Knobs genGlobalKnobs();
    const generative::Config& genConfig()const{return gen().config;}
    const generative::Phrase& genPhrase()const{return gen().phrase;}
    uint32_t genVersion()const{return gen().version;}
    int genPage()const{return gen().page;}
    int genMap()const{return gen().map;}
    bool genRunning()const{return gen().running;}
    int genCursor()const{return gen().cursor;}
    uint16_t genActive()const{return gen().active;}
    float genSeconds()const{return gen().seconds;}
    int genSourceState()const{return s_.generatorLoadState(track_);}
    const std::string& genSource()const{return gen().source;}
    Knobs genKnobs();
    void genHarmonic(int knob,int delta);
    void genSetPage(int p){gen().page=std::clamp(p,0,3);}
    void genSetMap(int m){gen().map=std::clamp(m,0,2);}
    void genApply(int a);
    void genMotif(int m);
    void genSteps(int n);
    void genRelocate(float x,float y);
    void genTransform(float span,float angle);
    void genTogglePin(int event);
    void genPinEnds();
    void genClearPins();
    void genKey(int pitch);
    void genPlay();
    void genStop(bool reset=false);
    void genBrowse(int delta);
    void genLoadEntry(int i);
    int genKeyState(int pitch)const; // 0 off, 1 phrase, 2 sounding/custom, 3 pinned

    void setEditor(bool on);
    void setLight(bool on);
    void gotoPage(Page p);
    static const char* pageName(Page p);
    int track() const { return track_; }
    void selectTrack(int t);
    Knobs knobs();
    Knobs knobs(Page p);
    static const char* algoName(int i);
    static const char* echoModeName(int i);
    bool pageAvailable(Page p) const { if(genDeck_)return p!=P_FX&&p!=P_SEQ&&p!=P_LEN&&p!=P_TEMPO; return p != P_FX && p != P_SEQ && ((p != P_ECHO && p != P_GRAIN && p != P_MOTION) || algo() == 4); }
    static Page menuPage(int slot) {
        constexpr Page pages[] = {P_HOME,P_ECHO,P_GRAIN,P_MOTION,P_FX,P_EFILTER,P_EDELAY,P_EREVERB,P_ENV,P_LFO,P_ARP,P_SEQ,P_ETAPE,P_EMIX,P_MIX,P_KEY,P_SAVE,P_LEN,P_TEMPO};
        return pages[slot];
    }
    static constexpr int kPrimaryMenus=10, kMenuX=116, kMenuStep=12, kMenuWidth=8;
    static Page primaryMenuPage(int slot) {
        constexpr Page p[]={P_HOME,P_FX,P_ENV,P_LFO,P_ARP,P_SEQ,P_MIX,P_KEY,P_SAVE,P_LEN};
        return p[slot];
    }
    static Page menuParent(Page p) {
        if(p==P_ECHO||p==P_GRAIN||p==P_MOTION)return P_HOME;
        if(p>=P_EFILTER&&p<=P_EREVERB)return P_FX;
        if(p==P_ETAPE||p==P_EMIX)return P_SEQ;
        return p==P_TEMPO?P_LEN:p;
    }
    static bool fxPage(Page p) { return p == P_FX || (p >= P_EFILTER && p <= P_EREVERB); }
    int algo() const { return ts_[track_].algo; }
    void setAlgo(int a);
    const std::vector<Entry>& library() const { return lib_; }
    int libraryCursor() const { return genDeck_ ? std::max(0,gen().entry) : libCursor_; }
    void browse(int delta);                       // PRESETS: moves the cursor, audition after a short pause
    void loadEntry(int index);                    // at once, on the selected track
    void refreshLibrary();
    void importFile(const std::string& wavPath);  // a file picked from the disk (already copied into the data dir as a WAV)
    bool wantFile() { bool w = wantFile_; wantFile_ = false; return w; }   // the UI asked for the disk picker: Java polls this
    void requestFile() { wantFile_ = true; }
    void requestFileManager() { fileAction_=1; }
    void requestProjectSave() { fileAction_=2; }
    int takeFileAction(){int a=fileAction_;fileAction_=0;return a;}
    std::string manageFile(int op,const std::string& path,const std::string& name);
    bool saveProject(const std::string& name);
    bool loadProject(const std::string& directory);
    int saveNoticeState() const { return saveNotice_==1||nowMs_<saveNoticeUntil_?saveNotice_:0; }
    const std::string& saveNoticeLabel()const{return saveNoticeLabel_;}
    // looper and recorder
    void transportPlay();
    void transportSet(bool play);                 // phone PLAY / STOP keys
    bool recordingNow() const { bool r = st_.freeRecording || st_.freeArmed; for (int t = 0; t < kTracks; t++) r |= st_.track_clip_rec[t]; return r; }
    void endRecording();                          // PLAY / STOP while recording: the take ends, the loop keeps playing
    void loopRecord();                            // REC: arm / end the first take, or toggle overdub once a loop exists
    void lenAction(bool dbl);
    void seek(float fraction) { s_.seek(fraction); }
    void scrub(int phase /*0 down 1 move 2 up*/, float fraction, Session::ScrubStyle style = Session::ScrubStyle::Timeline) { if (phase == 0) s_.scrubBegin(fraction, style); else if (phase == 1) s_.scrubMove(fraction); else s_.scrubEnd(); }
    void autoTune();                              // MELODIC / ONE-SHOT: detect the pitch of the sample and tune the keys to it (again: reset)
    bool tuned(int t) const { return ts_[t].tuned; }
    void loopSave();
    void loopClear();                             // every loop of every track (phone PROJECT menu)
    void loopClearTrack();                        // the loop of the selected track
    void micSample();                             // toggle: record the microphone into a new sample for the selected track
    float micThreshold() const { return 0.005f * std::pow(100.f, micThr_); }   // linear level that starts a take
    void setMicThreshold01(float v) { micThr_ = std::min(1.f, std::max(0.f, v)); s_.setMicThreshold(micThreshold()); }
    float micThreshold01() const { return micThr_; }
    int micState() const { return mic_; }         // 0 idle, 1 waiting for sound, 2 recording, 3 saving
    bool loopExists() const { return loopAny_; }
    std::string loopText() const;
    std::string toast() const { return nowMs_ < toastUntil_ ? toast_ : ""; }
    void say(const std::string& t) { toast_ = t; toastUntil_ = nowMs_ + 1800; }
    // mixer and per-track state
    struct TrackState {
        int algo = 0;
        float a = 0.1f, d = 0.35f, s = 0.8f, r = 0.63246f;   // release 5 r^2 = 2.0 s
        float lfoRate = 0.3f, lfoDepth = 0.f; int lfoTarget = 0, lfoShape = 0;
        float start = 0.f, end = 1.f, speed = 0.5f, pitch = 0.5f;
        float kitThr = 0.22f, paulStretch = 0.4f, paulWidth = 0.5f;
        EchoParams echo;
        int echoLatch = 0;
        EchoInsertParams echoInsert;
        float echoDelay = 0, echoRoom = 0;
        int arpRate = 0, arpPatt = 0, arpOct = 0; float arpLen = 0.7f;
        
        float vol = 0.8f, pan = 0.5f, sendR = 0.3f, sendD = 0.2f;
        int scSrc = 0; float scThr = 0.5f, scRatio = 0.4f;   // sidechain: source (0 off, 1..8 = track), threshold and ratio knobs
        int entry = -1;
        float root = 60.f; bool tuned = false;   // auto-tune: the detected pitch of the sample
    };
    const TrackState& state(int t) const { return ts_[t]; }
    void setVolume(int t, float v); void setPan01(int t, float v); void setSend(int t, int which, float v);
    Session::Status status() const { return st_; }
    bool hasSample(int t) const { return !s_.trackLabel(t).empty(); }
    ScaleState scale() const { return scale_; }
    void setScale(int scale, int root, int octave, int mode);
    static const char* scaleName(int i);
    static bool inScale(int scale, int pitchClass);   // pitch class relative to the root: does it belong to the scale (OFF: only the root)
    static const char* noteName(int r);
    uint16_t chordGhostMask() const;
    void playNote(int note, bool down);
    void keyEvent(int rawNote, bool down);        // FM-1 key: chromatic note from the firmware
    bool linkSeen() const { return linkSeen_; }
    std::vector<float> peaks(int t, int bins);    // waveform of a track's sample, cached
    std::string sampleLabel(int t) const;
    Session& session() { return s_; }

private:
    // ---- FM-1 drawing ----
    void sysex(int cmd, const std::vector<uint8_t>& payload, bool urgent = false, int key = -1);
    void clear(uint16_t c);
    void rect(int id, int x, int y, int w, int h, uint16_t c);
    void drawText(int id, int x, int y, int scale, uint16_t fg, uint16_t bg, const std::string& s, int width_chars);
    void draw(uint32_t now);
    void leds();
    void drawGenerator(uint32_t now);
    void genChanged();
    void genRefresh();
    void flush(uint32_t now);
    void heal();
    void discardPending(int firstKey, int lastKey);
    void hero(uint32_t now, const Knobs& k);
    void picture(uint32_t now, const Knobs& k);
    void plotMeters(const Knobs& k);
    void menuMarkers();
    // ---- input ----
    void button(int b, bool down);
    void encoder(int i, int delta, uint32_t eventMs);
    void editRange(int track, int edge, float value);
    // ---- parameters ----
    void applyAll(int t);
    void applyAlgo(int t); void applyEnv(int t); void applyLfo(int t); void applyHome(int t, bool commit = false); void applyArp(int t); void applyRoot(int t); void applyMix(int t);
    void applyFx();
    void editEchoFx(int t);
    void editEcho(int t);
    void loadEcho();
    void saveEcho();
    void queueCommit(int t);
    Knob cont(const char* name, float* v, std::function<std::string(float)> fmt, std::function<void()> apply, float stepSize = 0.02f, bool commit = false);
    Knob choice(const char* name, int* v, std::vector<std::string> opts, std::function<void()> apply);

    Session& s_;
    Send send_;
    Page page_ = P_HOME;
    bool editor_ = false, homeLong_ = false, gloLong_ = false;
    bool recLong_=false;uint32_t recAt_=0;
    bool genDeck_=false;
    bool fieldMode_=false,fieldHubSelected_=false;
    std::array<uint8_t,36*21> fieldPixels_{};
    std::array<uint64_t,28> fieldTileSig_{};
    int fieldTileCursor_=0;
    void drawField();
    std::vector<int> fieldEvents_; // pairs: encoder 0..6, button 100+Button, or 200 (theme)
    std::array<std::string,27> fieldLines_{};
    std::array<uint8_t,36*21> fieldSpectrum_{};
    bool surfaceV2_=false;
    uint16_t surfaceAck_=0,surfaceDataGen_=0;
    uint32_t surfaceAt_=0,surfaceDataAt_=0,surfaceCacheAt_=0;
    int surfaceDataContext_=-1,surfaceCacheCursor_=0;
    std::vector<uint8_t> surfaceLast_,surfaceData_;
    std::array<uint64_t,3*8*SV_PAGES> surfaceCacheSig_{};
    int surfaceContext() const;
    void surfaceEdit(const uint8_t*,size_t);
    void drawStateSurface(uint32_t);
    void resetStateSurface();
    uint32_t fieldLeds_=0;
    void fieldEvent(int code,int value);
    struct GenTrack {
        generative::Config config;
        generative::Phrase phrase=generative::makePhrase(config,0);
        int page=0,map=0,cursor=-1,entry=-1;
        bool running=false;
        std::string source,path;
        uint32_t pass=0,version=1,audioSerial=0;
        uint16_t active=0;float seconds=0;
    };
    std::array<GenTrack,kTracks> genTracks_;
    GenTrack& gen(){return genTracks_[track_];}
    const GenTrack& gen()const{return genTracks_[track_];}
    uint32_t genDrawAt_=0,genMapVersion_=0,genOctSentAt_=0;
    int genDrawMap_=-1,genDrawTrack_=-1,genTileCursor_=0;
    void genPalette();
    bool genOctAck_=false;
    std::array<uint8_t,20*20> genCells_{};
    std::array<uint64_t,80> genTileSig_{};
    std::string latestTake_,seenTake_;
    int lastOutputState_=0;
    void genEnsureSound();
    void selectSavedTake();

    uint32_t homeAt_ = 0, gloAt_ = 0;
    int track_ = 0;
    std::array<TrackState, kTracks> ts_;
    // global effects
    EchoReturnParams echoReturns_;
    float reverbSize_ = 0.7f, reverbDw_ = 0.35f, delayTime_ = 0.4f, delayFb_ = 0.45f;
    int loopSrc_ = 1;
    float micThr_ = 0.452f;
    uint32_t repaintAt_ = 0, echoSaveAt_ = 0;
    // library
    std::vector<Entry> lib_;
    int libCursor_ = 0, loadedCursor_ = -1;
    uint32_t auditionAt_ = 0, previewOffAt_ = 0; int previewNote_ = -1, previewTrack_ = -1;
    bool wantFile_ = false, previewAfterLoad_ = false;
    int fileAction_=0,saveNotice_=0;
    uint32_t saveNoticeUntil_=0,micSaveAt_=0;
    bool nativeFileSaving_=false;
    std::string saveNoticeLabel_;
    std::array<std::string,kTracks> samplePaths_{};
    void beginSaveNotice(const std::string& label);
    void endSaveNotice(bool ok);
    // loop / recorder
    Session::Status st_{};
    bool loopAny_ = false;
    int mic_ = 0, micTrack_ = 0;
    int lastSampleVersion_ = 0, lastSampleFails_ = 0;
    uint32_t commitAt_ = 0; int commitTrack_ = -1;
    std::string toast_; uint32_t toastUntil_ = 0;
    std::array<std::vector<float>, kTracks> peaks_; std::array<uint32_t, kTracks> peaksAt_{}; std::array<std::string, kTracks> peaksLabel_;
    // FM-1 link
    struct Frame { int key; std::vector<uint8_t> bytes; };
    std::deque<Frame> pending_; size_t pendingBytes_ = 0;
    uint32_t gap_ = 16, lastSend_ = 0, lastPing_ = 0, lastLoss_ = 0, lastRaise_ = 0, dropped_ = 0, healAt_ = 0, lastDraw_ = 0, nowMs_ = 0;
    bool haveDropBase_ = false, needClear_ = true, linkSeen_ = false;
    int pingId_ = 0, healCursor_ = 0;
    uint64_t lastLed_ = ~0ull; uint32_t ledAt_ = 0;   // the firmware may light / clear a button LED by itself when it is pressed: resend after every button event and every 400 ms
    int knobMsb_ = 0;
    struct EncoderMotion { uint32_t at=0; int sign=0, streak=0, track=-1; Page page=P_HOME; };
    std::array<EncoderMotion,4> motion_{};
    int heldNote_[128];
    int keyNotes_[128][3] = {}; int keyCount_[128] = {};
    bool fwDirty_ = true; uint32_t lenAt_ = 0;
    uint32_t held_ = 0;
    ScaleState scale_;
    uint32_t touchedAt_ = 0; int touchedKnob_ = -1;
    int lastLight_ = -1, lastHeroLayout_ = -1;
    std::array<uint32_t, 100> heroSig_{};
    std::unique_ptr<SamplerUI> graphic_;
    std::array<uint64_t,28> tileSig_{};
    uint64_t pictureSig_=~0ull;
    uint32_t pictureAt_=0;
    int picturePage_=-1;
    std::vector<SurfaceCell> cells_;
    std::vector<uint64_t> rects_;
};
