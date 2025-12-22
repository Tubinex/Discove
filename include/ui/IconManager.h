#pragma once

#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
#include <memory>
#include <string>
#include <unordered_map>

namespace IconManager {

Fl_SVG_Image *load_icon(const std::string &name, int size);
Fl_SVG_Image *load_icon(const std::string &name, int width, int height);

Fl_SVG_Image *load_recolored_icon(const std::string &name, int size, Fl_Color color);
Fl_SVG_Image *load_recolored_icon(const std::string &name, int width, int height, Fl_Color color);

void clear_cache();

constexpr const char *ICON_BASE_PATH = "assets/icons/";

} // namespace IconManager
