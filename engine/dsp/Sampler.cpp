#include "Sampler.h"
#include "Slicer.h"
#include "Wav.h"
#include <cstring>

Sampler::Sampler(int engineRate) : sr_(engineRate) {
    for (int i = 0; i < kVoices; i++) paul_.emplace_back(new PaulStream);
}
Sampler::~Sampler() = default;

bool Sampler::load(const std::string& path) {
    auto buf = std::make_shared<SampleBuf>();
    if (!readWav(path, buf->d, buf->sr)) return false;
    buf->frames = (int)(buf->d.size() / 2);
    if (buf->frames < 64) return false;
    // loudness normalisation: the RMS of the part that sounds (20 ms blocks within 30 dB of the loudest one) goes to a common level
    float pk = 0.f; for (float v : buf->d) pk = std::max(pk, std::fabs(v));
    if (pk > 1e-4f) {
        const int blk = std::max(64, buf->sr / 50); const int nb = std::max(1, buf->frames / blk);
        std::vector<double> e((size_t)nb, 0.0); double emax = 0;
        for (int b = 0; b < nb; b++) { double s = 0; for (int i = b * blk; i < (b + 1) * blk && i < buf->frames; i++) s += (double)buf->d[2 * (size_t)i] * buf->d[2 * (size_t)i] + (double)buf->d[2 * (size_t)i + 1] * buf->d[2 * (size_t)i + 1]; e[(size_t)b] = s / (2.0 * blk); emax = std::max(emax, e[(size_t)b]); }
        double sum = 0; int cnt = 0; for (double v : e) if (v > emax * 1e-3) { sum += v; cnt++; }
        const float rms = cnt ? (float)std::sqrt(sum / cnt) : pk;
        // sparse, peaky material (drums) would stay quiet if only the peak were normalised: it gets the same RMS as sustained sounds, up to +10 dB of peak,
        // and a soft limiter (smooth knee at 0.6, ceiling 0.98) keeps what rises above that
        const float g = std::min(std::min(3.0f / pk, 0.25f / std::max(rms, 1e-4f)), 16.f);
        for (float& v : buf->d) {
            float x = v * g; const float ax = std::fabs(x);
            if (ax > 0.6f) x = (x < 0 ? -1.f : 1.f) * (0.6f + 0.38f * std::tanh((ax - 0.6f) / 0.38f));
            v = x;
        }
    }
    peaks_.assign(512, 0.f);
    for (int b = 0; b < 512; b++) {
        const long a = (long)buf->frames * b / 512, e = std::max(a + 1, (long)buf->frames * (b + 1) / 512), step = std::max(1L, (e - a) / 64);
        float m = 0.f; for (long i = a; i < e; i += step) m = std::max(m, std::fabs(buf->d[(size_t)i * 2]));
        peaks_[(size_t)b] = m;
    }
    onsets_ = findOnsets(buf->d, buf->frames, buf->sr);
    const size_t slash = path.find_last_of("/\\"), dot = path.rfind('.');
    label_ = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (dot != std::string::npos && dot > (slash == std::string::npos ? 0 : slash)) label_ = label_.substr(0, label_.rfind('.'));
    orig_ = buf;
    reslice();
    return true;
}

void Sampler::reslice() {
    if (!orig_) return;
    const int from = (int)(rangeA_ * orig_->frames), to = std::max(from + 64, (int)(rangeB_ * orig_->frames));
    slices_ = pickSlices(onsets_, from, std::min(to, orig_->frames), orig_->sr, sliceThr_, 16);
    sliceEnd_ = std::min(to, orig_->frames);
}
std::vector<float> Sampler::sliceFractions() const {
    std::vector<float> f;
    if (!orig_ || orig_->frames <= 0) return f;
    for (int s : slices_) f.push_back((float)s / orig_->frames);
    f.push_back((float)(sliceEnd_ / orig_->frames));
    return f;
}
void Sampler::setMode(int m) { mode_ = std::min(kModes - 1, std::max(0, m)); }
void Sampler::setEnv(float a, float d, float s, float r) { atk_ = a; dec_ = d; sus_ = s; rel_ = r; }
void Sampler::setRange(float a, float b) { rangeA_ = clampf(a, 0.f, 1.f); rangeB_ = clampf(b, 0.f, 1.f); if (rangeB_ < rangeA_ + 0.005f) rangeB_ = std::min(1.f, rangeA_ + 0.005f); if(mode_!=GRAIN)reslice(); }
void Sampler::setProcessed(std::shared_ptr<const SampleBuf> p, float a, float b) {
    if (proc_ && proc_ != p) retired_.push_back(std::move(proc_));
    proc_ = std::move(p); procA_ = a; procB_ = std::max(a + 0.001f, b);
}
std::shared_ptr<const SampleBuf> Sampler::takeUnusedProcessed() {
    for (auto it = retired_.begin(); it != retired_.end(); ++it) {
        if (it->use_count() != 1) continue;
        auto buf = std::move(*it);
        retired_.erase(it);
        return buf;
    }
    return {};
}

void Sampler::resolveWindow(Voice& v) const {
    auto& buf = v.buf; auto& ws = v.ws; auto& we = v.we;
    if (v.mode == KIT || v.mode == GRAIN || !proc_) {
        buf = orig_;
        if (v.mode == KIT) {
            const int n = (int)slices_.size(), i = v.slice % n;
            ws = slices_[(size_t)i]; we = i + 1 < n ? slices_[(size_t)i + 1] : sliceEnd_;
        } else { ws = rangeA_ * buf->frames; we = rangeB_ * buf->frames; }
    } else {
        buf = proc_;   // a stretched copy of [procA, procB]: the window is the same range inside it
        v.sourceStart = procA_; v.sourceEnd = procB_;
        const double span = procB_ - procA_;
        ws = clampf((float)((rangeA_ - procA_) / span), 0.f, 1.f) * buf->frames; we = clampf((float)((rangeB_ - procA_) / span), 0.f, 1.f) * buf->frames;
    }
    we = std::min<double>(we, buf->frames - 2); ws = std::min(ws, we - 16);
    if (ws < 0) ws = 0;
}

void Sampler::noteOn(int note, int vel) {
    if (!orig_) return;
    int vi = -1; uint32_t oldest = 0xffffffffu;
    if (mode_ == PAUL) {   // each voice costs FFTs: at most four at once
        int active = 0; for (auto& v : voices_) active += v.on && v.mode == PAUL;
        if (active >= 4) { for (int i = 0; i < kVoices; i++) if (voices_[i].on && voices_[i].mode == PAUL && voices_[i].born < oldest) { oldest = voices_[i].born; vi = i; } }
    }
    if (vi < 0) for (int i = 0; i < kVoices; i++) if (!voices_[i].on) { vi = i; break; }
    if (vi < 0) { oldest = 0xffffffffu; for (int i = 0; i < kVoices; i++) if (voices_[i].born < oldest) { oldest = voices_[i].born; vi = i; } }
    Voice& v = voices_[vi];
    if (v.on) {
        // Fade the outgoing contribution instead of cutting it at a nonzero sample.
        stealL_ += v.lastL; stealR_ += v.lastR;
        stealLeft_ = std::max(1, sr_ / 200);
    }
    v = Voice();
    v.on = true; v.gate = true; v.note = note; v.vel = clampf(vel / 127.f, 0.f, 1.f); v.born = ++bornCounter_;
    v.slice = ((note - 60) % 16 + 16) % 16;
    v.mode = mode_;
    v.baseSemis = v.mode == KIT ? kitSemis_ : note - root_;
    resolveWindow(v);
    v.pos = v.ws;
    v.env.set(atk_, dec_, sus_, rel_, (float)sr_);
    if (mode_ == ONESHOT) v.env.set(0.002f, 0.01f, 1.f, 0.01f, (float)sr_);
    v.env.gateOn();
    if (mode_ == GRAIN) v.echo.seed(0x1234567u ^ (v.born * 0x9e3779b9u) ^ (uint32_t(note) << 16));
    if (mode_ == PAUL) { v.paulIdx = vi; paul_[(size_t)vi]->start(v.buf->d.data(), v.buf->frames, v.ws, v.we, paulStretch_); }
}
void Sampler::noteOff(int note) {
    for (auto& v : voices_) if (v.on && v.gate && v.note == note && v.mode != ONESHOT) { v.gate = false; v.env.gateOff(); }
}
void Sampler::allOff() { for (auto& v : voices_) v = Voice(); stealL_ = stealR_ = 0.f; stealLeft_ = 0; }
bool Sampler::hasVoices() const { for (auto& v : voices_) if (v.on) return true; return false; }

void Sampler::render(float* out, int n) {
    if (!orig_) return;
    int trackGrains = 0;
    for (const auto& v : voices_) if (v.on && v.mode == GRAIN) trackGrains += v.echo.count();
    for (auto& v : voices_) if (v.on) {
        renderVoice(v, out, n, trackGrains);
        if (!v.on) {
            if (v.mode == GRAIN) trackGrains -= v.echo.count();
            v.buf.reset();
        }
    }
    for (int i = 0; i < n && stealLeft_ > 0; ++i) {
        out[2 * i] += stealL_; out[2 * i + 1] += stealR_;
        const float fade = (float)(stealLeft_ - 1) / stealLeft_;
        stealL_ *= fade; stealR_ *= fade; --stealLeft_;
    }
}

void Sampler::renderVoice(Voice& v, float* out, int n, int& trackGrains) {
    const float* d = v.buf->d.data(); const int frames = v.buf->frames;
    const double srcRate = (double)v.buf->sr / sr_;
    const double pitch = std::pow(2.0, (v.baseSemis + bend_) / 12.0);
    const double rate = srcRate * pitch;
    auto readAt = [&](double p, float& l, float& r) {
        const int i0 = std::min(frames - 2, std::max(0, (int)p)); const float f = (float)(p - i0);
        l = d[(size_t)i0 * 2] + (d[(size_t)i0 * 2 + 2] - d[(size_t)i0 * 2]) * f;
        r = d[(size_t)i0 * 2 + 1] + (d[(size_t)i0 * 2 + 3] - d[(size_t)i0 * 2 + 1]) * f;
    };
    const float gain = 0.8f * v.vel;
    // Current-source granular notes use the edited region for new
    // grains; already sounding grains finish their original windows.
    if(mode_==GRAIN&&v.mode==GRAIN&&v.buf==orig_){
        v.we=std::min<double>(rangeB_*frames,frames-2);
        v.ws=std::max(0.0,std::min<double>(rangeA_*frames,v.we-16));
    }
    for (int i = 0; i < n; i++) {
        float l = 0.f, r = 0.f;
        const float e = v.env.next();
        if (v.env.done() && !v.gate) { v.on = false; return; }
        if (v.mode == PAUL) {
            PaulStream& ps = *paul_[(size_t)v.paulIdx];
            ps.next(pitch, l, r);
            if (paulWidth_ != 1.f) { const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * paulWidth_; l = m + sd; r = m - sd; }
            v.pos = ps.sourcePos();
        } else if (v.mode == GRAIN) {
            v.echo.next(echoParams_, d, frames, v.buf->sr, sr_, v.ws, v.we,
                        v.baseSemis + bend_, trackGrains, l, r);
        } else {
            double p = v.pos;
            if (v.mode == LOOPED) {
                const double xf = std::min(256.0, (v.we - v.ws) * 0.25);
                readAt(p, l, r);
                if (p >= v.we - xf) { const double t = (p - (v.we - xf)) / xf; float l2, r2; readAt(v.ws + (p - (v.we - xf)), l2, r2); l = l * (float)(1 - t) + l2 * (float)t; r = r * (float)(1 - t) + r2 * (float)t; }
                v.pos += rate;
                if (v.pos >= v.we) v.pos = v.ws + xf + (v.pos - v.we);
            } else {
                if (p >= v.we) { v.on = false; return; }
                readAt(p, l, r);
                const float tail = (float)std::min(1.0, (v.we - p) / (128.0 * rate));   // short fade at the end of the window
                l *= tail; r *= tail;
                v.pos += rate;
            }
        }
        v.lastL = l * e * gain; v.lastR = r * e * gain;
        out[2 * i] += v.lastL; out[2 * i + 1] += v.lastR;
    }
}

float Sampler::originalPosOf(const Voice& v, double pos) const {
    if (!v.buf || v.buf->frames <= 0) return 0.f;
    return clampf((float)(v.sourceStart + pos / v.buf->frames * (v.sourceEnd - v.sourceStart)), 0.f, 1.f);
}

int Sampler::voicePositions(float* out, int maxN, float* level) const {
    int n = 0;
    for (auto& v : voices_) {
        if (!v.on || n >= maxN) continue;
        if (v.mode == GRAIN) {
            double pos[EchoGrains::kGrains]; float amp[EchoGrains::kGrains];
            const int count = v.echo.positions(pos, amp, std::min(maxN - n, EchoGrains::kGrains));
            for (int i = 0; i < count; ++i) {
                if (level) level[n] = v.env.level * v.vel * amp[i];
                out[n++] = originalPosOf(v, pos[i]);
            }
        } else { if (level) level[n] = v.env.level * v.vel; out[n++] = originalPosOf(v, v.pos); }
    }
    return n;
}
