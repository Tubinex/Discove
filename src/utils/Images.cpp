#include "utils/Images.h"

#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/Fl_GIF_Image.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Shared_Image.H>
#include <ixwebsocket/IXHttpClient.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_set>
#include <vector>

namespace Images {

namespace {
std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> image_cache;
std::mutex cache_mutex;

std::unordered_set<std::string> failed_urls;
std::mutex failed_mutex;

struct DownloadRequest {
    std::string url;
    ImageCallback callback;
};

std::queue<DownloadRequest> download_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
std::atomic<bool> worker_running{true};
std::atomic<int> active_workers{0};
std::unordered_set<std::string> pending_downloads;
constexpr int MAX_CONCURRENT_DOWNLOADS = 8;

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

bool isValidPNG(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return false;

    unsigned char signature[8];
    file.read(reinterpret_cast<char *>(signature), 8);
    if (file.gcount() != 8)
        return false;

    return signature[0] == 0x89 && signature[1] == 0x50 && signature[2] == 0x4E && signature[3] == 0x47 &&
           signature[4] == 0x0D && signature[5] == 0x0A && signature[6] == 0x1A && signature[7] == 0x0A;
}

bool isValidGIF(const std::string &filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return false;

    char signature[6];
    file.read(signature, 6);
    if (file.gcount() != 6)
        return false;

    return signature[0] == 'G' && signature[1] == 'I' && signature[2] == 'F' && signature[3] == '8' &&
           (signature[4] == '7' || signature[4] == '9') && signature[5] == 'a';
}

bool shouldAttemptDownload(const std::string &url) {
    std::scoped_lock lock(failed_mutex);
    return failed_urls.find(url) == failed_urls.end();
}

void markUrlAsFailed(const std::string &url) {
    std::scoped_lock lock(failed_mutex);
    failed_urls.insert(url);
    Logger::warn("Marked URL as failed (will not retry): " + url);
}

void clearFailedUrl(const std::string &url) {
    std::scoped_lock lock(failed_mutex);
    failed_urls.erase(url);
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

        bool isValid = false;
        if (format == "png") {
            isValid = isValidPNG(cacheFile);
        } else if (format == "gif") {
            isValid = isValidGIF(cacheFile);
        } else {
            isValid = true;
        }

        if (!isValid) {
            Logger::warn("Corrupted " + format + " file detected, deleting: " + cacheFile);
            std::filesystem::remove(cacheFile);
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

        if (png_image) {
            if (png_image->w() > 0 && png_image->h() > 0) {
                return (Fl_RGB_Image *)png_image->copy();
            } else {
                Logger::warn("PNG image has invalid dimensions (" + std::to_string(png_image->w()) + "x" +
                             std::to_string(png_image->h()) + ") for URL: " + url);
            }
        } else {
            Logger::warn("Failed to create PNG image object for URL: " + url);
        }
    } else if (format == "gif") {
        std::string tempFile = generateTempFilename("gif");

        std::ofstream out(tempFile, std::ios::binary);
        if (out.is_open()) {
            out.write(data.data(), data.size());
            out.close();

            std::unique_ptr<Fl_GIF_Image> gif_image = std::make_unique<Fl_GIF_Image>(tempFile.c_str());

            if (gif_image) {
                if (gif_image->w() > 0 && gif_image->h() > 0) {
                    Fl_RGB_Image *result = (Fl_RGB_Image *)gif_image->copy();
                    std::remove(tempFile.c_str());
                    return result;
                } else {
                    Logger::warn("GIF image has invalid dimensions (" + std::to_string(gif_image->w()) + "x" +
                                 std::to_string(gif_image->h()) + ") for URL: " + url);
                }
            } else {
                Logger::warn("Failed to create GIF image object for URL: " + url);
            }

            std::remove(tempFile.c_str());
        } else {
            Logger::warn("Failed to create temp file for GIF decoding: " + tempFile);
        }
    } else {
        Logger::warn("Unsupported image format: " + format + " for URL: " + url);
    }

    return nullptr;
}

void processImageRequest(const std::string &url, ImageCallback callback) {
    // Check if this URL previously failed (404, etc.)
    if (!shouldAttemptDownload(url)) {
        auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
        Fl::awake(
            [](void *p) {
                std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                (*fnPtr)();
            },
            heapFn);
        return;
    }

    std::vector<std::string> formats = {"png", "gif", "jpeg", "webp"};
    std::string cachedData;

    for (const auto &format : formats) {
        cachedData = loadFromCache(url, format);
        if (!cachedData.empty()) {
            Fl_RGB_Image *image = imageFromData(cachedData, url);
            if (image) {
                {
                    std::scoped_lock lock(cache_mutex);
                    image_cache[url] = std::unique_ptr<Fl_RGB_Image>(image);
                }
                Logger::debug("Loaded from disk cache: " + url);
                auto *heapFn = new std::function<void()>([callback, url]() {
                    Fl_RGB_Image *cachedImg = getCachedImage(url);
                    callback(cachedImg);
                });
                Fl::awake(
                    [](void *p) {
                        std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                        (*fnPtr)();
                    },
                    heapFn);
                return;
            }
        }
    }

    Logger::debug("Downloading image: " + url);
    ix::HttpClient httpClient;
    httpClient.setTLSOptions(ix::SocketTLSOptions());

    ix::HttpRequestArgsPtr args = httpClient.createRequest();
    args->extraHeaders["User-Agent"] = "Discove/1.0";
    args->connectTimeout = 10;
    args->transferTimeout = 30;

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

        if (response->statusCode == 404 || response->statusCode == 403 || response->statusCode == 410) {
            markUrlAsFailed(url);
        }

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

    if (imageData.empty()) {
        Logger::error("Downloaded image has no data: " + url);
        auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
        Fl::awake(
            [](void *p) {
                std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                (*fnPtr)();
            },
            heapFn);
        return;
    }

    std::string format = detectImageFormat(imageData);
    if (format == "unknown") {
        Logger::error("Unknown image format for: " + url + " (size: " + std::to_string(imageData.size()) + " bytes)");
    } else {
        saveToCache(url, imageData, format);
    }

    Fl_RGB_Image *image = imageFromData(imageData, url);
    if (!image) {
        Logger::error("Failed to decode image from: " + url + " (format: " + format +
                      ", size: " + std::to_string(imageData.size()) + " bytes)");
        auto *heapFn = new std::function<void()>([callback]() { callback(nullptr); });
        Fl::awake(
            [](void *p) {
                std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
                (*fnPtr)();
            },
            heapFn);
        return;
    }

    clearFailedUrl(url);
    {
        std::scoped_lock lock(cache_mutex);
        image_cache[url] = std::unique_ptr<Fl_RGB_Image>(image);
    }
    Logger::debug("Successfully downloaded and cached image: " + url);
    auto *heapFn = new std::function<void()>([callback, url]() {
        Fl_RGB_Image *cachedImg = getCachedImage(url);
        callback(cachedImg);
    });
    Fl::awake(
        [](void *p) {
            std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
            (*fnPtr)();
        },
        heapFn);
}

void downloadWorker() {
    active_workers++;

    while (worker_running) {
        DownloadRequest request;
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            if (!queue_cv.wait_for(lock, std::chrono::seconds(1),
                                   [] { return !download_queue.empty() || !worker_running; })) {
                if (download_queue.empty()) {
                    break;
                }
                continue;
            }

            if (!worker_running && download_queue.empty()) {
                break;
            }

            if (!download_queue.empty()) {
                request = std::move(download_queue.front());
                download_queue.pop();
            } else {
                continue;
            }
        }

        processImageRequest(request.url, request.callback);
        {
            std::scoped_lock lock(queue_mutex);
            pending_downloads.erase(request.url);
        }
    }

    active_workers--;
}

void downloadImageAsync(const std::string &url, ImageCallback callback) {
    {
        std::scoped_lock lock(queue_mutex);
        if (pending_downloads.find(url) != pending_downloads.end()) {
            return;
        }

        pending_downloads.insert(url);
        download_queue.push({url, callback});
        Logger::debug("Queued image download (" + std::to_string(download_queue.size()) + " in queue): " + url);
    }

    queue_cv.notify_one();
    if (active_workers < MAX_CONCURRENT_DOWNLOADS) {
        std::thread(downloadWorker).detach();
    }
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

    downloadImageAsync(url, callback);
}

Fl_RGB_Image *getCachedImage(const std::string &url) {
    std::scoped_lock lock(cache_mutex);
    auto it = image_cache.find(url);
    if (it != image_cache.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::string getCacheFilePath(const std::string &url, const std::string &extension) {
    std::string cacheDir = getCacheDirectory();
    std::string filename = urlToFilename(url) + "." + extension;
    return cacheDir + "/" + filename;
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

void shutdownDownloadWorker() {
    worker_running = false;
    queue_cv.notify_all();

    while (active_workers > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
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
        Logger::error("makeRoundedRect: Invalid input parameters");
        return nullptr;
    }

    int maxRadius = std::min(width, height) / 2;
    radius = std::min(radius, maxRadius);

    Fl_RGB_Image *scaled = source;
    bool needsDelete = false;

    if (source->w() != width || source->h() != height) {
        scaled = (Fl_RGB_Image *)source->copy(width, height);
        needsDelete = true;
        if (!scaled) {
            Logger::error("makeRoundedRect: Failed to scale image from " + std::to_string(source->w()) + "x" +
                          std::to_string(source->h()) + " to " + std::to_string(width) + "x" + std::to_string(height));
            return nullptr;
        }
    }

    const unsigned char *srcData = (const unsigned char *)scaled->data()[0];
    if (!srcData) {
        Logger::error("makeRoundedRect: Source image has no data");
        if (needsDelete)
            delete scaled;
        return nullptr;
    }

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
