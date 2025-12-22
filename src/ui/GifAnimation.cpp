#include "ui/GifAnimation.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <gdiplus.h>
#include <objidl.h>
#include <windows.h>

using namespace Gdiplus;

class GdiplusInitializer {
  public:
    static GdiplusInitializer &getInstance() {
        static GdiplusInitializer instance;
        return instance;
    }

    bool isInitialized() const { return initialized_; }

  private:
    GdiplusInitializer() {
        GdiplusStartupInput input;
        if (GdiplusStartup(&token_, &input, nullptr) == Ok) {
            initialized_ = true;
        }
    }

    ~GdiplusInitializer() {
        if (initialized_) {
            GdiplusShutdown(token_);
        }
    }

    GdiplusInitializer(const GdiplusInitializer &) = delete;
    GdiplusInitializer &operator=(const GdiplusInitializer &) = delete;

    ULONG_PTR token_ = 0;
    bool initialized_ = false;
};

template <typename T> struct GdiplusDeleter {
    void operator()(T *ptr) const { delete ptr; }
};

using BitmapPtr = std::unique_ptr<Bitmap, GdiplusDeleter<Bitmap>>;
using ImagePtr = std::unique_ptr<Image, GdiplusDeleter<Image>>;
using GraphicsPtr = std::unique_ptr<Graphics, GdiplusDeleter<Graphics>>;

#else
#include <gif_lib.h>
#include <vector>
#endif

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

GifAnimation::GifAnimation(const std::string &filepath, ScalingStrategy strategy) : scalingStrategy_(strategy) {
    if (!loadGif(filepath)) {
        if (lastError_.empty()) {
            lastError_ = "Failed to load GIF: " + filepath;
        }
    }
}

GifAnimation::~GifAnimation() = default;

Fl_RGB_Image *GifAnimation::currentFrame() const {
    if (frames_.empty() || currentFrameIndex_ >= frames_.size()) {
        return nullptr;
    }
    return frames_[currentFrameIndex_].image.get();
}

Fl_RGB_Image *GifAnimation::getFrame(size_t index) const {
    if (index >= frames_.size()) {
        return nullptr;
    }
    return frames_[index].image.get();
}

Fl_Image *GifAnimation::getScaledFrame(int width, int height) {
    if (width <= 0 || height <= 0) {
        return currentFrame();
    }

    if (frames_.empty() || currentFrameIndex_ >= frames_.size()) {
        return nullptr;
    }

    if (scalingStrategy_ == ScalingStrategy::None) {
        return frames_[currentFrameIndex_].image.get();
    }

    if (width == lastScaledWidth_ && height == lastScaledHeight_ && lastScaledCache_) {
        if (currentFrameIndex_ < lastScaledCache_->size() && (*lastScaledCache_)[currentFrameIndex_]) {
            return (*lastScaledCache_)[currentFrameIndex_].get();
        }
    }

    return getOrCreateScaledFrame(currentFrameIndex_, width, height);
}

int GifAnimation::currentDelay() const {
    if (frames_.empty() || currentFrameIndex_ >= frames_.size()) {
        return 100;
    }
    return frames_[currentFrameIndex_].delay;
}

void GifAnimation::nextFrame() {
    if (frames_.size() <= 1) {
        return;
    }
    currentFrameIndex_ = (currentFrameIndex_ + 1) % frames_.size();
}

void GifAnimation::previousFrame() {
    if (frames_.size() <= 1) {
        return;
    }
    currentFrameIndex_ = (currentFrameIndex_ == 0) ? frames_.size() - 1 : currentFrameIndex_ - 1;
}

void GifAnimation::setFrame(size_t index) {
    if (index < frames_.size()) {
        currentFrameIndex_ = index;
    }
}

void GifAnimation::setScalingStrategy(ScalingStrategy strategy) {
    if (scalingStrategy_ != strategy) {
        scalingStrategy_ = strategy;
        scaledCache_.clear();
        lastScaledCache_ = nullptr;
    }
}

void GifAnimation::clearScaledCache() {
    scaledCache_.clear();
    lastScaledCache_ = nullptr;
    lastScaledWidth_ = 0;
    lastScaledHeight_ = 0;
}

Fl_Image *GifAnimation::getOrCreateScaledFrame(size_t frameIndex, int width, int height) {
    if (frameIndex >= frames_.size() || !frames_[frameIndex].image) {
        return nullptr;
    }

    ScaledFrameKey key{width, height};
    auto it = scaledCache_.find(key);
    if (it != scaledCache_.end()) {
        auto &frameCache = it->second;
        lastScaledWidth_ = width;
        lastScaledHeight_ = height;
        lastScaledCache_ = &frameCache;

        if (frameIndex < frameCache.size() && frameCache[frameIndex]) {
            return frameCache[frameIndex].get();
        }
    }

    if (scalingStrategy_ == ScalingStrategy::Eager && it == scaledCache_.end()) {
        preScaleAllFrames(width, height);
        it = scaledCache_.find(key);
        if (it != scaledCache_.end() && frameIndex < it->second.size()) {
            lastScaledWidth_ = width;
            lastScaledHeight_ = height;
            lastScaledCache_ = &it->second;
            return it->second[frameIndex].get();
        }
    }

    if (it == scaledCache_.end()) {
        scaledCache_[key] = std::vector<std::unique_ptr<Fl_Image>>(frames_.size());
        it = scaledCache_.find(key);
    }

    auto &frameCache = it->second;
    if (frameIndex >= frameCache.size()) {
        frameCache.resize(frames_.size());
    }

    Fl_Image *scaled = frames_[frameIndex].image->copy(width, height);
    if (!scaled) {
        return frames_[frameIndex].image.get();
    }

    frameCache[frameIndex].reset(scaled);

    lastScaledWidth_ = width;
    lastScaledHeight_ = height;
    lastScaledCache_ = &frameCache;

    return scaled;
}

void GifAnimation::preScaleAllFrames(int width, int height) {
    ScaledFrameKey key{width, height};
    auto &frameCache = scaledCache_[key];
    frameCache.clear();
    frameCache.reserve(frames_.size());

    for (const auto &frame : frames_) {
        if (!frame.image) {
            frameCache.push_back(nullptr);
            continue;
        }

        Fl_Image *scaled = frame.image->copy(width, height);
        frameCache.emplace_back(scaled);
    }
}

#ifdef _WIN32

bool GifAnimation::loadGif(const std::string &filepath) {
    if (!GdiplusInitializer::getInstance().isInitialized()) {
        lastError_ = "GDI+ initialization failed";
        return false;
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        lastError_ = "Failed to convert filepath to wide string";
        return false;
    }

    std::wstring wpath(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, filepath.c_str(), -1, &wpath[0], size);

    ImagePtr image(new Image(wpath.c_str()));
    if (!image || image->GetLastStatus() != Ok) {
        lastError_ = "Failed to load image file";
        return false;
    }

    UINT dimensionCount = image->GetFrameDimensionsCount();
    if (dimensionCount == 0) {
        lastError_ = "No frame dimensions found";
        return false;
    }

    std::vector<GUID> dimensionIDs(dimensionCount);
    image->GetFrameDimensionsList(dimensionIDs.data(), dimensionCount);

    UINT frameCount = image->GetFrameCount(&dimensionIDs[0]);
    if (frameCount == 0) {
        lastError_ = "No frames found in image";
        return false;
    }

    std::vector<int> delays;
    UINT propSize = image->GetPropertyItemSize(PropertyTagFrameDelay);
    if (propSize > 0) {
        std::vector<unsigned char> propBuffer(propSize);
        PropertyItem *propItem = reinterpret_cast<PropertyItem *>(propBuffer.data());

        if (image->GetPropertyItem(PropertyTagFrameDelay, propSize, propItem) == Ok) {
            UINT *delayArray = static_cast<UINT *>(propItem->value);
            for (UINT i = 0; i < frameCount; i++) {
                int delay = delayArray[i] * 10;
                delays.push_back(delay == 0 ? 100 : delay);
            }
        }
    }

    while (delays.size() < frameCount) {
        delays.push_back(100);
    }

    width_ = image->GetWidth();
    height_ = image->GetHeight();

    frames_.reserve(frameCount);
    for (UINT frameIdx = 0; frameIdx < frameCount; frameIdx++) {
        if (image->SelectActiveFrame(&dimensionIDs[0], frameIdx) != Ok) {
            lastError_ = "Failed to select frame " + std::to_string(frameIdx);
            continue;
        }

        int frameWidth = image->GetWidth();
        int frameHeight = image->GetHeight();

        BitmapPtr bitmap(new Bitmap(frameWidth, frameHeight, PixelFormat32bppARGB));
        if (!bitmap || bitmap->GetLastStatus() != Ok) {
            lastError_ = "Failed to create bitmap for frame " + std::to_string(frameIdx);
            continue;
        }

        GraphicsPtr graphics(Graphics::FromImage(bitmap.get()));
        if (!graphics || graphics->GetLastStatus() != Ok) {
            lastError_ = "Failed to create graphics for frame " + std::to_string(frameIdx);
            continue;
        }

        if (graphics->DrawImage(image.get(), 0, 0, frameWidth, frameHeight) != Ok) {
            lastError_ = "Failed to draw frame " + std::to_string(frameIdx);
            continue;
        }

        BitmapData bitmapData;
        Rect rect(0, 0, frameWidth, frameHeight);

        if (bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Ok) {
            lastError_ = "Failed to lock bitmap bits for frame " + std::to_string(frameIdx);
            continue;
        }

        unsigned char *fltkData = new unsigned char[frameWidth * frameHeight * 4];
        unsigned char *src = static_cast<unsigned char *>(bitmapData.Scan0);

        for (int y = 0; y < frameHeight; y++) {
            for (int x = 0; x < frameWidth; x++) {
                int srcIdx = y * bitmapData.Stride + x * 4;
                int dstIdx = (y * frameWidth + x) * 4;

                fltkData[dstIdx + 0] = src[srcIdx + 2];
                fltkData[dstIdx + 1] = src[srcIdx + 1];
                fltkData[dstIdx + 2] = src[srcIdx + 0];
                fltkData[dstIdx + 3] = src[srcIdx + 3];
            }
        }

        bitmap->UnlockBits(&bitmapData);

        auto *fltkImage = new Fl_RGB_Image(fltkData, frameWidth, frameHeight, 4);
        fltkImage->alloc_array = 1;

        Frame frame;
        frame.image.reset(fltkImage);
        frame.delay = delays[frameIdx];
        frames_.push_back(std::move(frame));
    }

    if (frames_.empty()) {
        lastError_ = "No frames were successfully loaded";
        return false;
    }

    lastError_.clear();
    return true;
}

#else

bool GifAnimation::loadGif(const std::string &filepath) {
    int errorCode = 0;
    GifFileType *gif = DGifOpenFileName(filepath.c_str(), &errorCode);

    if (!gif) {
        lastError_ = "Failed to open GIF file (error " + std::to_string(errorCode) + ")";
        return false;
    }

    if (DGifSlurp(gif) == GIF_ERROR) {
        lastError_ = "Failed to read GIF data";
        DGifCloseFile(gif, &errorCode);
        return false;
    }

    width_ = gif->SWidth;
    height_ = gif->SHeight;

    if (width_ <= 0 || height_ <= 0) {
        lastError_ = "Invalid GIF dimensions";
        DGifCloseFile(gif, &errorCode);
        return false;
    }

    std::vector<unsigned char> canvas(width_ * height_ * 4, 0);
    std::vector<unsigned char> previousCanvas;

    ColorMapObject *globalColorMap = gif->SColorMap;

    frames_.reserve(gif->ImageCount);

    for (int i = 0; i < gif->ImageCount; i++) {
        SavedImage *image = &gif->SavedImages[i];
        GifImageDesc *desc = &image->ImageDesc;

        ColorMapObject *colorMap = desc->ColorMap ? desc->ColorMap : globalColorMap;
        if (!colorMap) {
            lastError_ = "No color map found for frame " + std::to_string(i);
            continue;
        }

        int transparentColor = -1;
        int delayTime = 100;
        int disposalMode = DISPOSE_DO_NOT;

        for (int j = 0; j < image->ExtensionBlockCount; j++) {
            ExtensionBlock *ext = &image->ExtensionBlocks[j];
            if (ext->Function == GRAPHICS_EXT_FUNC_CODE && ext->ByteCount >= 4) {
                unsigned char *bytes = ext->Bytes;
                unsigned char packed = bytes[0];

                delayTime = (bytes[2] << 8) | bytes[1];
                delayTime *= 10;
                if (delayTime == 0) {
                    delayTime = 100;
                }

                disposalMode = (packed >> 2) & 0x07;

                if (packed & 0x01) {
                    transparentColor = bytes[3];
                }
            }
        }

        if (disposalMode == DISPOSE_PREVIOUS) {
            previousCanvas = canvas;
        }

        for (int y = 0; y < desc->Height; y++) {
            for (int x = 0; x < desc->Width; x++) {
                int frameX = desc->Left + x;
                int frameY = desc->Top + y;

                if (frameX >= width_ || frameY >= height_ || frameX < 0 || frameY < 0) {
                    continue;
                }

                int srcIdx = y * desc->Width + x;
                int dstIdx = (frameY * width_ + frameX) * 4;

                unsigned char colorIndex = image->RasterBits[srcIdx];

                if (colorIndex == transparentColor) {
                    continue;
                }

                if (colorIndex >= colorMap->ColorCount) {
                    continue;
                }

                GifColorType color = colorMap->Colors[colorIndex];

                canvas[dstIdx + 0] = color.Red;
                canvas[dstIdx + 1] = color.Green;
                canvas[dstIdx + 2] = color.Blue;
                canvas[dstIdx + 3] = 255;
            }
        }

        unsigned char *frameData = new unsigned char[width_ * height_ * 4];
        std::memcpy(frameData, canvas.data(), width_ * height_ * 4);

        auto *fltkImage = new Fl_RGB_Image(frameData, width_, height_, 4);
        fltkImage->alloc_array = 1;

        Frame frame;
        frame.image.reset(fltkImage);
        frame.delay = delayTime;
        frames_.push_back(std::move(frame));

        switch (disposalMode) {
        case DISPOSE_BACKGROUND:
            for (int y = desc->Top; y < desc->Top + desc->Height && y < height_; y++) {
                for (int x = desc->Left; x < desc->Left + desc->Width && x < width_; x++) {
                    int idx = (y * width_ + x) * 4;
                    canvas[idx + 0] = 0;
                    canvas[idx + 1] = 0;
                    canvas[idx + 2] = 0;
                    canvas[idx + 3] = 0;
                }
            }
            break;

        case DISPOSE_PREVIOUS:
            if (!previousCanvas.empty()) {
                canvas = previousCanvas;
            }
            break;

        case DISPOSE_DO_NOT:
        default:
            break;
        }
    }

    DGifCloseFile(gif, &errorCode);

    if (frames_.empty()) {
        lastError_ = "No frames were successfully loaded";
        return false;
    }

    lastError_.clear();
    return true;
}

#endif
