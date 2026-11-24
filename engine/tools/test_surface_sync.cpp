#include "SamplerApp.h"
#include <algorithm>
#include <cstdio>
#include <iterator>
#include <stdexcept>

static void require(bool ok, const char* msg) {
    if (!ok) throw std::runtime_error(msg);
}

int main() {
    try {
        Session session;
        int clears = 0, waveformRects = 0;
        uint16_t menu[SamplerApp::kPages] = {};
        SamplerApp app(session, [&](const uint8_t* m, size_t n) {
            if (n < 8) return;
            auto command = [&](const uint8_t* p, size_t len) {
                if (len == 0) return;
                if (p[0] == 2) { ++clears; waveformRects = 0; std::fill(std::begin(menu), std::end(menu), 0); }
                if (p[0] == 3 && len >= 12 && ((p[3] << 7) | p[4]) == 40) {
                    const int x = (p[1] << 7) | p[2];
                    if (x >= 116 && x < 116 + SamplerApp::kPages * 10 && (x - 116) % 10 == 0)
                        menu[(x - 116) / 10] = (uint16_t)((p[9] << 14) | (p[10] << 7) | p[11]);
                }
                if (p[0] == 3 && len >= 12 && ((p[3] << 7) | p[4]) == 54)
                    ++waveformRects;
            };
            if (m[6] != 10) { command(m + 6, n - 7); return; }
            for (size_t i = 7; i + 3 < n;) {
                size_t len = (size_t)m[i] << 7 | m[i + 1];
                if (len < 1 || i + 2 + len >= n) break;
                command(m + i + 2, len);
                i += 2 + len;
            }
        });
        uint32_t now = 0;
        auto run = [&](uint32_t ms) {
            const uint32_t end = now + ms;
            while (now < end) { now += 16; app.tick(now); }
        };
        auto pong = [&](uint32_t drops) {
            uint8_t m[30] = {0xf0, 0x7d, 'D', 'A', 'W', 1, 0x40};
            for (int i = 28; i >= 24; --i) { m[i] = drops & 127; drops >>= 7; }
            m[29] = 0xf7;
            app.onSysex(m, sizeof m);
        };
        run(3000);
        const uint16_t inactive = menu[SamplerApp::P_FX];
        require(menu[SamplerApp::P_HOME] != inactive, "initial page marker missing");
        app.setAlgo(4);
        for (int page = 0; page < SamplerApp::kPages; ++page) {
            app.gotoPage(static_cast<SamplerApp::Page>(page));
            run(3000);
            for (int i = 0; i < SamplerApp::kPages; ++i)
                require((menu[i] != inactive) == (SamplerApp::menuPage(i) == page), "page marker did not follow menu");
        }
        app.setAlgo(0);
        app.gotoPage(SamplerApp::P_HOME);
        run(3000);
        int before = clears;
        app.resetSurface();
        run(1500);
        require(clears > before, "reconnection did not clear the screen");
        require(waveformRects >= 54, "reconnection did not redraw the waveform");

        uint8_t reconnect[] = {0xf0, 0x7d, 'D', 'A', 'W', 1, 0x43, 0xf7};
        before = clears;
        app.onSysex(reconnect, sizeof reconnect);
        run(3000);
        require(clears > before && waveformRects >= 54, "firmware reconnect event did not redraw");
        pong(0);
        run(5000);
        before = clears;
        pong(1);
        run(5000);
        require(clears > before, "dropped frame did not trigger full screen recovery");
        require(waveformRects >= 54, "dropped frame did not redraw the waveform");
        std::puts("surface synchronization tests passed");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
