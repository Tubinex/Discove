#pragma once

#include <FL/Fl.H>

namespace ThemeColors {
constexpr Fl_Color BG_PRIMARY = 0x121214FF;
constexpr Fl_Color BG_SECONDARY = 0x1a1a1eFF;
constexpr Fl_Color BG_SECONDARY_ALT = 0x2b2b31FF;
constexpr Fl_Color BG_TERTIARY = 0x202024FF;
constexpr Fl_Color BG_ACCENT = 0x333338FF;
constexpr Fl_Color BG_MODIFIER_HOVER = 0x35353cFF;
constexpr Fl_Color BG_MODIFIER_SELECTED = 0x404249FF;
constexpr Fl_Color BG_MODIFIER_ACCENT = 0x4e505aFF;

constexpr Fl_Color TEXT_NORMAL = 0xDCDCDCFF;
constexpr Fl_Color TEXT_MUTED = 0x949ba4FF;
constexpr Fl_Color TEXT_LINK = 0x00a8fcFF;
constexpr Fl_Color COLOR_TRANSPARENT = 0x00000000;

constexpr Fl_Color BRAND_PRIMARY = 0x5865F2FF;
constexpr Fl_Color BRAND_ON_PRIMARY = 0xFFFFFFFF;
constexpr Fl_Color SEPARATOR_GUILD = 0x35353cFF;
constexpr Fl_Color GUILD_ICON_BG = 0x2b2b31FF;

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
