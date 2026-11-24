// SPDX-License-Identifier: MIT
// Adapted from ECHOFIELD FM-1, Copyright (c) 2026 Philip Bos.
// See echofield/UPSTREAM_COMMIT for the original effect topology.
#pragma once
#include <array>
#include <vector>
#include <cstdint>

struct EchoInsertParams {
    int filter = 0; // low, band, high pass
    float cutoff = .82f, resonance = .10f, drive = .05f, filterMix = 0;
    float tape = 0, wow = .4f, flutter = .25f, age = .25f, hiss = 0;
    float width = .75f, level = .790569415f; // unity at defaults
};
struct EchoReturnParams {
    float time = .52f, feedback = .45f, size = .78f, damp = .5f, shimmer = .22f;
    int ping = 1;
};

class EchoInsert {
public:
    explicit EchoInsert(int sr);
    void set(const EchoInsertParams& p) { target_ = p; }
    void process(float* stereo, int n);
private:
    int sr_;
    EchoInsertParams target_, smooth_;
    std::vector<float> tape_[2];
    int wr_ = 0;
    uint32_t rng_ = 12345;
    float wowPh_ = 0, flutPh_ = 0, wowNoise_ = 0, flutNoise_ = 0, ageTimer_ = 0, ageGain_ = 1;
    float tone_[2]{}, hiss_[2]{}, sv1_[2]{}, sv2_[2]{};
    float rnd();
};

class EchoReturns {
public:
    explicit EchoReturns(int sr);
    void set(const EchoReturnParams& p) { target_ = p; }
    // Independent, wet-only sends; returns are added to the master bus.
    void process(const float* echo, const float* room, float* out, int n);
private:
    struct Ring {
        std::vector<float> data;
        int w = 0;
        float read(float delay) const;
        void push(float value);
    };
    int sr_;
    EchoReturnParams target_, smooth_;
    Ring delay_[2], ap_[4], fdn_[8], shift_;
    int length_[8]{};
    float dlp_[2]{}, damp_[8]{}, phase1_ = 0, phase2_ = 0, shiftPhase_ = 0;
    float shimLP_ = 0, shimX_ = 0, shimY_ = 0, shimFB_ = 0;
};
