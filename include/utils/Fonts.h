#pragma once

#include <FL/Enumerations.H>
#include <string>

namespace FontLoader {
bool loadFonts();

Fl_Font getFontId(const std::string &name);
namespace Fonts {
extern Fl_Font INTER_THIN;
extern Fl_Font INTER_EXTRA_LIGHT;
extern Fl_Font INTER_LIGHT;
extern Fl_Font INTER_REGULAR;
extern Fl_Font INTER_MEDIUM;
extern Fl_Font INTER_SEMIBOLD;
extern Fl_Font INTER_BOLD;
extern Fl_Font INTER_EXTRA_BOLD;
extern Fl_Font INTER_BLACK;
extern Fl_Font INTER_THIN_ITALIC;
extern Fl_Font INTER_EXTRA_LIGHT_ITALIC;
extern Fl_Font INTER_LIGHT_ITALIC;
extern Fl_Font INTER_REGULAR_ITALIC;
extern Fl_Font INTER_MEDIUM_ITALIC;
extern Fl_Font INTER_SEMIBOLD_ITALIC;
extern Fl_Font INTER_BOLD_ITALIC;
extern Fl_Font INTER_EXTRA_BOLD_ITALIC;
extern Fl_Font INTER_BLACK_ITALIC;
} // namespace Fonts
} // namespace FontLoader
