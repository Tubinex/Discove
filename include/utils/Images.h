#pragma once

#include <FL/Fl_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Images {

// Image loading and caching
using ImageCallback = std::function<void(Fl_RGB_Image *)>;

void loadImageAsync(const std::string &url, ImageCallback callback);

Fl_RGB_Image *getCachedImage(const std::string &url);

void clearCache();

// Image manipulation
/**
 * @brief Create a circular image from the source image
 * @param source Source image to mask
 * @param diameter Diameter of the circle (will be scaled to fit)
 * @return New circular image, or nullptr on failure
 * @note Caller is responsible for deleting the returned image
 */
Fl_RGB_Image *makeCircular(Fl_RGB_Image *source, int diameter);

/**
 * @brief Create a rounded rectangle image from the source image
 * @param source Source image to mask
 * @param width Target width
 * @param height Target height
 * @param radius Corner radius in pixels
 * @return New rounded image, or nullptr on failure
 * @note Caller is responsible for deleting the returned image
 */
Fl_RGB_Image *makeRoundedRect(Fl_RGB_Image *source, int width, int height, int radius);

} // namespace Images
