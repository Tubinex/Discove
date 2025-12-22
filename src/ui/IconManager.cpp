#include "ui/IconManager.h"

#include <FL/Fl_SVG_Image.H>
#include <fstream>
#include <sstream>

namespace IconManager {

namespace {
std::unordered_map<std::string, std::unique_ptr<Fl_SVG_Image>> icon_cache;
std::string make_cache_key(const std::string &name, int width, int height) {
    return name + "_" + std::to_string(width) + "x" + std::to_string(height);
}

std::string make_recolored_cache_key(const std::string &name, int width, int height, Fl_Color color) {
    return name + "_" + std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string(color);
}

std::string load_svg_file(const std::string &name) {
    std::string filepath = std::string(ICON_BASE_PATH) + name + ".svg";
    std::ifstream file(filepath);

    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string recolor_svg_content(const std::string &svg_content, Fl_Color color) {
    unsigned char r = (color >> 24) & 0xFF;
    unsigned char g = (color >> 16) & 0xFF;
    unsigned char b = (color >> 8) & 0xFF;

    std::stringstream color_str;
    color_str << "rgb(" << static_cast<int>(r) << "," << static_cast<int>(g) << "," << static_cast<int>(b) << ")";

    std::string result = svg_content;
    std::string color_replacement = color_str.str();

    size_t pos = 0;
    while ((pos = result.find("fill=\"", pos)) != std::string::npos) {
        size_t end_pos = result.find("\"", pos + 6);
        if (end_pos != std::string::npos) {
            std::string old_color = result.substr(pos + 6, end_pos - pos - 6);
            if (old_color != "none") {
                result.replace(pos + 6, end_pos - pos - 6, color_replacement);
            }
        }
        pos += 6;
    }

    pos = 0;
    while ((pos = result.find("stroke=\"", pos)) != std::string::npos) {
        size_t end_pos = result.find("\"", pos + 8);
        if (end_pos != std::string::npos) {
            std::string old_color = result.substr(pos + 8, end_pos - pos - 8);
            if (old_color != "none") {
                result.replace(pos + 8, end_pos - pos - 8, color_replacement);
            }
        }
        pos += 8;
    }

    return result;
}

} // namespace

Fl_SVG_Image *load_icon(const std::string &name, int size) { return load_icon(name, size, size); }

Fl_SVG_Image *load_icon(const std::string &name, int width, int height) {
    std::string cache_key = make_cache_key(name, width, height);

    auto it = icon_cache.find(cache_key);
    if (it != icon_cache.end()) {
        return it->second.get();
    }

    std::string svg_content = load_svg_file(name);
    if (svg_content.empty()) {
        return nullptr;
    }

    auto svg_image = std::make_unique<Fl_SVG_Image>(nullptr, svg_content.c_str());
    svg_image->resize(width, height);

    Fl_SVG_Image *result = svg_image.get();
    icon_cache[cache_key] = std::move(svg_image);

    return result;
}

Fl_SVG_Image *load_recolored_icon(const std::string &name, int size, Fl_Color color) {
    return load_recolored_icon(name, size, size, color);
}

Fl_SVG_Image *load_recolored_icon(const std::string &name, int width, int height, Fl_Color color) {
    std::string cache_key = make_recolored_cache_key(name, width, height, color);

    auto it = icon_cache.find(cache_key);
    if (it != icon_cache.end()) {
        return it->second.get();
    }

    std::string svg_content = load_svg_file(name);
    if (svg_content.empty()) {
        return nullptr;
    }

    std::string recolored_svg = recolor_svg_content(svg_content, color);

    auto svg_image = std::make_unique<Fl_SVG_Image>(nullptr, recolored_svg.c_str());
    svg_image->resize(width, height);

    Fl_SVG_Image *result = svg_image.get();
    icon_cache[cache_key] = std::move(svg_image);

    return result;
}

void clear_cache() { icon_cache.clear(); }

} // namespace IconManager
