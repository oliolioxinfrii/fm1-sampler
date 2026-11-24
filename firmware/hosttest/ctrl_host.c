/* Host test of app/ctrl.c: stubs for the FM-1 SDK, scripted panel input, captured MIDI/SysEx and a framebuffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1.h"
#include "ctrl.c"

/* ---- SDK stubs ---- */
static struct fm1_controls ctl;
static uint32_t ms;
static uint16_t fb[240 * 240];
static unsigned char midi_log[4096][3]; static int midi_n; static int midi_full;
static unsigned usb_ready = 1, master_value;
static unsigned char sx_log[64][40]; static int sx_len[64]; static int sx_n;
unsigned fm1_midi_ready_ports(void) { return usb_ready ? 1U << FM1_MIDI_USB : 0; }
int groove_master_update(unsigned knob) { master_value = knob; return 0; }
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *s) { *s = ctl; memset(ctl.encoders, 0, sizeof ctl.encoders); }
void fm1_led_set(unsigned k, int on) { (void)k; (void)on; }
uint32_t fm1_millis(void) { return ms; }
void fm1_control_request(void) {}
void fm1_screen_init(void) {}
void fm1_screen_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { for (unsigned j = y; j < y + h && j < 240; j++) for (unsigned i = x; i < x + w && i < 240; i++) fb[j * 240 + i] = c; }
int fm1_screen_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *p, size_t n) { (void)n; for (unsigned j = 0; j < h; j++) for (unsigned i = 0; i < w; i++) if (x + i < 240 && y + j < 240) fb[(y + j) * 240 + x + i] = p[j * w + i]; return 0; }
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n) {
    (void)port;
    if (!usb_ready) return -1;
    if (midi_full) return -2;
    if (m[0] == 0xf0) { sx_len[sx_n] = (int)n; memcpy(sx_log[sx_n], m, n > 40 ? 40 : n); sx_n++; return 0; }
    midi_log[midi_n][0] = m[0]; midi_log[midi_n][1] = m[1]; midi_log[midi_n][2] = m[2]; midi_n++; return 0;
}

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void tick(void) { ms += 33; groove_screen_control(); }
static void press_key(int k, int down) { if (down) ctl.keys |= 1UL << k; else ctl.keys &= ~(1UL << k); tick(); }
static void ping(void) {
    uint8_t m[] = {0xf0, 0x7d, 'D', 'A', 'W', 1, CMD_PING, 0, 1, 0xf7};
    ctrl_sysex(m, sizeof m); tick();
}
static void clear_screen(void) {
    uint8_t m[] = {0xf0, 0x7d, 'D', 'A', 'W', 1, CMD_CLEAR, 0, 0, 0, 0xf7};
    ctrl_sysex(m, sizeof m); tick();
}
static int events(unsigned status, unsigned data) {
    int n = 0;
    for (int i = 0; i < midi_n; ++i) if (midi_log[i][0] == status && midi_log[i][1] == data) ++n;
    return n;
}
int main(void) {
    groove_screen_init();
    CHECK(memcmp(fm1_glyph('a'), fm1_glyph('A'), 5) != 0, "font preserves lowercase");
    CHECK(fm1_font_bit(fm1_glyph('g'), 2, 7) != 0, "font preserves descenders");
    CHECK(memcmp(fm1_glyph('('), fm1_glyph(' '), 5) != 0, "font supports punctuation");
    unsigned partial = 0;
    for (unsigned y = 0; y < 24; ++y)
        for (unsigned x = 0; x < 18; ++x) {
            const uint16_t px = fm1_font_pixel(fm1_glyph('A'), x, y, 3, 0xffff, 0);
            partial += px != 0 && px != 0xffff;
        }
    CHECK(partial > 0, "larger glyphs have smoothed edges");
    CHECK(link_state == LINK_WAIT, "boot waits for phone");
    press_key(0, 1); press_key(0, 0);
    CHECK(midi_n == 0, "unlinked controls locked");
    clear_screen();
    CHECK(link_state == LINK_WAIT, "clear before heartbeat ignored");
    ping();
    CHECK(link_state == LINK_CLEAR, "heartbeat awaits full redraw");
    clear_screen();
    CHECK(link_state == LINK_ACTIVE, "clear unlocks surface");
    ctl.knob = 512; tick();
    CHECK(master_value == 512, "master gain initialized");
    press_key(0, 1); press_key(0, 0);
    CHECK(events(0x90, 60) == 1 && events(0x80, 60) == 1, "connected notes forwarded");
    usb_ready = 0; tick();
    CHECK(link_state == LINK_WAIT && !reconnect_shown, "USB loss gates input immediately without drawing");
    const int before = midi_n;
    ctl.encoders[0] = 10; ctl.knob = 700; press_key(1, 1);
    ms += 1000; tick();
    CHECK(!reconnect_shown && midi_n == before && master_value == 512, "brief interruption freezes controls and preserves screen");
    ms += 2100; tick();
    CHECK(reconnect_shown && fb[40 * 240 + 8] != 0, "three-second timeout draws local reconnect panel");
    usb_ready = 1; ping();
    CHECK(link_state == LINK_CLEAR, "reconnect waits for repaint");
    CHECK(sx_n > 0 && sx_log[sx_n - 1][6] == EV_RECONNECT, "reconnect notifies phone");
    clear_screen(); tick();
    CHECK(link_state == LINK_ACTIVE && !reconnect_shown && master_value == 512, "repaint unlocks without replaying knob");
    CHECK(events(0x90, 62) == 0 && events(0xb0, 16) == 0, "held notes and offline detents discarded");
    press_key(1, 0); press_key(1, 1); press_key(1, 0);
    CHECK(events(0x90, 62) == 1 && events(0x80, 62) == 1, "new key gesture works after reconnect");
    ctl.knob = 720; tick();
    CHECK(master_value == 720, "deliberate master movement resumes control");
    ping(); ms += 3100; tick();
    CHECK(link_state == LINK_WAIT && reconnect_shown, "missing heartbeat also locks controls");
    midi_full = 1; ping();
    CHECK(redraw_pending, "failed reconnect notification remains pending");
    midi_full = 0; ping();
    CHECK(!redraw_pending && sx_log[sx_n - 1][6] == EV_RECONNECT, "reconnect notification retried");
    clear_screen();
    CHECK(link_state == LINK_ACTIVE, "second reconnect succeeds");
    printf("%s (%d failures)\n", fails ? "FAILED" : "ALL PASSED", fails);
    return fails != 0;
}
