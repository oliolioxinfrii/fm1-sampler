#include "Sampler.h"
#include "Wav.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

static void require(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

int main() {
    const int sr = SAMPLERATE, n = 256;
    const auto path = std::filesystem::temp_directory_path() /
        ("fm1-voice-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove(path, ec); }
    } cleanup{path};
    try {
        std::vector<float> wav(2 * sr);
        for (int i = 0; i < sr; ++i) wav[2*i] = wav[2*i+1] = 0.4f * std::sin(2.f*kPi*220.f*i/sr);
        require(writeWav16(path.string(), wav, sr), "write test sample");
        Sampler reference(sr), edited(sr);
        require(reference.load(path.string()) && edited.load(path.string()), "load test sample");
        auto processed = std::make_shared<SampleBuf>();
        processed->frames = sr; processed->sr = sr; processed->d.assign(2 * sr, 0.4f);
        reference.setProcessed(processed, 0.f, 1.f);
        edited.setProcessed(processed, 0.f, 1.f);
        reference.setMode(Sampler::LOOPED); edited.setMode(Sampler::LOOPED);
        reference.noteOn(60, 100); edited.noteOn(60, 100);
        std::vector<float> a(2*n), b(2*n);
        auto equalBlock = [&] {
            std::fill(a.begin(), a.end(), 0.f); std::fill(b.begin(), b.end(), 0.f);
            reference.render(a.data(), n); edited.render(b.data(), n);
            require(a == b, "active voice changed after an edit");
        };
        for (int i = 0; i < 10; ++i) equalBlock();
        edited.setRange(0.2f, 0.8f);
        edited.setRoot(64.f);
        edited.setMode(Sampler::ONESHOT);
        edited.setProcessed(nullptr, 0.f, 1.f);
        for (int i = 0; i < 10; ++i) equalBlock();
        require(edited.hasVoices(), "edit terminated held voice");
        require(!edited.takeUnusedProcessed(), "buffer reclaimed while still referenced");
        reference.noteOff(60); edited.noteOff(60);
        for (int i = 0; i < 100; ++i) equalBlock();
        require(!edited.hasVoices(), "old looped voice did not release");
        reference.allOff(); reference.setProcessed(nullptr, 0.f, 1.f);
        auto referenceRetired = reference.takeUnusedProcessed();
        referenceRetired.reset(); processed.reset();
        require(bool(edited.takeUnusedProcessed()), "unused processed buffer not reclaimed");

        edited.noteOn(60, 100);
        edited.noteOff(60);
        std::fill(b.begin(), b.end(), 0.f);
        edited.render(b.data(), n);
        require(edited.hasVoices(), "new one-shot voice obeyed old mode");
        float pos[1];
        require(edited.voicePositions(pos, 1) == 1 && pos[0] > 0.2f && pos[0] < 0.3f,
                "new voice did not use edited window");
        std::puts("voice continuity tests passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
