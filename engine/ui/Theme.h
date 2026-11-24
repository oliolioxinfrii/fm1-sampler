// Phone UI colors: desktop chrome, recessed signal displays, and semantic accents.
#pragma once
#include <cmath>
#include <cstdint>

namespace theme {
constexpr uint32_t argb(int r, int g, int b, int a = 255) { return (uint32_t)a << 24 | (uint32_t)r << 16 | (uint32_t)g << 8 | (uint32_t)b; }
constexpr uint32_t withAlpha(uint32_t c, int a) { return (c & 0x00ffffffu) | (uint32_t)a << 24; }

inline uint32_t BG, SURFACE, RAISED, OUTLINE, TEXT, DIM, TRACK, ACCENT, ON_ACCENT;
inline uint32_t VIOLET, GREEN, RED, AMBER, PINK, HI, SHADOW, WELL, GRID, WELL_TEXT, WELL_DIM;
inline uint32_t TRACE_BLUE, TRACE_GREEN, TRACE_VIOLET, TRACE_AMBER, TRACE_RED, TRACE_PINK;
inline bool light = true;
inline void apply(bool lightVariant) {
    light = lightVariant;
    if (light) {
        BG = 0xffc6c9cb; SURFACE = 0xffd9dce0; RAISED = 0xffe9e9e5;
        OUTLINE = 0xff828b99; TEXT = 0xff161b29; DIM = 0xff4e5868; TRACK = 0xffaab4be;
        ACCENT = 0xff164bc9; ON_ACCENT = 0xffffffff; GREEN = 0xff007d58;
        VIOLET = 0xff8522ba; AMBER = 0xff925900; RED = 0xffc42248; PINK = 0xffb51b79;
        HI = 0xfff8fafb; SHADOW = 0xff667080;
        WELL = 0xff17202a; GRID = 0xff364455; WELL_TEXT = 0xffecf4fa; WELL_DIM = 0xffa2b3c7;
        TRACE_BLUE = 0xff51b8ff; TRACE_GREEN = 0xff39efac; TRACE_VIOLET = 0xffc68aff;
        TRACE_AMBER = 0xffffc361; TRACE_RED = 0xffff5876; TRACE_PINK = 0xffff70c7;
    } else {
        BG = 0xff100e1a; SURFACE = 0xff201c32; RAISED = 0xff302845;
        OUTLINE = 0xff685588; TEXT = 0xfff2f2ff; DIM = 0xffb3a9cb; TRACK = 0xff48405f;
        ACCENT = 0xff49d9ff; ON_ACCENT = 0xff10101a; GREEN = 0xff53ffab;
        VIOLET = 0xffbd8aff; AMBER = 0xffffc457; RED = 0xffff5279; PINK = 0xffff64df;
        HI = 0xff766394; SHADOW = 0xff080710;
        WELL = 0xff090815; GRID = 0xff322747; WELL_TEXT = 0xffe7f5ff; WELL_DIM = 0xff9f91bb;
        TRACE_BLUE = ACCENT; TRACE_GREEN = GREEN; TRACE_VIOLET = VIOLET;
        TRACE_AMBER = AMBER; TRACE_RED = RED; TRACE_PINK = PINK;
    }
}
inline const bool kInit = (apply(true), true);

constexpr float R_CARD = 0, R_CONTROL = 0, GAP = 6, MARGIN = 8;
constexpr float T_CAPTION = 10, T_BODY = 12, T_VALUE = 15, T_TITLE = 18, T_NUMBER = 22;

inline uint32_t encoderColor(int i) {
    const uint32_t colors[] = {ACCENT, GREEN, VIOLET, AMBER};
    return colors[i & 3];
}
inline uint32_t encoderTrace(int i) {
    const uint32_t colors[] = {TRACE_BLUE, TRACE_GREEN, TRACE_VIOLET, TRACE_AMBER};
    return colors[i & 3];
}
inline uint32_t trackColor(int i) {
    const uint32_t colors[] = {ACCENT, GREEN, VIOLET, PINK, AMBER, ACCENT, GREEN, VIOLET};
    return colors[i & 7];
}
inline uint32_t trackTrace(int i) {
    const uint32_t colors[] = {TRACE_BLUE, TRACE_GREEN, TRACE_VIOLET, TRACE_PINK, TRACE_AMBER, TRACE_BLUE, TRACE_GREEN, TRACE_VIOLET};
    return colors[i & 7];
}
inline uint32_t contrastInk(uint32_t c) {
    auto lin = [](float v) { v /= 255.f; return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    const float y = 0.2126f * lin((c >> 16) & 255) + 0.7152f * lin((c >> 8) & 255) + 0.0722f * lin(c & 255);
    return y > 0.179f ? 0xff090a12 : 0xffffffff;
}
}  // namespace theme
