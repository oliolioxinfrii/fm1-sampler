/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * FM1-DAW control surface: panel events out over USB MIDI, LEDs + screen
 * commands in over SysEx. Replaces the stock UI (app/screen.c).
 * Protocol: see ../../PROTOCOL.md */
#include <string.h>
#include "fm1.h"
#include "instrument.h"

enum { SYSEX_MAX = 300, SYSEX_Q = 4, KEYS = 27, BUTTONS = 14, ENC = 7, KEY_BASE = 53 };
enum { CMD_LED = 1, CMD_CLEAR = 2, CMD_RECT = 3, CMD_TEXT = 4, CMD_PING = 5, CMD_SCALE = 6, CMD_PAL = 8, CMD_BLIT = 9, CMD_BATCH = 10, CMD_NOP = 0x7e };   /* numbers are the wire format (PROTOCOL.md): keep them explicit */
enum { EV_SCALE = 0x41, EV_REDRAW = 0x42, EV_RECONNECT = 0x43 };
enum { LINK_WAIT, LINK_CLEAR, LINK_ACTIVE };
enum { LINK_TIMEOUT_MS = 3000 };
enum { BTN_OCT_DOWN = 0, BTN_OCT_UP = 1, BTN_SCL = 3 };

/* ---- SysEx receive: routing task -> control task ------------------------- */
static struct { uint8_t data[SYSEX_MAX]; volatile uint16_t length; } rxq[SYSEX_Q];
static volatile uint8_t rx_head;            /* written by routing task */
static uint8_t rx_tail;                     /* read by control task */
static volatile uint32_t rx_frames, rx_bytes, rx_dropped;

static void barrier(void)
{
#ifndef FM1_HOST_TEST
    __asm__ volatile("csync" ::: "memory");
#endif
}

/* Called from fm1_app_sysex (transfer.c) before its own filtering. */
int ctrl_sysex(const uint8_t *m, size_t n)
{
    if (n < 8 || m[n - 1] != 0xf7 || m[5] != 1 || m[0] != 0xf0 || m[1] != 0x7d || m[2] != 'D' || m[3] != 'A' || m[4] != 'W') return 0;
    rx_frames++; rx_bytes += (uint32_t)n;
    uint8_t slot = rx_head;
    if (n > SYSEX_MAX || rxq[slot].length) { rx_dropped++; return 1; }
    if (m[6] == CMD_NOP) return 1;
    memcpy(rxq[slot].data, m, n);
    barrier();
    rxq[slot].length = (uint16_t)n;
    rx_head = (uint8_t)((slot + 1) % SYSEX_Q);
    fm1_control_request();
    return 1;
}

/* ---- drawing ------------------------------------------------------------- */
#include "display_font.h"

enum { MAX_SCALE = 4, CELL_W = 6 * MAX_SCALE, CELL_H = 8 * MAX_SCALE };
static uint16_t cell[CELL_W * CELL_H];   /* glyph cell, and the pixel buffer of CMD_BLIT (768 px) */
static uint16_t blit_pal[16];

static void draw_char(unsigned x, unsigned y, unsigned scale, unsigned c, uint16_t fg, uint16_t bg)
{
    unsigned w = fm1_font_width(scale), h = 8 * scale;
    if (x + w > FM1_SCREEN_WIDTH || y + h > FM1_SCREEN_HEIGHT) return;
    fm1_font_draw(cell, w, scale, c, fg, bg);
    (void)fm1_screen_blit(x, y, w, h, cell, w * h);
}

static unsigned n14(const uint8_t *p) { return (unsigned)(p[0] & 127U) << 7 | (p[1] & 127U); }
static uint16_t color(const uint8_t *p) { return (uint16_t)((p[0] & 3U) << 14 | (p[1] & 127U) << 7 | (p[2] & 127U)); }

/* ---- scales (key index -> scale degree, root, octave, KEY or CHORD mode) ---------------- */
enum { SCALE_OFF = 7, SCALES = 8, MODE_KEY = 0, MODE_CHORD = 1 };
static const struct { const char *name; uint8_t n, step[12]; } scale_tab[SCALES - 1] = {
    {"CHROM", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"MIX", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"MAJ", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"MIN", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"HMIN", 7, {0, 2, 3, 5, 7, 8, 11}},
    {"PHRYG", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"MIN+", 8, {0, 2, 3, 5, 7, 8, 10, 11}},
};
static const char *const note_name[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static uint8_t sc_scale = 3, sc_root, sc_mode;      /* default MIN, root C, KEY */
static int8_t sc_oct;                                /* -3..+3 */
static uint8_t overlay, overlay_dirty;

/* Note of scale degree `k` (0 = root of the first octave). */
static int degree_note(unsigned k)
{
    if (sc_scale == SCALE_OFF) return 53 + (int)k;   /* the plain chromatic keyboard, F3 upwards */
    const unsigned n = scale_tab[sc_scale].n;
    return 60 + sc_root + 12 * sc_oct + (int)(k / n) * 12 + scale_tab[sc_scale].step[k % n];
}
static unsigned key_notes(unsigned key, uint8_t *out)   /* notes sounded by one key, 1 or 3 */
{
    if (sc_scale == SCALE_OFF || sc_mode == MODE_KEY) { out[0] = (uint8_t)degree_note(key); return 1; }
    for (unsigned j = 0; j < 3; ++j) out[j] = (uint8_t)degree_note(key + 2 * j);
    return 3;
}
static uint8_t held_notes[KEYS][3], held_count[KEYS];

/* ---- MIDI out ------------------------------------------------------------ */
static int midi3(uint8_t status, uint8_t a, uint8_t b)
{
    uint8_t m[3] = {status, a, b};
    return fm1_midi_send_port(FM1_MIDI_USB, m, 3);
}
static int sysex_reply(const uint8_t *body, unsigned n)
{
    uint8_t m[40] = {0xf0, 0x7d, 'D', 'A', 'W', 1};
    if (n > sizeof m - 7) return -1;
    memcpy(m + 6, body, n);
    m[6 + n] = 0xf7;
    return fm1_midi_send_port(FM1_MIDI_USB, m, 7 + n);
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 28 & 15U); p[1] = (uint8_t)(v >> 21 & 127U); p[2] = (uint8_t)(v >> 14 & 127U);
    p[3] = (uint8_t)(v >> 7 & 127U); p[4] = (uint8_t)(v & 127U);
}

static void report_scale(void)
{
    uint8_t b[6] = {EV_SCALE, sc_scale, sc_root, (uint8_t)(sc_oct + 3), sc_mode, 0};
    sysex_reply(b, 5);
}
static void scale_set(unsigned scale, unsigned root, int oct, unsigned mode)
{
    sc_scale = (uint8_t)(scale % SCALES); sc_root = (uint8_t)(root % 12);
    sc_oct = (int8_t)(oct < -3 ? -3 : oct > 3 ? 3 : oct); sc_mode = (uint8_t)(mode & 1U);
    overlay_dirty = 1; report_scale();
}

static void draw_text(unsigned x, unsigned y, unsigned scale, const char *t, uint16_t fg, uint16_t bg)
{
    for (; *t; ++t, x += fm1_font_width(scale)) draw_char(x, y, scale, (unsigned char)*t, fg, bg);
}
static void draw_overlay(void)
{
    char b[24];
    fm1_screen_fill(0, 0, 240, 240, 0);
    fm1_screen_fill(0, 0, 240, 28, 0x20e4);
    draw_text(8, 6, 2, "SCALE", 0x07ff, 0x20e4);
    unsigned n = 0;
    const char *r = note_name[sc_root];
    while (*r) b[n++] = *r++;
    b[n++] = ' ';
    const char *sn = sc_scale == SCALE_OFF ? "OFF" : scale_tab[sc_scale].name;
    while (*sn) b[n++] = *sn++;
    b[n] = 0;
    draw_text(8, 44, 4, b, 0xef5d, 0);
    draw_text(8, 92, 2, sc_mode == MODE_KEY ? "MODE  KEY" : "MODE  CHORD", 0xfd20, 0);
    b[0] = 'O'; b[1] = 'C'; b[2] = 'T'; b[3] = ' '; b[4] = ' '; b[5] = sc_oct < 0 ? '-' : '+'; b[6] = (char)('0' + (sc_oct < 0 ? -sc_oct : sc_oct)); b[7] = 0;
    draw_text(8, 116, 2, b, 0xfd20, 0);
    draw_text(8, 152, 1, "K1 SCALE  K2 ROOT  K3 OCT  K4 MODE", 0x738e, 0);
    draw_text(8, 168, 1, "KEYS FROM THE LEFT:", 0x738e, 0);
    unsigned x = 8, y = 182;
    for (unsigned k = 0; k < 14; ++k) {
        int nt = degree_note(k);
        const char *nm = note_name[nt % 12];
        draw_text(x, y, 1, nm, k % 7 == 0 && sc_scale != SCALE_OFF ? 0x07ff : 0xef5d, 0);
        x += 16;
    }
    x = 8; y = 196;
    for (unsigned k = 14; k < 27; ++k) {
        int nt = degree_note(k);
        draw_text(x, y, 1, note_name[nt % 12], 0x738e, 0);
        x += 16;
    }
    draw_text(8, 224, 1, "RELEASE SCL TO RETURN", 0x738e, 0);
}

/* ---- state --------------------------------------------------------------- */
static uint32_t sent_buttons;
static uint32_t down_keys;                 /* keys whose Note On was sent */
static int enc_acc[ENC];
static int knob_sent = -100;
static uint32_t battery_at;
static uint8_t link_state, ever_linked, reconnect_shown, redraw_pending, master_hold;
static uint32_t last_ping, lost_at, blocked_keys, blocked_buttons, prev_oct;
static uint16_t shell_bg;

static void leds_apply(const uint8_t *p, unsigned n)
{
    if (n < 6) return;
    uint64_t mask = 0;
    for (unsigned i = 0; i < 6; ++i) mask |= (uint64_t)(p[i] & 127U) << (7U * i);
    for (unsigned k = 0; k < 41; ++k) fm1_led_set(k, (int)(mask >> k & 1U));
}

static uint16_t rgb565(unsigned rgb)
{
    return (uint16_t)(((rgb >> 19) & 31U) << 11 | ((rgb >> 10) & 63U) << 5 | ((rgb >> 3) & 31U));
}

static void link_screen(int reconnect)
{
    const int light = (shell_bg >> 11) >= 16;
    const uint16_t bg = shell_bg;
    const uint16_t panel = rgb565(light ? 0xd9dce0U : 0x201c32U);
    const uint16_t accent = rgb565(light ? 0x164bc9U : 0x49d9ffU);
    const uint16_t ink = rgb565(light ? 0x161b29U : 0xf2f2ffU);
    const uint16_t well = rgb565(light ? 0x17202aU : 0x090815U);
    fm1_screen_fill(0, 0, 240, 240, bg);
    fm1_screen_fill(8, 8, 224, 24, accent);
    draw_text(14, 12, 2, "FM1 SAMPLER", light ? 0xffff : 0, accent);
    fm1_screen_fill(8, 40, 224, 168, panel);
    fm1_screen_fill(14, 46, 212, 82, well);
    draw_text(26, 64, 2, reconnect ? "LINK LOST" : "USB MIDI", rgb565(0x49d9ffU), well);
    fm1_screen_fill(27, 92, 62, 4, rgb565(0xbd8affU));
    fm1_screen_fill(151, 92, 62, 4, rgb565(0xbd8affU));
    /* A symmetric 28px cross, centered between the two cable segments. */
    const uint16_t cross = rgb565(0xff5279U);
    for (unsigned i = 0; i < 28; ++i) {
        const unsigned a = i < 2 ? 0 : i - 2;
        const unsigned b = i > 25 ? 27 : i + 2;
        fm1_screen_fill(106 + a, 80 + i, b - a + 1, 1, cross);
        fm1_screen_fill(133 - b, 80 + i, b - a + 1, 1, cross);
    }
    draw_text(18, 142, 2, reconnect ? "RECONNECT PHONE" : "CONNECT PHONE", ink, panel);
    draw_text(18, 169, 1, "CONTROLS LOCKED UNTIL LINK", ink, panel);
}

static void screen_cmd(unsigned cmd, const uint8_t *p, unsigned len);

/* CMD_BATCH: many small commands in one frame (the queue is counted in frames, not bytes). Payload: { n14 length, cmd, payload... } repeated. */
static void screen_batch(const uint8_t *p, unsigned len)
{
    unsigned i = 0;
    while (i + 3 <= len) {
        unsigned l = n14(p + i);
        if (l < 1 || i + 2 + l > len) break;
        screen_cmd(p[i + 2], p + i + 3, l - 1);
        i += 2 + l;
    }
}

static void screen_frame(const uint8_t *m, unsigned n)
{
    screen_cmd(m[6], m + 7, n - 8);
}

static void screen_cmd(unsigned cmd, const uint8_t *p, unsigned len)
{
    const unsigned drawing = cmd == CMD_CLEAR || cmd == CMD_RECT || cmd == CMD_TEXT || cmd == CMD_BLIT || cmd == CMD_BATCH;
    if (cmd != CMD_PING && cmd != CMD_BATCH && link_state != LINK_ACTIVE && !(cmd == CMD_CLEAR && link_state == LINK_CLEAR)) return;
    if (drawing && overlay && cmd != CMD_BATCH) return;
    switch (cmd) {
    case CMD_LED: leds_apply(p, len); break;
    case CMD_CLEAR:
        if (len >= 3) {
            shell_bg = color(p);
            fm1_screen_fill(0, 0, FM1_SCREEN_WIDTH, FM1_SCREEN_HEIGHT, shell_bg);
            link_state = LINK_ACTIVE; ever_linked = 1; reconnect_shown = 0; battery_at = fm1_millis();
        }
        break;
    case CMD_RECT:
        if (len >= 11) {
            unsigned x = n14(p), y = n14(p + 2), w = n14(p + 4), h = n14(p + 6);
            if (w && h && x + w <= FM1_SCREEN_WIDTH && y + h <= FM1_SCREEN_HEIGHT)
                fm1_screen_fill(x, y, w, h, color(p + 8));
        }
        break;
    case CMD_TEXT:   /* x14 y14 scale fg(3) bg(3) text... */
        if (len >= 11) {
            unsigned x = n14(p), y = n14(p + 2), scale = p[4];
            uint16_t fg = color(p + 5), bg = color(p + 8);
            if (scale < 1 || scale > MAX_SCALE) break;
            for (unsigned i = 11; i < len; ++i, x += fm1_font_width(scale)) draw_char(x, y, scale, p[i], fg, bg);
        }
        break;
    case CMD_PING:   /* id14 -> PONG id14 millis32 rx_frames rx_bytes rx_dropped */
        if (len >= 2) {
            last_ping = fm1_millis();
            if (link_state == LINK_WAIT) { link_state = LINK_CLEAR; redraw_pending = 1; }
            uint8_t r[2 + 2 + 5 + 5 + 5 + 5] = {0x40, p[0], p[1]};
            put32(r + 3, fm1_millis()); put32(r + 8, rx_frames); put32(r + 13, rx_bytes); put32(r + 18, rx_dropped);
            sysex_reply(r, 23);
            if (redraw_pending) {
                if (!sysex_reply((const uint8_t[]){ever_linked ? EV_RECONNECT : EV_REDRAW}, 1)) redraw_pending = 0;
            }
        }
        break;
    case CMD_PAL:    /* 16 colours x color(3): the palette of CMD_BLIT */
        for (unsigned i = 0; i < 16 && 3 * i + 3 <= len; ++i) blit_pal[i] = color(p + 3 * i);
        break;
    case CMD_BLIT:   /* x14 y14 w14 h14 tokens: a w*h rectangle (<= 768 px) of palette colours, run-length coded.
                      * Token: 1 byte = colour(4) << 3 | (run - 1); a run field of 7 means the next byte adds to it: run = 8 + next. */
        if (len >= 8) {
            unsigned x = n14(p), y = n14(p + 2), w = n14(p + 4), h = n14(p + 6), px = 0, total = w * h;
            if (!w || !h || total > CELL_W * CELL_H || x + w > FM1_SCREEN_WIDTH || y + h > FM1_SCREEN_HEIGHT) break;
            for (unsigned i = 8; i < len && px < total; ++i) {
                unsigned col = p[i] >> 3 & 15U, run = (p[i] & 7U) + 1U;
                if ((p[i] & 7U) == 7U && i + 1 < len) run = 8U + p[++i];
                uint16_t c = blit_pal[col];
                while (run-- && px < total) cell[px++] = c;
            }
            if (px == total) (void)fm1_screen_blit(x, y, w, h, cell, total);
        }
        break;
    case CMD_BATCH: screen_batch(p, len); break;
    case CMD_SCALE:  /* scale root octave+3 mode */
        if (len >= 4) scale_set(p[0], p[1], (int)p[2] - 3, p[3]);
        break;
    default: break;
    }
}

void groove_screen_init(void)
{
    sent_buttons = down_keys = 0;
    memset(enc_acc, 0, sizeof enc_acc);
    memset(held_count, 0, sizeof held_count);
    knob_sent = -100; battery_at = 0; overlay = overlay_dirty = 0;
    link_state = LINK_WAIT; ever_linked = reconnect_shown = redraw_pending = master_hold = 0;
    blocked_keys = blocked_buttons = prev_oct = 0;
    last_ping = lost_at = fm1_millis(); shell_bg = rgb565(0x100e1aU);
    rx_head = rx_tail = 0;
    for (unsigned i = 0; i < SYSEX_Q; ++i) rxq[i].length = 0;
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
    link_screen(0);
}

/* A key went down: sound its notes (one in KEY mode, a triad in CHORD mode) and remember them for the Note Off. */
static void key_down(unsigned key)
{
    uint8_t notes[3];
    const unsigned n = key_notes(key, notes);
    unsigned sent = 0;
    for (unsigned j = 0; j < n; ++j) {
        if (midi3(0x90, notes[j], n > 1 ? 85 : 100)) break;
        held_notes[key][sent++] = notes[j];
    }
    held_count[key] = (uint8_t)sent;
    down_keys |= 1UL << key;
}
static void key_up(unsigned key)
{
    while (held_count[key]) {
        if (midi3(0x80, held_notes[key][held_count[key] - 1], 0)) return;   /* retry next tick */
        --held_count[key];
    }
    down_keys &= ~(1UL << key);
}

void groove_screen_control(void)
{
    struct fm1_controls c;
    fm1_controls_read(&c);
    uint32_t now = fm1_millis();
    const unsigned ready = fm1_midi_ready_ports() & (1U << FM1_MIDI_USB);
    if (link_state != LINK_WAIT && (!ready || now - last_ping >= LINK_TIMEOUT_MS)) {
        link_state = LINK_WAIT; lost_at = ready ? last_ping : now;
        redraw_pending = 0; overlay = overlay_dirty = 0;
        sent_buttons = down_keys = 0;
        memset(held_count, 0, sizeof held_count);
        memset(enc_acc, 0, sizeof enc_acc);
    }

    /* Drain incoming frames before forwarding any panel events. */
    const unsigned was_active = link_state == LINK_ACTIVE, was_linked = ever_linked;
    for (unsigned i = 0; i < SYSEX_Q && rxq[rx_tail].length; ++i) {
        if (ready) screen_frame(rxq[rx_tail].data, rxq[rx_tail].length);
        barrier();
        rxq[rx_tail].length = 0;
        rx_tail = (uint8_t)((rx_tail + 1) % SYSEX_Q);
    }
    if (link_state != LINK_ACTIVE || !was_active) {
        blocked_keys = c.keys; blocked_buttons = c.buttons; prev_oct = c.buttons;
        if (was_linked) { master_hold = 1; knob_sent = c.knob & 1023; }
        memset(enc_acc, 0, sizeof enc_acc);
        if (ever_linked && !reconnect_shown && now - lost_at >= LINK_TIMEOUT_MS && link_state != LINK_ACTIVE) {
            link_screen(1); reconnect_shown = 1;
            for (unsigned i = 0; i < 41; ++i) fm1_led_set(i, 0);
        }
        return;
    }
    blocked_keys &= c.keys; blocked_buttons &= c.buttons;
    c.keys &= ~blocked_keys; c.buttons &= ~blocked_buttons;
    /* The physical master controls the jack only while the phone is linked. */
    if (!master_hold || c.knob - knob_sent > 3 || knob_sent - c.knob > 3) {
        master_hold = 0;
        (void)groove_master_update(c.knob);
    }

    /* SCL held = the on-device scale screen. */
    const unsigned scl = 0;   /* FM1 sampler: the phone owns the tonality; SCL is forwarded as a button (ch2 note 3), no on-device scale screen */
    if (scl && !overlay) { overlay = 1; overlay_dirty = 1; }
    else if (!scl && overlay) { overlay = 0; sysex_reply((const uint8_t[]){EV_REDRAW}, 1); }

    /* OCT -/+ shift the keys by an octave (a scale setting, so the phone hears the final notes). */
    {
        uint32_t pressed = c.buttons & ~prev_oct & ((1U << BTN_OCT_DOWN) | (1U << BTN_OCT_UP));
        prev_oct = c.buttons;
        if (pressed & (1U << BTN_OCT_DOWN)) scale_set(sc_scale, sc_root, sc_oct - 1, sc_mode);
        if (pressed & (1U << BTN_OCT_UP)) scale_set(sc_scale, sc_root, sc_oct + 1, sc_mode);
    }

    /* Keys: notes through the scale, channel 1. */
    uint32_t keys = c.keys & ((1UL << KEYS) - 1U), kdiff = keys ^ down_keys;
    for (unsigned i = 0; i < KEYS && kdiff; ++i) {
        if (!(kdiff >> i & 1U)) continue;
        if (keys >> i & 1U) key_down(i); else key_up(i);
    }
    /* Other buttons: notes 0..13 on channel 2 (SCL and OCT are handled here, not forwarded). */
    const uint32_t local = (1U << BTN_OCT_DOWN) | (1U << BTN_OCT_UP);
    uint32_t forwarded = c.buttons & ((1UL << BUTTONS) - 1U) & ~local;
    uint32_t bdiff = forwarded ^ sent_buttons;
    for (unsigned i = 0; i < BUTTONS && bdiff; ++i)
        if (bdiff >> i & 1U) {
            if (midi3((forwarded >> i & 1U) ? 0x91 : 0x81, (uint8_t)i, 100)) break;
            sent_buttons ^= 1UL << i;
        }
    /* Encoders: on the scale screen K1..K4 set scale, root, octave, mode; otherwise relative CC 16..22 on channel 1. */
    for (unsigned i = 0; i < ENC; ++i) {
        if (overlay) {
            int d = c.encoders[i];
            if (!d) continue;
            if (i == 0) scale_set((unsigned)((int)sc_scale + (d > 0 ? 1 : SCALES - 1)), sc_root, sc_oct, sc_mode);
            else if (i == 1) scale_set(sc_scale, (unsigned)((int)sc_root + (d > 0 ? 1 : 11)), sc_oct, sc_mode);
            else if (i == 2) scale_set(sc_scale, sc_root, sc_oct + (d > 0 ? 1 : -1), sc_mode);
            else if (i == 3) scale_set(sc_scale, sc_root, sc_oct, (unsigned)!sc_mode);
            continue;
        }
        enc_acc[i] += c.encoders[i];
        while (enc_acc[i]) {
            int d = enc_acc[i] > 63 ? 63 : enc_acc[i] < -63 ? -63 : enc_acc[i];
            if (midi3(0xb0, (uint8_t)(16 + i), (uint8_t)(64 + d))) break;
            enc_acc[i] -= d;
        }
    }
    /* Knob: 14-bit CC 7 (MSB) / 39 (LSB) on channel 1, with hysteresis. */
    int knob = c.knob & 1023;
    if (!master_hold && (knob - knob_sent > 3 || knob_sent - knob > 3)) {
        if (!midi3(0xb0, 7, (uint8_t)(knob >> 3)) && !midi3(0xb0, 39, (uint8_t)((knob & 7) << 4))) knob_sent = knob;
    }
    /* Battery CC 24 and the scale state every 2 s, so a phone that connects later learns both. */
    if (now - battery_at >= 2000U && !midi3(0xb0, 24, (uint8_t)(c.battery >> 3))) { battery_at = now; report_scale(); }

    if (overlay && overlay_dirty) { draw_overlay(); overlay_dirty = 0; }
}

void groove_screen_tap(uint32_t now) { (void)now; }
void groove_screen_mmc(unsigned command) { (void)command; }
