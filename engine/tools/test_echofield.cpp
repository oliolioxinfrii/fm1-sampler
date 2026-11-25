#include "EchoGrains.h"
#include "Wav.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int sr = 48000, sourceRate = 44100, frames = 1200000, renderFrames = 3 * sr;
constexpr double start = 1050000, end = 1199998;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::vector<float> render(const std::vector<float>& pcm, EchoParams p, uint32_t seed, int& peakGrains) {
    EchoGrains engine; engine.seed(seed);
    std::vector<float> out(2 * renderFrames);
    int count = 0;
    peakGrains = 0;
    for (int i = 0; i < renderFrames; ++i) {
        float l, r;
        engine.next(p, pcm.data(), frames, sourceRate, sr, start, end, 0.f, count, l, r);
        require(std::isfinite(l) && std::isfinite(r), "nonfinite output");
        require(count == engine.count() && count >= 0 && count <= EchoGrains::kGrains, "invalid grain count");
        if (i % 257 == 0) {
            double pos[EchoGrains::kGrains]; float level[EchoGrains::kGrains];
            const int n = engine.positions(pos, level, EchoGrains::kGrains);
            require(n == count, "grain visualization count mismatch");
            for (int j = 0; j < n; ++j) {
                require(pos[j] >= start - 1 && pos[j] <= end + 1, "grain position escaped long region");
                require(level[j] >= 0 && level[j] <= 1.00001f, "invalid grain envelope");
            }
        }
        peakGrains = std::max(peakGrains, count);
        out[2 * i] = l; out[2 * i + 1] = r;
    }
    return out;
}
}

int main(int argc, char** argv) {
    try {
        const char* names[] = {"cloud", "dust", "tape", "bloom", "pulse", "drift"};
        std::vector<float> pcm(2 * frames);
        // The prefix is silent. The selected region begins beyond Q20.12's
        // address range and has independent stereo content.
        for (int i = int(start); i < frames; ++i) {
            const double t = double(i - start) / sourceRate;
            pcm[2 * i] = 0.16f * float(std::sin(6.283185307179586 * (180 * t + 4 * t * t)));
            pcm[2 * i + 1] = 0.12f * float(std::sin(6.283185307179586 * (237 * t + 9 * t * t)));
        }
        if (argc > 2) throw std::runtime_error("usage: test_echofield [WAV output directory]");
        if (argc == 2) std::filesystem::create_directories(argv[1]);
        EchoParams p;
        std::vector<float> cloud;
        for (int mode = 0; mode < EchoParams::ModeCount; ++mode) {
            p.mode = mode;
            int peak;
            auto out = render(pcm, p, 12345, peak);
            double energy = 0, side = 0;
            for (int i = 0; i < renderFrames; ++i) {
                const double l = out[2 * i], r = out[2 * i + 1];
                energy += l*l + r*r; side += (l-r)*(l-r);
            }
            const double rms = std::sqrt(energy / (2 * renderFrames));
            require(rms > 0.0001 && side > 1e-5 && peak > 0, "silent or non-stereo mode");
            if (mode == EchoParams::Cloud) cloud = out;
            else require(out != cloud, "mode does not differ from CLOUD");
            if (argc == 2)
                require(writeWav16(std::string(argv[1]) + "/" + names[mode] + ".wav", out, sr), "WAV write failed");
            std::printf("PASS %s: RMS %.5f, peak %d grains\n", names[mode], rms, peak);
        }
        p.mode = EchoParams::Cloud;
        int peak;
        require(render(pcm, p, 12345, peak) == cloud, "seeded render is not reproducible");
        p.texture = 0.15f; p.scatter = 0.9f; p.drift = 0.8f; p.evolve = 0.95f;
        require(render(pcm, p, 12345, peak) != cloud, "parameter changes did not affect audio");
        EchoGrains voices[3];
        int count = 0;
        p.texture = 1.f;
        for (int i = 0; i < 3; ++i) voices[i].seed(42 + i);
        for (int i = 0; i < renderFrames; ++i) {
            for (auto& v : voices) {
                float l, r;
                v.next(p, pcm.data(), frames, sourceRate, sr, start, end, 0, count, l, r);
            }
            require(count >= 0 && count <= EchoGrains::kTrackCap &&
                    count == voices[0].count() + voices[1].count() + voices[2].count(), "polyphonic track budget exceeded");
        }
        // Moving the region of a sustained note must not abruptly cut
        // existing grains. New grains are confined to the new window.
        EchoGrains moving; moving.seed(71);
        EchoParams q; q.freeze=1; q.position=.5f; q.scatter=0; q.drift=0; q.evolve=0;
        int active=0; float l=0,r=0;
        const double leftEnd=start+50000,rightStart=start+75000;
        moving.next(q,pcm.data(),frames,sourceRate,sr,start,leftEnd,0,active,l,r);
        require(active>0,"range test did not spawn initial grains");
        const int before=active;
        q.position=0;
        moving.next(q,pcm.data(),frames,sourceRate,sr,rightStart,end,0,active,l,r);
        require(active>=before,"region edit cut sounding grains");
        for(int i=0;i<4*sr;++i){
            moving.next(q,pcm.data(),frames,sourceRate,sr,rightStart,end,0,active,l,r);
            require(std::isfinite(l)&&std::isfinite(r)&&active==moving.count(),"invalid audio or count after region edit");
        }
        double pos[EchoGrains::kGrains];float amp[EchoGrains::kGrains];
        const int finalCount=moving.positions(pos,amp,EchoGrains::kGrains);
        require(finalCount>0,"region edit stopped granular synthesis");
        for(int i=0;i<finalCount;++i)require(pos[i]>=rightStart-1&&pos[i]<=end+1,"new grain outside edited region");
        std::puts("PASS deterministic render, parameters, long PCM addressing, shared grain budget, live region edits");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
