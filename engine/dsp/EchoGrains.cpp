// SPDX-License-Identifier: MIT
// Adapted from ECHOFIELD FM-1, Copyright (c) 2026 Philip Bos.
// Mode equations, stochastic scheduling, grain windows, and birth-time
// pitch/colour/pan derive from app/dsp/engine.c (see echofield/UPSTREAM_COMMIT).
#include "EchoGrains.h"
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {
constexpr float pi = 3.14159265358979f, twopi = 2 * pi;
constexpr int kControl = 64, kWindowN = 256;
float clip(float x, float a, float b) { return std::clamp(x, a, b); }
float unit(float x) { return std::isfinite(x) ? clip(x, 0.f, 1.f) : 0.f; }
float wrap(float x) { return x - std::floor(x); }
float mix(float a, float b, float t) { return a + (b - a) * t; }
float semis(float x) { return std::exp2(x / 12.f); }
float shape(int w, float x) {
    switch (w) {
    default: case 0: return 0.5f - 0.5f * std::cos(twopi * x);
    case 1:
        if (x < 0.15f) return 0.5f - 0.5f * std::cos(twopi * x / 0.3f);
        if (x > 0.85f) return 0.5f - 0.5f * std::cos(twopi * (1.f - x) / 0.3f);
        return 1;
    case 2: {
        float v = x < 0.03f ? std::sin(0.5f * pi * x / 0.03f) : std::exp(-5.f * (x - 0.03f) / 0.97f);
        if (x > 0.92f) v *= 0.5f + 0.5f * std::cos(pi * (x - 0.92f) / 0.08f);
        return v;
    }
    case 3: return shape(2, 1.f - x);
    case 4: {
        float d = (x - 0.5f) / 0.16f;
        return clip((std::exp(-0.5f * d * d) - 0.00752f) / (1.f - 0.00752f), 0.f, 1.f);
    }
    }
}
// Immutable lookup tables are initialized before audio starts.
const auto windows = [] {
    std::array<std::array<float, kWindowN + 1>, 5> table{};
    for (int w = 0; w < 5; ++w) {
        for (int i = 1; i < kWindowN; ++i) table[w][i] = shape(w, float(i) / kWindowN);
    }
    return table;
}();
}

float EchoGrains::window(int kind, float phase) {
    const float x = clip(phase, 0.f, 1.f) * kWindowN;
    const int i = std::min(kWindowN - 1, int(x));
    return mix(windows[kind][i], windows[kind][i + 1], x - i);
}
float EchoGrains::random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return float(rng_ >> 8) * (1.f / 16777216.f);
}
void EchoGrains::seed(uint32_t value) {
    rng_ = value ? value : 0x1234567u;
    for (auto& p : stepPos_) p = random();
    for (auto& o : stepOct_) o = 0;
}

void EchoGrains::control(const EchoParams& p, float dt, double regionSeconds) {
    const float alpha = initial_ ? 1.f : 1.f - std::exp(-dt / 0.04f);
    const int previousMode = sm_.mode;
    const float previousPosition = sm_.position;
    sm_.mode = std::clamp(p.mode, 0, int(EchoParams::ModeCount) - 1);
    sm_.position = mix(sm_.position, unit(p.position), alpha);
    sm_.texture = mix(sm_.texture, unit(p.texture), alpha);
    sm_.scatter = mix(sm_.scatter, unit(p.scatter), alpha);
    sm_.drift = mix(sm_.drift, unit(p.drift), alpha);
    sm_.evolve = mix(sm_.evolve, unit(p.evolve), alpha);
    sm_.freeze = p.freeze != 0; sm_.window = std::clamp(p.window,0,5);
    sm_.size = mix(sm_.size,unit(p.size),alpha); sm_.density = mix(sm_.density,unit(p.density),alpha);
    sm_.scan = mix(sm_.scan,unit(p.scan),alpha); sm_.reverse = mix(sm_.reverse,unit(p.reverse),alpha);
    sm_.pitchRandom = mix(sm_.pitchRandom,unit(p.pitchRandom),alpha);
    const float T = sm_.texture, S = sm_.scatter, D = sm_.drift, M = sm_.evolve;
    if (!sm_.freeze) {
    lfo1_ = wrap(lfo1_ + dt * (0.03f + 0.3f * M));
    lfo2Time_ += dt * (0.1f + 1.5f * M);
    if (lfo2Time_ >= 1.f) { lfo2Time_ -= 1.f; lfo2Target_ = bipolar(); }
    lfo2Cur_ = mix(lfo2Cur_, lfo2Target_, std::min(1.f, dt * (0.5f + 3.f * M)));
    const float wr = (sm_.mode == EchoParams::Drift ? 0.25f + 1.2f * M : 0.08f + 0.4f * M) * dt;
    walkV1_ += bipolar() * wr * 2.f - walkV1_ * wr * 2.f;
    walkPos_ = clip(walkPos_ + walkV1_ * wr * 4.f - walkPos_ * wr * 0.3f, -1.f, 1.f);
    walkV2_ += bipolar() * wr * 2.f - walkV2_ * wr * 2.f;
    walkPitch_ = clip(walkPitch_ + walkV2_ * wr * 4.f - walkPitch_ * wr * 0.3f, -1.f, 1.f);
    walkSize_ = mix(walkSize_, bipolar(), dt * 0.3f);
        scan_ = wrap(scan_ + (2*sm_.scan-1)*dt/float(std::max(.05,regionSeconds)));
    }
    const float lfo = std::sin(twopi * lfo1_), pos = sm_.position, wander = D * 0.3f * walkPos_;
    c_ = Control{};
    c_.pitchRand = 0.08f + D * 0.3f;
    c_.reverse = sm_.reverse;
    switch (sm_.mode) {
    default: case EchoParams::Cloud:
        c_.size = 0.12f * std::pow(1500.f / 120.f, T);
        c_.density = (4.f + 8.f * T) / c_.size * (1.f + 0.4f * M * lfo2Cur_);
        c_.jitter = 0.4f + 0.6f * S;
        c_.center = pos + 0.15f * M * lfo + wander;
        c_.scatter = 0.03f + 0.6f * std::pow(S, 1.3f);
        c_.reverse += S * 0.1f;
        break;
    case EchoParams::Dust:
        c_.size = 0.02f * std::pow(200.f / 20.f, T);
        c_.density = (6.f + 24.f * (1.f - T * 0.5f)) * (0.5f + S);
        c_.jitter = 0.7f + 0.3f * S;
        c_.center = pos + 0.1f * M * lfo + wander;
        c_.scatter = 0.1f + 0.85f * S;
        c_.reverse += 0.2f + 0.3f * S;
        c_.window = Gauss; c_.amp = 0.85f; c_.tone += 0.25f; c_.toneRand = 0.3f;
        c_.width = std::min(1.f, c_.width * 1.3f);
        break;
    case EchoParams::Tape: {
        float target = mix(1.f, 0.3f, std::pow(T, 1.2f)) * (1.f + 0.03f * M * lfo2Cur_ + 0.02f * D * walkPitch_);
        if (!sm_.freeze && tapeStop_ <= 0 && random() < M * 0.02f * dt) tapeStop_ = 1.6f;
        if (!sm_.freeze && tapeStop_ > 0) {
            tapeStop_ -= dt;
            const float ph = 1.6f - tapeStop_;
            target *= ph < 0.8f ? 1.f - ph / 0.8f : (ph - 0.8f) / 0.8f;
            target = std::max(target, 0.02f);
        }
        tapeSpeed_ = mix(tapeSpeed_, target, std::min(1.f, dt * 6.f));
        if (initial_ || previousMode != EchoParams::Tape) tapeHead_ = pos;
        else tapeHead_ = wrap(tapeHead_ + pos - previousPosition);
        if (!sm_.freeze) tapeHead_ = wrap(tapeHead_ + tapeSpeed_ * dt / float(std::max(0.005, regionSeconds)));
        c_.tape = true; c_.tapeSpeed = tapeSpeed_;
        c_.size = 0.18f * (1.f + T); c_.density = 4.f / c_.size;
        c_.jitter = 0.15f; c_.center = tapeHead_; c_.scatter = 0.002f + S * 0.03f;
        c_.pitchRand = 0; c_.width *= 0.8f;
        break;
    }
    case EchoParams::Bloom:
        c_.size = 0.6f * std::pow(2.5f / 0.6f, T);
        c_.density = (3.f + 3.f * T) / c_.size * (1.f + 0.5f * M * lfo2Cur_);
        c_.jitter = 0.9f; c_.center = pos + 0.2f * M * lfo + wander;
        c_.scatter = 0.08f + 0.5f * S; c_.reverse += 0.25f;
        c_.tone += 0.15f; c_.toneRand = 0.25f; c_.window = random() < 0.5f ? Hann : Gauss;
        c_.amp = 0.95f;
        break;
    case EchoParams::Pulse:
        c_.clocked = true; c_.size = 0.06f * std::pow(1200.f / 60.f, T);
        c_.center = pos + wander; c_.scatter = S;
        c_.window = T > 0.6f ? Hann : Perc;
        break;
    case EchoParams::Drift:
        c_.size = 0.1f * std::pow(12.f, T) * (1.f + 0.5f * walkSize_);
        c_.density = (2.f + 4.f * T) * (1.f + 0.5f * lfo) / c_.size;
        c_.jitter = 0.8f; c_.center = pos + walkPos_ * (0.1f + 0.5f * S);
        c_.scatter = 0.03f + 0.3f * S; c_.pitchRand = D * 0.5f;
        c_.reverse += 0.15f; c_.window = Gauss;
        break;
    }
    c_.size *= std::exp2(4*sm_.size-2); c_.density *= std::exp2(4*sm_.density-2);
    c_.pitchRand += 12*sm_.pitchRandom;
    if (sm_.window) c_.window = sm_.window-1;
    c_.center += scan_;
    c_.reverse = unit(c_.reverse); c_.scatter = unit(c_.scatter);
    c_.size = clip(c_.size, 0.004f, 2.5f); c_.density = clip(c_.density, 0.f, 400.f);
    if(c_.center!=1.f)c_.center = wrap(c_.center);
}

void EchoGrains::spawn(int sr, int sourceRate, double start, double end, float pitch,
                       int& trackGrains, float ampScale, float extraPitch, float overridePos) {
    if (trackGrains >= kTrackCap || end - start < 32) return;
    Grain* g = nullptr;
    for (auto& x : grains_) if (!x.active) { g = &x; break; }
    if (!g) return;
    const double room = end - start;
    const float size = c_.size * (1.f + bipolar() * 0.3f * 0.7f * (c_.tape ? 0.3f : 1.f));
    int len = std::max(64, int(std::max(size, 0.003f) * sr));
    float harmony = 0;
    if (random() < 0.30f * 0.65f) {
        static constexpr float iv[] = {12, 12, -12, 7, 19, 24, -5, 7};
        harmony = iv[std::min(7, int(random() * 8))]; ampScale *= 0.7f;
    }
    int head = 0;
    double step;
    if (c_.tape) {
        static constexpr float rates[] = {1, 0.5f, 2}, amps[] = {1, 0.8f, 0.4f};
        head = tapeHeadIndex_; tapeHeadIndex_ = (tapeHeadIndex_ + 1) % 3;
        step = c_.tapeSpeed * rates[head] * semis(clip(pitch + harmony, -48.f, 36.f));
        ampScale *= amps[head];
    } else {
        const float st = clip(pitch + harmony + extraPitch + bipolar() * c_.pitchRand + walkPitch_ * sm_.drift * 2.f, -48.f, 36.f);
        step = semis(st);
    }
    step = std::clamp(step * double(sourceRate) / sr, 1.0 / 64, 8.0);
    len = std::min(len, int(std::min(double(len), room * 0.98 / step)));
    if (len < 32) return;
    const double span = len * step;
    const bool rev = random() < c_.reverse;
    float center = overridePos >= 0 ? overridePos : c_.center + bipolar() * c_.scatter * 0.5f;
    if (c_.tape) center = c_.center + float(head) / 3.f + bipolar() * c_.scatter;
    const double cp = start + (center==1.f?1.f:wrap(center)) * room;
    const double p = rev ? std::clamp(cp + span * 0.5, start + span, end)
                         : std::clamp(cp - span * 0.5, start, end - span);
    float pan = 0.5f + bipolar() * 0.5f * c_.width;
    if (c_.tape) pan = unit(0.5f + (head == 1 ? -0.35f : head == 2 ? 0.35f : 0) * c_.width + bipolar() * 0.1f);
    const float amp = c_.amp * ampScale * 0.75f / std::sqrt(std::max(1.f, c_.density * c_.size));
    const float tone = clip(c_.tone + bipolar() * c_.toneRand, -1.f, 1.f);
    *g = Grain{};
    g->active = true; g->pos = p; g->step = rev ? -step : step; g->len = len; g->window = c_.window;
    g->gainL = clip(amp * std::sqrt(2.f * (1.f - pan)), 0.f, 1.9f);
    g->gainR = clip(amp * std::sqrt(2.f * pan), 0.f, 1.9f);
    if (std::abs(tone) >= 0.02f) {
        g->tone = tone < 0 ? 1 : 2;
        const float hz = tone < 0 ? 18000.f * std::pow(0.02f, -tone) : 150.f * std::pow(30.f, tone);
        g->a = 1.f - std::exp(-twopi * std::min(hz, 0.45f * sr) / sr);
    }
    ++trackGrains;
}

void EchoGrains::next(const EchoParams& params, const float* pcm, int frames, int sourceRate,
                      int sr, double start, double end, float pitch,
                      int& trackGrains, float& l, float& r) {
    l = r = 0;
    if (!pcm || frames < 2 || sr <= 0 || sourceRate <= 0) return;
    start = std::clamp(start, 0.0, double(frames - 2));
    end = std::clamp(end, start, double(frames - 2));
    if(!initial_&&(start!=previousStart_||end!=previousEnd_)&&end>start){
        auto remap=[&](float p){return unit(float((previousStart_+p*(previousEnd_-previousStart_)-start)/(end-start)));};
        sm_.position=remap(sm_.position);
        tapeHead_=remap(tapeHead_);
        untilControl_=0;
    }
    previousStart_=start;previousEnd_=end;
    if (untilControl_-- <= 0) {
        control(params, float(kControl) / sr, (end - start) / sourceRate);
        untilControl_ = kControl - 1;
        if (initial_ && !c_.clocked) {
            spawn(sr, sourceRate, start, end, pitch, trackGrains);
            spawn(sr, sourceRate, start, end, pitch, trackGrains, 0.8f);
            spawnWait_ = sr / std::max(1.f, c_.density);
        }
        initial_ = false;
    }
    if (c_.clocked) {
        if (--pulseWait_ <= 0) {
            const int st = pulseStep_; pulseStep_ = (pulseStep_ + 1) & 15;
            pulseWait_ += sr * 15.f / (30.f + 90.f * sm_.evolve) * (1.f + (st & 1 ? 0.08f : -0.08f) * sm_.drift);
            if (st == 0 && !sm_.freeze && random() < sm_.evolve * 0.35f) {
                int j = std::min(15, int(random() * 16)); stepPos_[j] = random();
                stepOct_[j] = random() < sm_.drift * 0.5f ? (random() < 0.5f ? 12 : 7) : 0;
                if (random() < sm_.evolve * 0.3f) pulseRot_ = (pulseRot_ + 1) & 15;
            }
            // Five evenly distributed triggers on sixteen steps, rotating
            // with the evolving pattern. No dependency on transport state.
            if ((((st - pulseRot_ + 16) & 15) * 5) % 16 < 5) {
                float p = wrap(c_.center + (stepPos_[st] - 0.5f) * c_.scatter);
                spawn(sr, sourceRate, start, end, pitch, trackGrains, st % 4 == 0 ? 1.1f : 0.77f, float(stepOct_[st]), p);
            }
        }
    } else if (c_.density > 0 && --spawnWait_ <= 0) {
        spawnWait_ = std::max(1.f, spawnWait_ + sr / c_.density * (1.f - c_.jitter + 2.f * c_.jitter * random()));
        if (sm_.mode == EchoParams::Dust) {
            if (stutterLeft_ > 0) {
                --stutterLeft_;
                spawn(sr, sourceRate, start, end, pitch, trackGrains, 1, 0, stutterPos_);
            } else {
                if (random() < sm_.evolve * 0.05f) { stutterLeft_ = 2 + int(random() * 4); stutterPos_ = wrap(c_.center + bipolar() * c_.scatter * 0.5f); }
                constexpr float intervals[] = {12, 19, 24, 7, -12};
                float sp = random() < sm_.scatter * 0.35f ? intervals[std::min(4, int(random() * 5))] : 0;
                spawn(sr, sourceRate, start, end, pitch, trackGrains, 1, sp);
            }
        } else if (sm_.mode == EchoParams::Bloom) {
            constexpr float intervals[] = {0, 12, 7, -12};
            const float x = random(); int k = x < 0.55f ? 0 : x < 0.75f ? 1 : x < 0.9f ? 2 : 3;
            spawn(sr, sourceRate, start, end, pitch, trackGrains, 0.5f + 0.5f * random(), intervals[k]);
        } else spawn(sr, sourceRate, start, end, pitch, trackGrains);
    }
    for (auto& g : grains_) {
        if (!g.active) continue;
        if (g.age >= g.len || g.pos < 0 || g.pos > frames - 2) {
            g.active = false; --trackGrains; continue;
        }
        const int i = std::clamp(int(g.pos), 0, frames - 2);
        const float f = float(g.pos - i);
        float x = mix(pcm[2 * std::size_t(i)], pcm[2 * std::size_t(i + 1)], f);
        float y = mix(pcm[2 * std::size_t(i) + 1], pcm[2 * std::size_t(i + 1) + 1], f);
        if (g.tone) {
            g.zL += (x - g.zL) * g.a; g.zR += (y - g.zR) * g.a;
            x = g.tone == 1 ? g.zL : x - g.zL; y = g.tone == 1 ? g.zR : y - g.zR;
        }
        const float w = window(g.window, float(g.age) / std::max(1, g.len - 1));
        l += x * w * g.gainL; r += y * w * g.gainR;
        ++g.age; g.pos += g.step;
        if (g.age >= g.len) { g.active = false; --trackGrains; }
    }
}
int EchoGrains::count() const {
    int n = 0; for (const auto& g : grains_) n += g.active; return n;
}
int EchoGrains::positions(double* pos, float* level, int max) const {
    int n = 0;
    for (const auto& g : grains_) if (g.active && n < max) {
        pos[n] = g.pos;
        if (level) level[n] = window(g.window, float(g.age) / std::max(1, g.len - 1));
        ++n;
    }
    return n;
}
