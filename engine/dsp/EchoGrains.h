// SPDX-License-Identifier: MIT
// Adapted from ECHOFIELD FM-1, Copyright (c) 2026 Philip Bos.
#pragma once
#include <array>
#include <cstdint>

struct EchoParams {
    enum Mode { Cloud, Dust, Tape, Bloom, Pulse, Drift, ModeCount };
    int mode = Cloud;
    float position = 0.35f, texture = 0.72f, scatter = 0.45f, drift = 0.35f, evolve = 0.35f;
};

// One note's independently scheduled grains. All storage is fixed-size;
// the caller owns the PCM source and enforces a shared track grain budget.
class EchoGrains {
public:
    static constexpr int kGrains = 20;
    static constexpr int kTrackCap = 32;
    void seed(uint32_t value);
    void next(const EchoParams& params, const float* pcm, int frames, int sourceRate,
              int outputRate, double start, double end, float semitones,
              int& trackGrains, float& left, float& right);
    int count() const;
    int positions(double* pos, float* level, int max) const;
private:
    enum Window { Hann, Tukey, Perc, Swell, Gauss, WindowCount };
    struct Control {
        float size = 0.12f, density = 0, jitter = 0, center = 0, scatter = 0;
        float pitchRand = 0, reverse = 0, width = 0.85f, amp = 1, tone = -0.1f, toneRand = 0;
        float tapeSpeed = 1;
        int window = Hann;
        bool tape = false, clocked = false;
    };
    struct Grain {
        bool active = false;
        double pos = 0, step = 0;
        int age = 0, len = 0, window = Hann;
        float gainL = 0, gainR = 0, a = 0, zL = 0, zR = 0;
        int tone = 0;
    };
    std::array<Grain, kGrains> grains_{};
    EchoParams sm_{};
    Control c_{};
    uint32_t rng_ = 0x1234567u;
    int untilControl_ = 0, tapeHeadIndex_ = 0, pulseStep_ = 0, pulseRot_ = 0, stutterLeft_ = 0;
    bool initial_ = true;
    float spawnWait_ = 0, pulseWait_ = 0, stutterPos_ = 0;
    float lfo1_ = 0, lfo2Time_ = 0, lfo2Target_ = 0, lfo2Cur_ = 0;
    float walkV1_ = 0, walkV2_ = 0, walkPos_ = 0, walkPitch_ = 0, walkSize_ = 0;
    float tapeHead_ = 0, tapeSpeed_ = 1, tapeStop_ = 0;
    std::array<float, 16> stepPos_{};
    std::array<int, 16> stepOct_{};
    float random();
    float bipolar() { return 2 * random() - 1; }
    void control(const EchoParams& params, float dt, double regionSeconds);
    void spawn(int sr, int sourceRate, double start, double end, float semitones,
               int& trackGrains, float ampScale = 1, float extraPitch = 0, float overridePos = -1);
    static float window(int kind, float phase);
};
