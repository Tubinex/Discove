#pragma once

#include <FL/Fl_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GifAnimation {
  public:
    enum class ScalingStrategy {
        None,
        Lazy,
        Eager
    };

    explicit GifAnimation(const std::string &filepath, ScalingStrategy strategy = ScalingStrategy::Lazy);
    ~GifAnimation();

    GifAnimation(const GifAnimation &) = delete;
    GifAnimation &operator=(const GifAnimation &) = delete;
    GifAnimation(GifAnimation &&) noexcept = default;
    GifAnimation &operator=(GifAnimation &&) noexcept = default;

    Fl_RGB_Image *currentFrame() const;
    Fl_Image *getScaledFrame(int width, int height);
    Fl_RGB_Image *getFrame(size_t index) const;

    int baseWidth() const { return width_; }
    int baseHeight() const { return height_; }

    int currentDelay() const;
    void nextFrame();
    void previousFrame();
    void setFrame(size_t index);
    size_t getCurrentFrameIndex() const { return currentFrameIndex_; }
    size_t frameCount() const { return frames_.size(); }

    bool isValid() const { return !frames_.empty(); }
    bool isAnimated() const { return frames_.size() > 1; }
    const std::string &getLastError() const { return lastError_; }

    void setScalingStrategy(ScalingStrategy strategy);
    void clearScaledCache();

  private:
    struct Frame {
        std::unique_ptr<Fl_RGB_Image> image;
        int delay = 100;
    };

    struct ScaledFrameKey {
        int width;
        int height;

        bool operator==(const ScaledFrameKey &other) const {
            return width == other.width && height == other.height;
        }
    };

    struct ScaledFrameKeyHash {
        std::size_t operator()(const ScaledFrameKey &key) const {
            return std::hash<int>()(key.width) ^ (std::hash<int>()(key.height) << 1);
        }
    };

    bool loadGif(const std::string &filepath);
    Fl_Image *getOrCreateScaledFrame(size_t frameIndex, int width, int height);
    void preScaleAllFrames(int width, int height);

    std::vector<Frame> frames_;
    size_t currentFrameIndex_ = 0;
    int width_ = 0;
    int height_ = 0;

    ScalingStrategy scalingStrategy_ = ScalingStrategy::Lazy;
    std::unordered_map<ScaledFrameKey, std::vector<std::unique_ptr<Fl_Image>>, ScaledFrameKeyHash> scaledCache_;
    std::string lastError_;

    int lastScaledWidth_ = 0;
    int lastScaledHeight_ = 0;
    std::vector<std::unique_ptr<Fl_Image>> *lastScaledCache_ = nullptr;
};
