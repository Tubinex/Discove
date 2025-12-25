#include "utils/Images.h"

#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <ixwebsocket/IXHttpClient.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

namespace Images {

namespace {
std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> image_cache;
std::mutex cache_mutex;

std::string detectImageFormat(const std::string &data) {
    if (data.size() < 8)
        return "unknown";

    if (data.size() >= 8 && static_cast<unsigned char>(data[0]) == 0x89 && data[1] == 'P' && data[2] == 'N' &&
        data[3] == 'G') {
        return "png";
    }

    if (data.size() >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F') {
        return "gif";
    }

    if (data.size() >= 12 && data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' && data[8] == 'W' &&
        data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
        return "webp";
    }

    if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xFF &&
        static_cast<unsigned char>(data[1]) == 0xD8 && static_cast<unsigned char>(data[2]) == 0xFF) {
        return "jpeg";
    }

    return "unknown";
}

std::string generateTempFilename(const std::string &extension) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 999999);

    std::string tempDir;
#ifdef _WIN32
    char *tempPath = nullptr;
    size_t len;
    if (_dupenv_s(&tempPath, &len, "TEMP") == 0 && tempPath != nullptr) {
        tempDir = tempPath;
        free(tempPath);
    } else {
        tempDir = "C:\\Temp";
    }
#else
    tempDir = "/tmp";
#endif

    return tempDir + "/discove_img_" + std::to_string(dis(gen)) + "." + extension;
}

std::string getCacheDirectory() {
    std::string cacheDir;
#ifdef _WIN32
    char *appData = nullptr;
    size_t len;
    if (_dupenv_s(&appData, &len, "LOCALAPPDATA") == 0 && appData != nullptr) {
        cacheDir = std::string(appData) + "\\Discove\\cache";
        free(appData);
    } else {
        cacheDir = "C:\\Temp\\Discove\\cache";
    }
#else
    const char *home = getenv("HOME");
    if (home) {
        cacheDir = std::string(home) + "/.cache/discove";
    } else {
        cacheDir = "/tmp/discove_cache";
    }
#endif

    try {
        std::filesystem::create_directories(cacheDir);
    } catch (const std::exception &e) {
        Logger::error("Failed to create cache directory: " + std::string(e.what()));
    }

    return cacheDir;
}

std::string urlToFilename(const std::string &url) {
    std::hash<std::string> hasher;
    size_t hash = hasher(url);

    std::ostringstream oss;
    oss << std::hex << hash;

    return oss.str();
}

std::string getCacheFilePath(const std::string &url, const std::string &extension) {
    std::string cacheDir = getCacheDirectory();
    std::string filename = urlToFilename(url) + "." + extension;
    return cacheDir + "/" + filename;
}

void saveToCache(const std::string &url, const std::string &data, const std::string &format) {
    try {
        std::string cacheFile = getCacheFilePath(url, format);
        std::ofstream out(cacheFile, std::ios::binary);
        if (out.is_open()) {
            out.write(data.data(), data.size());
            out.close();
            Logger::debug("Cached image to: " + cacheFile);
        }
    } catch (const std::exception &e) {
        Logger::warn("Failed to save image to cache: " + std::string(e.what()));
    }
}

std::string loadFromCache(const std::string &url, const std::string &format) {
    try {
        std::string cacheFile = getCacheFilePath(url, format);

        if (!std::filesystem::exists(cacheFile)) {
            return "";
        }

        std::ifstream in(cacheFile, std::ios::binary);
        if (!in.is_open()) {
            return "";
        }

        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    } catch (const std::exception &e) {
        Logger::warn("Failed to load image from cache: " + std::string(e.what()));
        return "";
    }
}

Fl_RGB_Image *imageFromData(const std::string &data, const std::string &url) {
    std::string format = detectImageFormat(data);

    if (format == "png") {
        std::unique_ptr<Fl_PNG_Image> png_image =
            std::make_unique<Fl_PNG_Image>(nullptr, (const unsigned char *)data.data(), data.size());

        if (png_image && png_image->w() > 0 && png_image->h() > 0) {
            return (Fl_RGB_Image *)png_image->copy();
        }
    } else if (format == "gif") {
        std::string tempFile = generateTempFilename("gif");

        std::ofstream out(tempFile, std::ios::binary);
        if (out.is_open()) {
            out.write(data.data(), data.size());
            out.close();

            std::unique_ptr<Fl_GIF_Image> gif_image = std::make_unique<Fl_GIF_Image>(tempFile.c_str());

            if (gif_image && gif_image->w() > 0 && gif_image->h() > 0) {
                Fl_RGB_Image *result = (Fl_RGB_Image *)gif_image->copy();
                std::remove(tempFile.c_str());
                return result;
            }

            std::remove(tempFile.c_str());
        }
    }

    Logger::warn("Unsupported or corrupted image format: " + format + " for URL: " + url);
    return nullptr;
}

void downloadImageAsync(const std::string &url, ImageCallback callback) {
    std::thread([url, callback]() {
        ix::HttpClient httpClient;
        ix::HttpRequestArgsPtr args = httpClient.createRequest();

        args->extraHeaders["User-Agent"] = "Discove/1.0";

        ix::HttpResponsePtr response = httpClient.get(url, args);

        if (!response) {
            Logger::error("Failed to fetch image from: " + url);
            auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
            Fl::awake(
                [](void *p) {
                    std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                    (*fnPtr)();
                },
                heapFn);
            return;
        }

        if (response->statusCode != 200) {
            Logger::error("HTTP " + std::to_string(response->statusCode) + " for image: " + url);
            auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
            Fl::awake(
                [](void *p) {
                    std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                    (*fnPtr)();
                },
                heapFn);
            return;
        }

        const std::string &imageData = response->body;

        std::string format = detectImageFormat(imageData);
        saveToCache(url, imageData, format);

        Fl_RGB_Image *image = imageFromData(imageData, url);
        if (!image) {
            Logger::error("Failed to decode image from: " + url);
            auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
            Fl::awake(
                [](void *p) {
                    std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                    (*fnPtr)();
                },
                heapFn);
            return;
        }
        {
            std::scoped_lock lock(cache_mutex);
            image_cache[url] = std::unique_ptr<Fl_RGB_Image>(image);
        }
        auto *heapFn = new std::function<void()>([callback, image]() { callback(image); });
        Fl::awake(
            [](void *p) {
                std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                (*fnPtr)();
            },
            heapFn);
    }).detach();
}

bool isInsideCircle(int x, int y, int centerX, int centerY, int radius) {
    int dx = x - centerX;
    int dy = y - centerY;
    return (dx * dx + dy * dy) <= (radius * radius);
}

bool isInsideRoundedRect(int x, int y, int width, int height, int radius) {
    bool inLeftCorner = x < radius;
    bool inRightCorner = x >= width - radius;
    bool inTopCorner = y < radius;
    bool inBottomCorner = y >= height - radius;

    if (!inLeftCorner && !inRightCorner)
        return true;
    if (!inTopCorner && !inBottomCorner)
        return true;

    int cornerX = inLeftCorner ? radius : width - radius - 1;
    int cornerY = inTopCorner ? radius : height - radius - 1;

    int dx = x - cornerX;
    int dy = y - cornerY;
    return (dx * dx + dy * dy) <= (radius * radius);
}

} // namespace

void loadImageAsync(const std::string &url, ImageCallback callback) {
    {
        std::scoped_lock lock(cache_mutex);
        auto it = image_cache.find(url);
        if (it != image_cache.end()) {
            callback(it->second.get());
            return;
        }
    }

    std::thread([url, callback]() {
        std::vector<std::string> formats = {"png", "gif", "jpeg", "webp"};
        std::string cachedData;
        std::string foundFormat;

        for (const auto &format : formats) {
            cachedData = loadFromCache(url, format);
            if (!cachedData.empty()) {
                foundFormat = format;
                break;
            }
        }

        if (!cachedData.empty()) {
            Fl_RGB_Image *image = imageFromData(cachedData, url);

            if (image) {
                {
                    std::scoped_lock lock(cache_mutex);
                    image_cache[url] = std::unique_ptr<Fl_RGB_Image>(image);
                }
                auto *heapFn = new std::function<void()>([callback, image]() { callback(image); });
                Fl::awake(
                    [](void *p) {
                        std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                        (*fnPtr)();
                    },
                    heapFn);
                return;
            }
        }

        downloadImageAsync(url, callback);
    }).detach();
}

Fl_RGB_Image *getCachedImage(const std::string &url) {
    std::scoped_lock lock(cache_mutex);
    auto it = image_cache.find(url);
    if (it != image_cache.end()) {
        return it->second.get();
    }
    return nullptr;
}

void clearCache() {
    {
        std::scoped_lock lock(cache_mutex);
        image_cache.clear();
    }
    try {
        std::string cacheDir = getCacheDirectory();
        if (std::filesystem::exists(cacheDir)) {
            for (const auto &entry : std::filesystem::directory_iterator(cacheDir)) {
                if (entry.is_regular_file()) {
                    std::filesystem::remove(entry.path());
                }
            }
            Logger::info("Cleared disk cache at: " + cacheDir);
        }
    } catch (const std::exception &e) {
        Logger::error("Failed to clear disk cache: " + std::string(e.what()));
    }
}

Fl_RGB_Image *makeCircular(Fl_RGB_Image *source, int diameter) {
    if (!source || source->w() <= 0 || source->h() <= 0 || diameter <= 0) {
        return nullptr;
    }

    Fl_RGB_Image *scaled = source;
    bool needsDelete = false;

    if (source->w() != diameter || source->h() != diameter) {
        scaled = (Fl_RGB_Image *)source->copy(diameter, diameter);
        needsDelete = true;
        if (!scaled)
            return nullptr;
    }

    const unsigned char *srcData = (const unsigned char *)scaled->data()[0];
    int srcDepth = scaled->d();
    int srcLineSize = scaled->ld() ? scaled->ld() : diameter * srcDepth;

    int newDepth = 4;
    std::vector<unsigned char> buffer(diameter * diameter * newDepth);

    int radius = diameter / 2;
    int centerX = radius;
    int centerY = radius;

    for (int y = 0; y < diameter; y++) {
        for (int x = 0; x < diameter; x++) {
            int srcIdx = y * srcLineSize + x * srcDepth;
            int dstIdx = (y * diameter + x) * newDepth;

            if (srcDepth >= 3) {
                buffer[dstIdx + 0] = srcData[srcIdx + 0];
                buffer[dstIdx + 1] = srcData[srcIdx + 1];
                buffer[dstIdx + 2] = srcData[srcIdx + 2];
            } else if (srcDepth == 1) {
                buffer[dstIdx + 0] = srcData[srcIdx];
                buffer[dstIdx + 1] = srcData[srcIdx];
                buffer[dstIdx + 2] = srcData[srcIdx];
            }

            unsigned char sourceAlpha = 255;
            if (srcDepth == 4) {
                sourceAlpha = srcData[srcIdx + 3];
            }

            if (isInsideCircle(x, y, centerX, centerY, radius)) {
                buffer[dstIdx + 3] = sourceAlpha;
            } else {
                buffer[dstIdx + 3] = 0;
            }
        }
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    Fl_RGB_Image *result = new Fl_RGB_Image(heapData, diameter, diameter, newDepth);
    result->alloc_array = 1;

    if (needsDelete) {
        delete scaled;
    }

    return result;
}

Fl_RGB_Image *makeRoundedRect(Fl_RGB_Image *source, int width, int height, int radius) {
    if (!source || source->w() <= 0 || source->h() <= 0 || width <= 0 || height <= 0) {
        return nullptr;
    }

    int maxRadius = std::min(width, height) / 2;
    radius = std::min(radius, maxRadius);

    Fl_RGB_Image *scaled = source;
    bool needsDelete = false;

    if (source->w() != width || source->h() != height) {
        scaled = (Fl_RGB_Image *)source->copy(width, height);
        needsDelete = true;
        if (!scaled)
            return nullptr;
    }

    const unsigned char *srcData = (const unsigned char *)scaled->data()[0];
    int srcDepth = scaled->d();
    int srcLineSize = scaled->ld() ? scaled->ld() : width * srcDepth;

    int newDepth = 4;
    std::vector<unsigned char> buffer(width * height * newDepth);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = y * srcLineSize + x * srcDepth;
            int dstIdx = (y * width + x) * newDepth;

            if (srcDepth >= 3) {
                buffer[dstIdx + 0] = srcData[srcIdx + 0];
                buffer[dstIdx + 1] = srcData[srcIdx + 1];
                buffer[dstIdx + 2] = srcData[srcIdx + 2];
            } else if (srcDepth == 1) {
                buffer[dstIdx + 0] = srcData[srcIdx];
                buffer[dstIdx + 1] = srcData[srcIdx];
                buffer[dstIdx + 2] = srcData[srcIdx];
            }

            unsigned char sourceAlpha = 255;
            if (srcDepth == 4) {
                sourceAlpha = srcData[srcIdx + 3];
            }

            if (isInsideRoundedRect(x, y, width, height, radius)) {
                buffer[dstIdx + 3] = sourceAlpha;
            } else {
                buffer[dstIdx + 3] = 0;
            }
        }
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    Fl_RGB_Image *result = new Fl_RGB_Image(heapData, width, height, newDepth);
    result->alloc_array = 1;

    if (needsDelete) {
        delete scaled;
    }

    return result;
}

} // namespace Images
