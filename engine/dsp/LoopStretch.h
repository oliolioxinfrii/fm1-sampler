// Circular WSOLA read heads. All tracks share the same positions and blend.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

class LoopStretch {
public:
    struct Frame { double a, b; float blend; };
    explicit LoopStretch(int sr) : hop_(std::max(64, sr / 50)), search_(std::max(16, sr / 100)) {}
    void reset() { phase_ = hop_; initialized_ = false; }
    static double wrap(double p, int frames) {
        p = std::fmod(p, frames);
        return p < 0 ? p + frames : p;
    }
    static float read(const std::vector<float>& clip, int frames, double p, int ch) {
        const int i = (int)p;
        const int j = i + 1 == frames ? 0 : i + 1;
        const float u = (float)(p - i);
        return clip[2 * (size_t)i + ch] + u * (clip[2 * (size_t)j + ch] - clip[2 * (size_t)i + ch]);
    }
    Frame next(double nominal, double pitchRate, int frames, const std::vector<float>* reference) {
        if (!initialized_) {
            old_ = current_ = wrap(nominal, frames);
            phase_ = 0;
            initialized_ = true;
        } else if (phase_ >= hop_) {
            old_ = current_;
            current_ = wrap(nominal, frames);
            if (reference) {
                // Compare a short section of the overlap. Search both channels
                // independently to avoid cancellation of stereo signals.
                double best = -2.0;
                int bestOffset = 0;
                const int stride = std::max(1, search_ / 40);
                for (int offset = -search_; offset <= search_; offset += stride) {
                    double ab = 0, aa = 1e-12, bb = 1e-12;
                    for (int i = 0; i < hop_; i += 16) {
                        const double p = wrap(old_ + i * pitchRate, frames);
                        const double q = wrap(nominal + offset + i * pitchRate, frames);
                        for (int ch = 0; ch < 2; ++ch) {
                            const float x = read(*reference, frames, p, ch);
                            const float y = read(*reference, frames, q, ch);
                            ab += x * y; aa += x * x; bb += y * y;
                        }
                    }
                    const double score = ab / std::sqrt(aa * bb) - 0.01 * std::abs(offset) / search_;
                    if (score > best) { best = score; bestOffset = offset; }
                }
                current_ = wrap(nominal + bestOffset, frames);
            }
            phase_ = 0;
        }
        const float u = (float)phase_ / hop_;
        const float blend = u * u * (3.f - 2.f * u);
        const Frame f{old_, current_, blend};
        old_ = wrap(old_ + pitchRate, frames);
        current_ = wrap(current_ + pitchRate, frames);
        ++phase_;
        return f;
    }
private:
    int hop_, search_, phase_ = 0;
    bool initialized_ = false;
    double old_ = 0, current_ = 0;
};
