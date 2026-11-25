// Host tests of the sampler app: every algorithm sounds, LFO targets, stretch/pitch, tape knobs, arp, loop speed, library, import. Exit code 1 on failure.
#include <Session.h>
#include <SamplerApp.h>
#include <SamplerUI.h>
#include <Paul.h>
#include <LoopStretch.h>
#include <Common.h>
#include <Stretch.h>
#include <Wav.h>
#include <Slicer.h>
#include <Reverb.h>
#include <cmath>
#include <thread>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s: ", #c); printf(__VA_ARGS__); printf("\n"); } else { printf("ok   "); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main() {
    Denormals::enableOnThisThread();
    const int sr = SAMPLERATE;
    system("rm -rf /tmp/fm1sampler_t2; mkdir -p /tmp/fm1sampler_t2");
    Session s(sr); s.setDataDir("/tmp/fm1sampler_t2"); s.setSampleDir(std::string(getenv("HOME")) + "/fm1-sampler/samples");
    uint64_t ledMask = 0; int ledFrames = 0;   // the last LED frame the FM-1 got (SysEx 01)
    SamplerApp app(s, [&](const uint8_t* m, size_t n) {   // frames come packed: F0 7D D A W 1 10 { len(2x7bit) cmd payload }... F7
        if (n < 9 || m[6] != 10) return;
        for (size_t i = 7; i + 3 < n;) { const size_t sub = (size_t)m[i] << 7 | m[i + 1]; if (sub < 1 || i + 2 + sub > n) break;
            if (m[i + 2] == 1 && sub == 7) { ledMask = 0; for (int k = 0; k < 6; k++) ledMask |= (uint64_t)(m[i + 3 + k] & 127) << (7 * k); ledFrames++; }
            i += 2 + sub; }
    });
    std::vector<float> buf(512), in(512, 0.f), all; uint32_t now = 0;
    auto run = [&](double sec) { for (long i = 0; i < (long)(sec * sr) / 256; i++) { std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); all.insert(all.end(), buf.begin(), buf.end()); if (i % 5 == 0) { now += 33; app.tick(now); } } };
    auto rmsFrom = [&](size_t from) { double a = 0; size_t n = 0; for (size_t i = from; i < all.size(); i++) { a += all[i] * all[i]; n++; } return n ? std::sqrt(a / n) : 0.0; };
    auto enc = [&](int i, int d) { uint8_t m[3] = {0xb0, (uint8_t)(16 + i), (uint8_t)(64 + d)}; app.onMidi(m, 3); };
    auto btn = [&](int b) { uint8_t m[3] = {0x91, (uint8_t)b, 100}; app.onMidi(m, 3); m[0] = 0x81; app.onMidi(m, 3); };
    auto pageLed = [&](int b) { return (ledMask >> b & 1) != 0; };
    auto play = [&](int note, double sec) { size_t m = all.size(); app.playNote(note, true); run(sec); app.playNote(note, false); run(0.3); return rmsFrom(m); };
    run(2.5);
    CHECK(app.hasSample(0) && app.hasSample(1) && app.hasSample(2) && app.hasSample(3), "four default tracks load (%s, %s, %s, %s)", app.sampleLabel(0).c_str(), app.sampleLabel(1).c_str(), app.sampleLabel(2).c_str(), app.sampleLabel(3).c_str());
    CHECK(!app.hasSample(4), "track 5 is empty");
    { // HOME long press changes the phone view exactly once; SEL cycles pages.
      uint8_t hd[3] = {0x91, (uint8_t)SamplerApp::HOME, 100}, hu[3] = {0x81, (uint8_t)SamplerApp::HOME, 0};
      app.onMidi(hd, 3); run(1.0); CHECK(app.editor(), "holding HOME opens the original editor");
      run(0.8); CHECK(app.editor(), "holding HOME does not toggle repeatedly");
      app.onMidi(hu, 3); app.onMidi(hd, 3); run(1.0); app.onMidi(hu, 3);
      CHECK(!app.editor(), "holding HOME again returns to the deck");
      btn(SamplerApp::GLO); CHECK(app.page() == SamplerApp::P_KEY, "GLO opens key page");
      btn(SamplerApp::GLO); CHECK(app.page() == SamplerApp::P_KEY, "GLO remains on key page");
      app.gotoPage(SamplerApp::P_FX);
      const bool initialLight = theme::light;
      uint8_t gd[3] = {0x91, (uint8_t)SamplerApp::GLO, 100}, gu[3] = {0x81, (uint8_t)SamplerApp::GLO, 0};
      app.onMidi(gd, 3); run(1.0);
      CHECK(theme::light != initialLight && app.page() == SamplerApp::P_FX, "GLO hold switches theme without changing page");
      run(0.8);
      CHECK(theme::light != initialLight, "GLO hold switches theme only once");
      app.onMidi(gu, 3);
      CHECK(app.page() == SamplerApp::P_FX, "GLO release after hold preserves page");
      app.onMidi(gd, 3); run(1.0); app.onMidi(gu, 3);
      CHECK(theme::light == initialLight, "second GLO hold restores theme");
      btn(SamplerApp::GLO); CHECK(app.page() == SamplerApp::P_KEY, "short GLO still opens key page");
      btn(SamplerApp::SEL); CHECK(app.page() == SamplerApp::P_LEN, "SEL opens length page");
      btn(SamplerApp::SEL); run(0.6); CHECK(app.page() == SamplerApp::P_TEMPO && pageLed(SamplerApp::SEL) && !pageLed(SamplerApp::GLO), "SEL cycles to tempo and keeps its LED lit");
      btn(SamplerApp::SEL); CHECK(app.page() == SamplerApp::P_LEN, "SEL returns to length page");
      btn(SamplerApp::HOME); CHECK(app.page() == SamplerApp::P_HOME && !app.editor(), "short HOME selects sample page only"); }
    // algorithms on track 1
    for (int a = 0; a < 6; a++) { app.setAlgo(a); run(0.3); double r = play(60, 1.0); CHECK(r > 0.003, "algorithm %s sounds (rms %.4f)", SamplerApp::algoName(a), r); }
    app.setAlgo(4);
    app.gotoPage(SamplerApp::P_HOME);
    btn(SamplerApp::HOME); CHECK(app.page() == SamplerApp::P_ECHO, "granular HOME cycles to ECHOFIELD");
    auto ek = app.knobs();
    CHECK(ek[0].name == "TEXTURE" && ek[1].name == "SCATTER" && ek[2].name == "DRIFT" && ek[3].name == "EVOLVE", "ECHOFIELD performance controls");
    ek[0].set(0.23f); ek[1].set(0.62f); ek[2].set(0.17f); ek[3].set(0.81f);
    app.setAlgo(0); CHECK(app.page() == SamplerApp::P_HOME, "leaving granular closes ECHOFIELD page");
    app.setAlgo(4); btn(SamplerApp::HOME); run(0.6);
    CHECK(app.page() == SamplerApp::P_ECHO && pageLed(SamplerApp::HOME), "ECHOFIELD keeps HOME LED");
    CHECK(app.knobs()[0].norm == 0.23f && app.knobs()[1].norm == 0.62f && app.knobs()[2].norm == 0.17f && app.knobs()[3].norm == 0.81f, "granular settings survive algorithm cycle");
    app.selectTrack(1); CHECK(app.page() == SamplerApp::P_HOME && app.state(1).echo.texture == 0.72f, "granular settings are per track");
    { Session restored(sr); restored.setDataDir("/tmp/fm1sampler_t2");
      SamplerApp saved(restored, [](const uint8_t*, size_t) {});
      CHECK(std::abs(saved.state(0).echo.texture - 0.23f) < 1e-6f &&
            std::abs(saved.state(0).echo.scatter - 0.62f) < 1e-6f &&
            std::abs(saved.state(0).echo.drift - 0.17f) < 1e-6f &&
            std::abs(saved.state(0).echo.evolve - 0.81f) < 1e-6f,
            "ECHOFIELD settings survive app reconstruction"); }
    app.selectTrack(0);
    app.gotoPage(SamplerApp::P_HOME);
    app.knobs()[3].set(0.35f);
    {
        SamplerUI ui(app); ui.setSize(860, 400, 0, 0);
        ui.setEditor(false); ui.frame(now);
        auto marker = [&]() {
            for (const auto& o : ui.ops()) if (o.type == UiOp::TEXT && o.text == "POS 35.0%" && o.color == theme::TRACE_AMBER) return true;
            return false;
        };
        auto plot = [&]() {
            std::vector<UiOp> ops; bool inPlot = false;
            for (const auto& o : ui.ops()) {
                if (o.type == UiOp::CLIP && o.f[0] > 440 && o.f[1] < 60) inPlot = true;
                if (inPlot) ops.push_back(o);
                if (inPlot && o.type == UiOp::ENDCLIP) break;
            }
            return ops;
        };
        CHECK(marker(), "deck SAMPLE has position marker");
        const auto home = plot();
        app.gotoPage(SamplerApp::P_ECHO); ui.frame(now);
        CHECK(marker(), "deck ECHOFIELD has position marker");
        const auto echo = plot();
        bool same = !home.empty() && home.size() == echo.size();
        for (size_t i = 0; same && i < home.size(); ++i) {
            same = home[i].type == echo[i].type && home[i].color == echo[i].color && home[i].text == echo[i].text;
            for (int j = 0; same && j < 6; ++j) same = home[i].f[j] == echo[i].f[j];
        }
        CHECK(same, "SAMPLE and ECHOFIELD share waveform drawing");
        ui.setEditor(true); ui.frame(now);
        CHECK(marker(), "original editor has position marker on ECHOFIELD");
        app.gotoPage(SamplerApp::P_HOME); ui.frame(now);
        CHECK(marker(), "original editor has position marker on SAMPLE");
        // The marker is a small amber triangle at the bottom of the sample.
        for (const auto& o : ui.ops()) if (o.type == UiOp::TRI && o.color == theme::TRACE_AMBER &&
                o.f[1] > 200 && o.f[1] < 240) {
            const float x = o.f[4], y = o.f[1] - 3;
            ui.touch(0, x, y, now); ui.touch(1, x + 41.2f, y, now); ui.touch(2, x + 41.2f, y, now);
            break;
        }
        CHECK(app.state(0).echo.position > 0.44f && app.state(0).echo.position < 0.46f, "editor position handle edits the granular parameter");
        ui.setEditor(false);
    }
    app.gotoPage(SamplerApp::P_HOME);
    btn(SamplerApp::HOME); btn(SamplerApp::HOME);
    CHECK(app.page()==SamplerApp::P_GRAIN && app.knobs()[3].name=="FREEZE","grain controls and freeze");
    app.knobs()[3].set(1); CHECK(app.state(0).echo.freeze==1,"freeze enabled");
    app.knobs()[3].set(0);
    btn(SamplerApp::HOME);
    CHECK(app.page()==SamplerApp::P_MOTION && app.knobs()[3].name=="LATCH","motion and latch page");
    app.knobs()[3].set(1); app.playNote(60,true); run(.4);app.playNote(60,false);run(3);
    CHECK(app.status().voiceN[0]>0,"granular latch sustains after key release");
    app.knobs()[3].set(0);
    // ADSR release specifies the time to -40 dB; retirement is at -80 dB.
    const float releaseSeconds = 5.f * app.state(0).r * app.state(0).r;
    run(2.1 * releaseSeconds + .25);
    CHECK(app.status().voiceN[0]==0,"disabling latch releases grains after envelope decay");
    app.setAlgo(0);
    app.gotoPage(SamplerApp::P_HOME);
    const SamplerApp::Page fxPages[]={SamplerApp::P_FX,SamplerApp::P_EFILTER,SamplerApp::P_EDELAY,SamplerApp::P_EREVERB,SamplerApp::P_FX};
    for(auto pg:fxPages){btn(SamplerApp::FX);CHECK(app.page()==pg,"FX menu cycle");}
    const SamplerApp::Page seqPages[]={SamplerApp::P_SEQ,SamplerApp::P_ETAPE,SamplerApp::P_EMIX,SamplerApp::P_SEQ};
    for(auto pg:seqPages){btn(SamplerApp::SEQ);CHECK(app.page()==pg,"SEQ menu cycle");}
    app.gotoPage(SamplerApp::P_EDELAY);
    app.knobs()[2].set(.3f);
    app.gotoPage(SamplerApp::P_EREVERB);
    app.knobs()[0].set(.4f);
    app.gotoPage(SamplerApp::P_EMIX);
    app.knobs()[2].set(.25f);app.knobs()[3].set(.4f);
    run(.5);
    CHECK(app.state(0).echoDelay==.3f && app.state(0).echoRoom==.4f &&
          app.state(0).echoInsert.tape==.25f && app.state(0).echoInsert.filterMix==.4f,
          "ECHOFIELD wet routing and inserts");
    CHECK(app.state(1).echoDelay==0 && app.state(1).echoRoom==0 &&
          app.state(1).echoInsert.tape==0 && app.state(1).echoInsert.filterMix==0,
          "ECHOFIELD routing independent across tracks");
    app.knobs()[2].set(0);app.knobs()[3].set(0);
    app.gotoPage(SamplerApp::P_EDELAY);app.knobs()[2].set(0);
    app.gotoPage(SamplerApp::P_EREVERB);app.knobs()[0].set(0);
    { // Both phone views use the same menu group and direct child navigation.
        SamplerUI ui(app);
        ui.setSize(860,400,0,0);
        auto tap=[&](float x,float y){ui.frame(now);ui.touch(0,x,y,now);ui.touch(2,x,y,now);};
        ui.setEditor(false);app.gotoPage(SamplerApp::P_HOME);
        // At 860 logical pixels the primary row is 10 x 24, right aligned.
        tap(632,163);CHECK(app.page()==SamplerApp::P_FX,"deck opens FX group");
        tap(632,163);CHECK(app.page()==SamplerApp::P_EFILTER,"deck primary tab cycles submenu");
        tap(753.5f,142);CHECK(app.page()==SamplerApp::P_EDELAY,"deck selects delay directly");
        tap(632,163);CHECK(app.page()==SamplerApp::P_EREVERB,"deck cycles from selected child");
        tap(632,163);CHECK(app.page()==SamplerApp::P_FX,"deck wraps FX group");
        tap(632,163);CHECK(app.page()==SamplerApp::P_EFILTER,"deck cycles with child above parent");
        tap(645,142);CHECK(app.page()==SamplerApp::P_FX,"overlapping child is independently selectable");
        tap(728,163);CHECK(app.page()==SamplerApp::P_SEQ,"deck opens SEQ group");
        tap(728,163);CHECK(app.page()==SamplerApp::P_ETAPE,"deck cycles tape submenu");
        tap(817,142);CHECK(app.page()==SamplerApp::P_EMIX,"tape children cascade right");
        tap(824,163);CHECK(app.page()==SamplerApp::P_LEN,"deck opens LENGTH group");
        tap(756.5f,142);CHECK(app.page()==SamplerApp::P_TEMPO,"length children cascade left");
        tap(824,163);CHECK(app.page()==SamplerApp::P_LEN,"length parent wraps from TEMPO");
        ui.setEditor(true);
        tap(680,66);CHECK(app.page()==SamplerApp::P_FX,"editor opens FX group");
        tap(680,66);CHECK(app.page()==SamplerApp::P_EFILTER,"editor primary button cycles submenu");
        tap(758,130);CHECK(app.page()==SamplerApp::P_EDELAY,"editor selects delay directly");
        tap(632,98);CHECK(app.page()==SamplerApp::P_SEQ,"editor opens SEQ group");
        tap(804,130);CHECK(app.page()==SamplerApp::P_EMIX,"editor selects EF MIX directly");
        ui.setEditor(false);
    }
    // LFO on each target
    btn(SamplerApp::LFO);
    for (int tg = 0; tg < 4; tg++) { auto k = app.knobs(); while (app.state(0).lfoTarget < tg) k[2].step(1); enc(3, 40); run(0.2); double r = play(60, 1.2); CHECK(r > 0.002, "LFO target %d sounds (rms %.4f), lfo value %.2f", tg, r, app.status().lfoValue[0]); }
    enc(3, -80); run(0.3);
    // stretch + pitch (offline processing, applied when the knob rests)
    btn(SamplerApp::HOME); enc(4, 20); enc(5, 12); run(2.0); { double r = play(60, 1.0); CHECK(r > 0.002, "stretch + pitch sound (rms %.4f)", r); }
    enc(4, -20); enc(5, -12); run(2.0);
    // tape page
    btn(SamplerApp::SEQ); enc(3, 30); enc(4, 30); enc(5, 20); run(0.3); { double r = play(60, 1.0); CHECK(r > 0.002 && std::isfinite(r), "exciter + lofi + wobble sound (rms %.4f)", r); }
    // arp
    btn(SamplerApp::ARP); enc(2, 1); for (int i = 0; i < 6; i++) enc(2, 1); run(0.2);
    { size_t m = all.size(); app.playNote(60, true); app.playNote(64, true); app.playNote(67, true); run(1.5); app.playNote(60, false); app.playNote(64, false); app.playNote(67, false); run(0.3); CHECK(rmsFrom(m) > 0.002, "arp sounds (rms %.4f)", rmsFrom(m)); }
    for (int i = 0; i < 12; i++) enc(2, -1); run(0.2);
    // loop: free take, speed, overdub, save
    btn(SamplerApp::REC); run(0.1);
    int seq[] = {60, 64, 67, 72}; for (int n : seq) { app.playNote(n, true); run(0.3); app.playNote(n, false); run(0.1); }
    btn(SamplerApp::REC); run(0.5);
    CHECK(app.status().track_has_clip[0] && app.loopExists(), "first take became the loop (%.0f bpm, %d bars)", app.status().bpm, app.status().loopBars);
    size_t m = all.size(); run(2.0); CHECK(rmsFrom(m) > 0.003, "loop plays back (rms %.4f)", rmsFrom(m));
    btn(SamplerApp::SEQ); enc(2, -10); run(0.1); m = all.size(); run(2.0); CHECK(rmsFrom(m) > 0.001 && app.status().clipPos[0] >= 0.f, "loop at half speed plays (pos %.2f)", app.status().clipPos[0]);
    enc(2, 10); run(0.2);
    btn(SamplerApp::REC); run(0.1); app.playNote(55, true); run(0.3); app.playNote(55, false); run(2.0); btn(SamplerApp::REC); run(0.2);
    CHECK(!app.status().track_clip_rec[0], "overdub ended");
    { app.selectTrack(0); app.setAlgo(4); run(0.3); app.playNote(60, true); run(0.8); CHECK(app.status().voiceN[0] > 0, "granular: active grains are reported (%d)", app.status().voiceN[0]); app.playNote(60, false); run(0.5); app.setAlgo(0); run(0.2); }
    { const double L = app.status().loopSamples; btn(SamplerApp::SEL); enc(2, 1); run(0.3);
      CHECK(app.status().loopSamples == 2 * L, "LENGTH page: loop doubled (%.0f -> %.0f)", L, app.status().loopSamples);
      m = all.size(); run(2.5); CHECK(rmsFrom(m) > 0.003, "doubled loop plays (rms %.4f)", rmsFrom(m));
      enc(2, -1); run(0.3); CHECK(app.status().loopSamples == L, "LENGTH page: loop halved back (%.0f)", app.status().loopSamples); }
    app.playNote(60, true); run(0.3); CHECK(app.status().voiceN[0] > 0 && app.status().voicePos[0][0] > 0.f, "active sampler voices reported (%d, pos %.3f)", app.status().voiceN[0], app.status().voicePos[0][0]); app.playNote(60, false); run(0.5);
    { app.selectTrack(3); app.setAlgo(3); run(0.3); size_t km = all.size(); uint8_t kd[3] = {0x90, (uint8_t)(53 + 26), 100}, ku[3] = {0x80, (uint8_t)(53 + 26), 0}; app.onMidi(kd, 3); run(0.6); app.onMidi(ku, 3); run(0.3);
      CHECK(rmsFrom(km) > 0.002, "KIT: the last of the 27 keys plays a pad (rms %.4f)", rmsFrom(km)); app.selectTrack(0); }
    app.seek(0.5f); run(0.05); { const float cp = app.status().clipPos[0]; CHECK(cp > 0.49f && cp < 0.65f, "seek: playhead jumps to the middle of the loop (%.3f)", cp); }
    uint8_t sd[3] = {0x91, (uint8_t)SamplerApp::SAVE, 100}, su[3] = {0x81, (uint8_t)SamplerApp::SAVE, 100};
    app.onMidi(sd, 3); run(0.9); app.onMidi(su, 3); run(0.3);   // the FM-1 SAVE button neither saves nor clears any more
    CHECK(app.loopExists() && app.page() == SamplerApp::P_SAVE, "SAVE button only opens the LOOP page (held for 0.9 s: loop stays)");
    app.loopSave(); run(1.0);
    int wavs = 0; { FILE* p = popen("ls /tmp/fm1sampler_t2/sample_*.wav 2>/dev/null | wc -l", "r"); if (p) { fscanf(p, "%d", &wavs); pclose(p); } }
    CHECK(wavs == 1, "SAVE wrote the loop into the library (%d files)", wavs);
    app.loopClearTrack(); run(0.3);
    CHECK(!app.status().track_has_clip[0], "clearing one track's loop");
    app.loopClear(); run(0.3);
    CHECK(!app.loopExists(), "PROJECT menu: clear all loops");
    { bool any = false; for (int t = 0; t < 8; t++) any |= app.status().track_has_clip[t]; CHECK(!any, "...and every other track's loop"); }
    { constexpr int FXB = SamplerApp::FX, ENVB = SamplerApp::ENV;
      btn(FXB); run(0.6); const bool fx1 = pageLed(FXB) && !pageLed(SamplerApp::HOME);
      btn(ENVB); run(0.6); CHECK(fx1 && pageLed(ENVB) && !pageLed(FXB), "FM-1 LEDs: pressing another page button moves the light (ENV lit, FX dark)");
      const int before = ledFrames; ledMask = 0; run(1.0); CHECK(ledFrames > before && pageLed(ENVB), "FM-1 LEDs are resent periodically (the firmware may clear them)"); }
    {   // PLAY / STOP: while recording it only ends the recording, the loop plays on; the next press stops the tape (it slows down, not a cut); PLAY runs it up again
      app.selectTrack(0); app.loopClear(); run(0.3);
      app.loopRecord(); run(0.1); app.playNote(60, true); run(0.15); app.playNote(60, false); app.playNote(64, true); run(0.5); app.playNote(64, false); run(0.3);
      CHECK(app.status().freeRecording, "a take is recording");
      app.transportPlay(); run(0.3);
      CHECK(!app.status().freeRecording && app.loopExists() && app.status().playing, "PLAY/STOP while recording ends the recording and keeps playing");
      const auto recorded = app.status();
      btn(SamplerApp::SEL); if (app.page() != SamplerApp::P_TEMPO) btn(SamplerApp::SEL);
      app.knobs()[0].set(1.f / 31.f); run(0.1);
      const auto twoBars = app.status();
      CHECK(twoBars.loopBars == 2 && twoBars.loopSamples == recorded.loopSamples && std::fabs(twoBars.bpm - 480.0 * sr / twoBars.loopSamples) < 0.01, "bar edit reinterprets the original recording");
      const float requested = 180.f;
      const double expectedRate = std::max(0.25, std::min(4.0, requested / (double)twoBars.bpm));
      app.knobs()[1].set((requested - 30.f) / 270.f); run(0.1);
      const auto edited = app.status();
      CHECK(std::fabs(edited.bpm - twoBars.bpm * expectedRate) < 0.1f && edited.loopSamples == recorded.loopSamples, "tempo edit preserves recorded frames");
      run(0.1); const auto later = app.status();
      const double advanced = std::fmod(later.clipPos[0] - edited.clipPos[0] + 1.0, 1.0);
      const double expectedAdvance = std::fmod((long)(0.1 * sr) / 256 * 256 * expectedRate / edited.loopSamples, 1.0);
      CHECK(std::fabs(advanced - expectedAdvance) < 0.02, "tempo edit changes actual tape rate (%.3f vs %.3f)", advanced, expectedAdvance);
      app.knobs()[2].set(1.f); run(0.1);
      CHECK(app.status().tempoStretch && app.knobs()[2].value == "STRETCH", "global pitch-preserving mode selected");
      app.knobs()[2].set(0.f); run(0.1);
      CHECK(!app.status().tempoStretch, "global tape mode restored");
      s.setLoopBars(recorded.loopBars); run(0.1);
      s.setLoopBpm(240.f * recorded.loopBars * sr / recorded.loopSamples); run(0.1);
      size_t m1 = all.size(); run(1.2); const double playing1 = rmsFrom(m1);
      CHECK(playing1 > 0.003, "the loop plays on (rms %.4f)", playing1);
      app.transportPlay(); run(0.05);
      CHECK(!app.status().playing, "second press: stopped");
      size_t m2 = all.size(); run(0.15); const double slowing = rmsFrom(m2);
      CHECK(slowing > 0.2 * playing1, "...but the tape slows down instead of cutting (rms %.4f vs %.4f)", slowing, playing1);
      run(1.2); size_t m3 = all.size(); run(0.5); CHECK(rmsFrom(m3) < 0.15 * playing1, "...and stands still afterwards (only effect tails: rms %.5f)", rmsFrom(m3));
      app.transportPlay(); run(0.1); size_t m4 = all.size(); run(0.3); const double up = rmsFrom(m4); size_t m5 = all.size(); run(1.5);
      CHECK(app.status().playing && up < rmsFrom(m5) && rmsFrom(m5) > 0.003, "PLAY runs the tape up again (ramp rms %.4f, then %.4f)", up, rmsFrom(m5));
      app.loopClear(); run(0.3); }
    {   // loudness normalisation and auto-tune, on synthetic samples: a quiet and a loud 220 Hz tone
      auto tone = [&](const char* path, float amp, double hz) { std::vector<float> st; for (int i = 0; i < 2 * sr; i++) { const float v = amp * (float)std::sin(2 * M_PI * hz * i / sr) * (i < 100 ? i / 100.f : 1.f); st.push_back(v); st.push_back(v); } writeWav16(path, st, sr); };
      tone("/tmp/fm1sampler_t2/tone_quiet.wav", 0.03f, 220.0); tone("/tmp/fm1sampler_t2/tone_loud.wav", 0.95f, 220.0); tone("/tmp/fm1sampler_t2/tone_flat.wav", 0.5f, 220.0 * std::pow(2.0, 0.3 / 12));
      s.loadSamplePath("/tmp/fm1sampler_t2/tone_quiet.wav", false, 6); s.loadSamplePath("/tmp/fm1sampler_t2/tone_loud.wav", false, 7); run(2.0);
      float pkq = 0, pkl = 0; auto pq = s.samplePeaksOf(6, 64), pl = s.samplePeaksOf(7, 64); for (float v : pq) pkq = std::max(pkq, v); for (float v : pl) pkl = std::max(pkl, v);
      CHECK(pkq > 0.2f && std::fabs(pkq - pkl) < 0.05f * std::max(pkq, pkl) + 0.02f, "loudness normalisation: quiet (%.3f) and loud (%.3f) tones end up equally loud", pkq, pkl);
      {   // a sparse drum hit (a sharp decaying burst, then silence) must not stay quiet next to a sustained tone
        std::vector<float> st; unsigned rs = 1; for (int i = 0; i < 2 * sr; i++) { rs = rs * 1664525u + 1013904223u; const float nz = ((rs >> 8) & 0xffff) / 32768.f - 1.f; const float v = 0.9f * nz * std::exp(-i / (0.04f * sr)); st.push_back(v); st.push_back(v); }
        writeWav16("/tmp/fm1sampler_t2/drum_hit.wav", st, sr); s.loadSamplePath("/tmp/fm1sampler_t2/drum_hit.wav", false, 4); run(1.5);
        auto pd = s.samplePeaksOf(4, 64); float pkd = 0; for (float v : pd) pkd = std::max(pkd, v);
        CHECK(pkd > 0.7f && pkd <= 1.0f, "drum hit: peaks high but limited (%.3f)", pkd); }
      const float m = s.detectRoot(6); CHECK(std::fabs(m - 57.f) < 0.15f, "auto-tune: a 220 Hz tone is detected as A3 = note 57 (%.3f)", m);
      s.loadSamplePath("/tmp/fm1sampler_t2/tone_flat.wav", false, 5); run(1.0);
      const float mf = s.detectRoot(5); CHECK(std::fabs(mf - 57.3f) < 0.15f, "...and a tone 30 cents sharp as 57.3 (%.3f)", mf);
      app.selectTrack(5); app.setAlgo(0); run(0.2); app.autoTune(); run(0.2); CHECK(app.tuned(5), "autoTune marks the track tuned"); app.autoTune(); CHECK(!app.tuned(5), "...and a second tap resets it");
      app.setAlgo(2); app.autoTune(); CHECK(!app.tuned(5), "no auto-tune in LOOPED mode"); app.setAlgo(0); app.selectTrack(0); }
    {   // scrubbing the timeline with a finger: smooth, forward and backward, no clicks; the tape returns to speed when the finger lifts
      app.selectTrack(0); app.loopRecord(); run(0.1); app.playNote(60, true); run(0.8); app.playNote(60, false); app.playNote(67, true); run(0.8); app.playNote(67, false); run(0.2);
      app.transportPlay(); run(0.6);
      auto maxStep = [&](size_t m0) { float mx = 0; for (size_t i = m0 + 2; i < all.size(); i += 2) mx = std::max(mx, std::fabs(all[i] - all[i - 2])); return mx; };
      CHECK(app.loopExists() && app.status().playing, "a loop plays (for the scrub test)");
      size_t b0 = all.size(); run(1.0); const float base = maxStep(b0);
      size_t s0 = all.size(); app.scrub(0, 0.5f); run(0.1);
      for (int k = 0; k < 24; k++) { app.scrub(1, 0.5f + 0.35f * (float)std::sin(k * 0.45)); run(0.04); }
      app.scrub(1, 0.2f); run(0.7); const float at = app.status().clipPos[0];
      CHECK(std::fabs(at - 0.2f) < 0.05f, "the tape follows the finger to its place (%.3f, wanted 0.2)", at);
      app.scrub(2, 0); run(0.8);
      const float sm = maxStep(s0);
      CHECK(sm < 5.f * base + 0.1f, "no clicks while scrubbing (largest step %.3f, normal playback %.3f)", sm, base);
      const float p1 = app.status().clipPos[0]; run(0.3); CHECK(std::fabs(app.status().clipPos[0] - p1) > 0.02f && app.status().playing, "after the finger lifts the loop plays on (%.3f -> %.3f)", p1, app.status().clipPos[0]);
      app.transportPlay(); run(1.2); const float ps = app.status().clipPos[0]; app.scrub(0, 0.6f); run(0.3); app.scrub(2, 0); run(0.8);
      CHECK(!app.status().playing && std::fabs(app.status().clipPos[0] - 0.6f) < 0.06f, "scrubbing a stopped tape moves it, it stays stopped afterwards (%.3f -> %.3f)", ps, app.status().clipPos[0]);
      // The lower tangent runs right, the upper tangent left; the splice
      // is at the large wheel's lower tangent. Both share one path coordinate.
      s.seek(0.25f); run(0.2);
      SamplerUI ui(app); ui.setSize(860, 400, 0, 0); ui.frame(now);
      auto dragTape = [&](float x0, float y0, float x1, float y1) {
          ui.touch(0, x0, y0, now); run(0.1);
          ui.touch(1, x1, y1, now); run(0.8);
          ui.touch(2, x1, y1, now); run(0.4);
          return app.status().clipPos[0];
      };
      const float lower = dragTape(130, 195, 180, 194);
      CHECK(lower > 0.32f && lower < 0.37f, "lower ribbon right advances (%.3f)", lower);
      const float upper = dragTape(190, 124, 140, 112);
      CHECK(upper > lower + 0.07f && upper < lower + 0.13f, "upper ribbon left advances (%.3f)", upper);
      const float back = dragTape(140, 112, 190, 124);
      CHECK(std::fabs(back - lower) < 0.025f, "upper ribbon right reverses (%.3f)", back);
      const float center = dragTape(93, 149, 130, 149);
      CHECK(std::fabs(center - back) < 0.005f, "reel interior does not grab the ribbon (%.3f)", center);
      s.seek(0.04f); run(0.2);
      ui.touch(0, 120, 195, now); run(0.1);
      ui.touch(1, 94, 195, now); run(0.3);
      ui.touch(1, 61, 183, now); run(0.8);
      const float reverse = app.status().clipPos[0];
      CHECK(reverse > 0.85f && reverse < 0.97f, "ribbon crosses the splice backwards (%.3f)", reverse);
      ui.touch(1, 94, 195, now); run(0.3);
      ui.touch(1, 120, 195, now); run(0.8);
      const float forward = app.status().clipPos[0];
      CHECK(std::fabs(forward - 0.04f) < 0.025f, "ribbon crosses the splice forwards (%.3f)", forward);
      ui.touch(2, 120, 195, now); run(0.8);
      CHECK(!app.status().playing && std::fabs(app.status().clipPos[0] - forward) < 0.02f, "ribbon release preserves stopped transport");
      // Compare the digital shuttle with the ordinary linear tape timeline.
      const float delta = (float)(0.08 * sr / s.status().loopSamples);
      s.seek(0.25f); run(0.2);
      app.scrub(0, 0.25f); app.scrub(1, 0.25f + delta); run(0.08);
      const float slowPos = s.status().clipPos[0];
      app.scrub(2, 0); run(0.4);
      s.seek(0.25f); run(0.2);
      ui.setEditor(true); ui.frame(now);
      // The strip is below the sample waveform in the 860 x 400 editor.
      const float sx = 187.5f, sw = 409.f, sy = 259.f;
      ui.touch(0, sx + sw * 0.25f, sy, now);
      ui.touch(1, sx + sw * (0.25f + delta), sy, now); run(0.08);
      const float fastPos = s.status().clipPos[0];
      CHECK(fastPos > slowPos + delta * 0.15f, "editor shuttle follows faster than tape timeline (%.3f vs %.3f)", fastPos, slowPos);
      ui.touch(1, sx + sw * 0.25f, sy, now); run(0.12);
      CHECK(std::fabs(s.status().clipPos[0] - 0.25f) < delta * 0.1f, "editor shuttle reverses to its target");
      ui.touch(1, sx + sw * (0.25f + delta), sy, now); run(0.025);
      ui.touch(2, sx + sw * (0.25f + delta), sy, now); run(0.06);
      const float stopped = s.status().clipPos[0];
      run(0.2);
      CHECK(!s.status().playing && std::fabs(s.status().clipPos[0] - stopped) < 0.001f, "editor shuttle stops promptly on release");
      app.transportSet(true); run(0.6); ui.frame(now);
      ui.touch(0, sx + sw * 0.5f, sy, now); run(0.2);
      ui.touch(2, sx + sw * 0.5f, sy, now); run(0.12);
      const float resumed = s.status().clipPos[0]; run(0.15);
      const float moved = std::fmod(s.status().clipPos[0] - resumed + 1.f, 1.f);
      CHECK(s.status().playing && moved > 0.01f, "editor shuttle resumes playback after release");
      // The workstation transport uses full rate, with a short gain
      // fade; the cassette transport still has its speed ramps.
      auto editorPlay = [&] { ui.frame(now); ui.touch(0, 30, 22, now); ui.touch(2, 30, 22, now); };
      const double L = s.status().loopSamples;
      auto advance = [&](double p) { return std::fmod(s.status().pos - p + L, L); };
      editorPlay(); const double stopAt = s.status().pos;
      run(0.04);
      const double stopDist = advance(stopAt), stoppedAt = s.status().pos;
      CHECK(!s.status().playing && stopDist < 0.02 * sr * s.status().tempoRate, "digital stop has no tape deceleration");
      run(0.1);
      CHECK(std::fabs(s.status().pos - stoppedAt) < 1.0, "digital stop holds position");
      s.setLoopSpeed(1.f); s.seek(0.25f); run(0.1);
      const double start = s.status().pos;
      const double expected = (long)(0.04 * sr) / 256 * 256 * s.status().tempoRate;
      editorPlay(); run(0.04);
      CHECK(s.status().playing && std::fabs(advance(start) - expected) < 2.0, "editor starts at configured speed");
      btn(SamplerApp::PLAY); run(0.04);
      const double hardwareStop = s.status().pos; run(0.1);
      CHECK(!s.status().playing && std::fabs(s.status().pos - hardwareStop) < 1.0, "hardware PLAY uses digital transport in editor");
      ui.setEditor(false); s.seek(0.25f); run(0.1);
      const double tapeStart = s.status().pos;
      btn(SamplerApp::PLAY); run(0.04);
      CHECK(s.status().playing && advance(tapeStart) < expected * 0.2, "cassette start retains acceleration");
      btn(SamplerApp::PLAY); run(0.8);
      app.loopClear(); run(0.3); }
    {   // the first take can be drawn while it is recorded
      app.selectTrack(0); app.loopClear(); run(0.3); app.loopRecord(); run(0.1); app.playNote(60, true); run(0.8); app.playNote(60, false); run(0.3);
      float fp[64]; const int fn = s.freePeaks(fp, 64); float fm = 0; for (int i = 0; i < fn; i++) fm = std::max(fm, fp[i]);
      CHECK(app.status().freeRecording && fn >= 15 && fm > 0.05f, "the first take has peaks while recording (%d bins, max %.3f)", fn, fm);
      app.loopRecord(); run(0.3); app.loopClear(); run(0.3); }
    // microphone sample
    btn(SamplerApp::SAVE);
    enc(2, 1); run(0.3); CHECK(app.micState() == 1, "mic sampling waits for sound");
    { double ph = 0; for (long i = 0; i < (long)(1.0 * sr) / 256; i++) { for (int k = 0; k < 256; k++) { float v = 0.4f * std::sin(ph); ph += 2 * M_PI * 440.0 / sr; in[2 * k] = in[2 * k + 1] = v; } std::fill(buf.begin(), buf.end(), 0.f); s.process(buf.data(), in.data(), 256); std::this_thread::sleep_for(std::chrono::microseconds(200)); if (i % 5 == 0) { now += 33; app.tick(now); } } std::fill(in.begin(), in.end(), 0.f); }
    CHECK(app.micState() == 2, "mic sampling recording");
    enc(2, -1); run(3.0);
    CHECK(app.micState() == 0 && app.library().back().group == "User", "mic sample saved into the library and loaded (%s)", app.sampleLabel(0).c_str());
    // import from disk: a WAV dropped into the data dir
    { std::ifstream src(std::string(getenv("HOME")) + "/fm1-sampler/samples/Bass/Fuzz.wav", std::ios::binary); std::ofstream dst("/tmp/fm1sampler_t2/sample_FromDisk.wav", std::ios::binary); dst << src.rdbuf(); }
    app.selectTrack(4); app.importFile("/tmp/fm1sampler_t2/sample_FromDisk.wav"); run(2.0);
    CHECK(app.hasSample(4) && app.sampleLabel(4) == "sample_FromDisk", "import from disk loads into the selected track (%s)", app.sampleLabel(4).c_str());
    // presets audition
    app.selectTrack(4); size_t pm = all.size(); enc(6, 1); run(1.5); CHECK(rmsFrom(pm) > 0.002, "browsing plays the sample (rms %.4f)", rmsFrom(pm));
    { // High-rate Paulstretch must consume the same sequence as a decimated 1x stream.
      std::vector<float> src(8192);
      for (int i = 0; i < 4096; ++i) src[2*i] = src[2*i+1] = 0.4f * std::sin(i * 0.03f);
      PaulStream fast, reference;
      fast.start(src.data(), 4096, 0, 4096, 8.f); reference.start(src.data(), 4096, 0, 4096, 8.f);
      bool same = true;
      for (int i = 0; i < 4096; ++i) {
          float l, r, a, b; fast.next(17.0, l, r); reference.next(1.0, a, b);
          same &= std::isfinite(l) && std::isfinite(r) && std::fabs(l-a) < 1e-6f && std::fabs(r-b) < 1e-6f;
          for (int j = 1; j < 17; ++j) reference.next(1.0, a, b);
      }
      CHECK(same, "Paulstretch high-rate FIFO matches reference"); }
    { // Circular stretching preserves a sinusoid's pitch while the nominal head advances faster or slower.
      const int L = sr * 2;
      std::vector<float> clip(2 * L);
      for (int i = 0; i < L; ++i) clip[2*i] = clip[2*i+1] = std::sin(2 * M_PI * 440 * i / sr);
      for (double speed : {0.5, 1.5, 2.0}) {
          LoopStretch heads(sr);
          float last = 0; int crossings = 0;
          for (int i = 0; i < 2 * sr; ++i) {
              const auto f = heads.next(LoopStretch::wrap(i * speed, L), 1.0, L, &clip);
              const float a = LoopStretch::read(clip, L, f.a, 0), b = LoopStretch::read(clip, L, f.b, 0);
              const float v = a + (b - a) * f.blend;
              if (i > sr && (v > 0) != (last > 0)) ++crossings;
              last = v;
          }
          CHECK(std::abs(crossings / 2.0 - 440) < 12, "loop stretch %.1fx keeps tone near 440 Hz (%.1f)", speed, crossings / 2.0);
      } }
    // DSP units: offline stretch / pitch and the slicer
    {
        const int N = 44100; std::vector<float> sine((size_t)N * 2);
        for (int i = 0; i < N; i++) sine[(size_t)i * 2] = sine[(size_t)i * 2 + 1] = 0.5f * std::sin(2 * M_PI * 440.0 * i / 44100.0);
        auto freq = [&](const std::vector<float>& v) { int zc = 0; const size_t n = v.size() / 2; for (size_t i = 1; i < n; i++) if ((v[i * 2] > 0) != (v[(i - 1) * 2] > 0)) zc++; return zc / 2.0 / (n / 44100.0); };
        auto fast = stretchPitch(sine, 44100, 2.0, 0), up = stretchPitch(sine, 44100, 1.0, 12), slow = stretchPitch(sine, 44100, 0.5, 0);
        CHECK(std::fabs(fast.size() / 2.0 - N / 2.0) < 4000 && std::fabs(freq(fast) - 440) < 15, "stretch 2x: %.2fs at %.0f Hz", fast.size() / 2.0 / 44100, freq(fast));
        CHECK(std::fabs(slow.size() / 2.0 - N * 2.0) < 6000 && std::fabs(freq(slow) - 440) < 15, "stretch 0.5x: %.2fs at %.0f Hz", slow.size() / 2.0 / 44100, freq(slow));
        CHECK(std::fabs(up.size() / 2.0 - N) < 100 && std::fabs(freq(up) - 880) < 20, "pitch +12: %.2fs at %.0f Hz", up.size() / 2.0 / 44100, freq(up));
        std::vector<float> hits((size_t)N * 2 * 2, 0.f);   // three clicks with decays
        for (int h : {2000, 30000, 60000}) for (int i = 0; i < 6000; i++) hits[(size_t)(h + i) * 2] = hits[(size_t)(h + i) * 2 + 1] = std::exp(-i / 800.f) * std::sin(i * 0.3f);
        auto sl = findSlices(hits, N * 2, 44100, 16);
        auto near = [&](int t) { for (int v : sl) if (std::abs(v - t) < 600) return true; return false; };
        CHECK(near(2000) && near(30000) && near(60000) && sl.size() <= 5, "slicer finds the three hits (%zu slices)", sl.size());
        auto on = findOnsets(hits, N * 2, 44100);
        auto lo = pickSlices(on, 0, N * 2, 44100, 0.0f), hi = pickSlices(on, 0, N * 2, 44100, 1.0f), win = pickSlices(on, 20000, N * 2, 44100, 0.2f);
        CHECK(lo.size() >= hi.size() && !win.empty() && win[0] >= 20000, "threshold: %zu slices at 0, %zu at 1, window starts at %d", lo.size(), hi.size(), win.empty() ? -1 : win[0]);
        auto eq = findSlices(std::vector<float>((size_t)N * 2, 0.1f), N, 44100, 16);
        CHECK(eq.size() == 16, "no hits: %zu equal slices", eq.size());
    }
    {   // sidechain: track 2 ducked by track 1, send effects return something
        app.loopClear(); run(0.3);
        for (int tr : {0, 1}) { app.selectTrack(tr); app.setAlgo(2); }
        auto level = [&]() { app.selectTrack(1); app.playNote(60, true); run(0.5); app.selectTrack(0); app.playNote(60, true); run(0.9); float pk = 0; for (int i = 0; i < 20; i++) { run(0.03); pk = std::max(pk, app.status().track_peak[1]); } app.playNote(60, false); app.selectTrack(1); app.playNote(60, false); run(0.8); return pk; };
        app.selectTrack(1); { auto k = app.knobs(SamplerApp::P_MIX); k[1].set(0.f); } const float open = level();
        { auto k = app.knobs(SamplerApp::P_MIX); k[1].set(1.f / 8.f); k = app.knobs(SamplerApp::P_MIX); k[2].set(0.1f); k = app.knobs(SamplerApp::P_MIX); k[3].set(1.f); } run(0.2);
        const float ducked = level();
        CHECK(open > 0.01f && ducked < open * 0.6f, "sidechain: track 2 ducked by track 1 (%.3f -> %.3f)", open, ducked);
        { auto k = app.knobs(SamplerApp::P_MIX); k[1].set(0.f); }
        app.selectTrack(1); app.setAlgo(0); app.selectTrack(0); app.setAlgo(0);
    }
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL OK", fails);
    return fails ? 1 : 0;
}
