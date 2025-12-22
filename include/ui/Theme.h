#pragma once

#include <FL/Fl.H>

namespace ThemeColors {
constexpr Fl_Color BG_PRIMARY = 0x121214FF;
constexpr Fl_Color BG_SECONDARY = 0x1a1a1eFF;
constexpr Fl_Color BG_TERTIARY = 0x202024FF;
constexpr Fl_Color BG_ACCENT = 0x333338FF;
constexpr Fl_Color TEXT_NORMAL = 0xDCDCDCFF;
constexpr Fl_Color COLOR_TRANSPARENT = 0x00000000;

constexpr Fl_Color BTN_PRIMARY = 0x4F6FFFFF;
constexpr Fl_Color BTN_PRIMARY_HOVER = 0x3D5EEFFF;
constexpr Fl_Color BTN_SECONDARY = 0x4A4A4FFF;
constexpr Fl_Color BTN_SECONDARY_HOVER = 0x5A5A5FFF;
constexpr Fl_Color BTN_DANGER = 0xE53E3EFF;
constexpr Fl_Color BTN_DANGER_HOVER = 0xC92A2AFF; 

inline constexpr unsigned char red(Fl_Color color) { return (color >> 24) & 0xFF; }
inline constexpr unsigned char green(Fl_Color color) { return (color >> 16) & 0xFF; }
inline constexpr unsigned char blue(Fl_Color color) { return (color >> 8) & 0xFF; }
} // namespace ThemeColors

void init_theme();
