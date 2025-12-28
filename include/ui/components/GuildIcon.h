#pragma once

#include <FL/Fl_Box.H>
#include <functional>
#include <memory>
#include <string>

class GifAnimation;

class GuildIcon : public Fl_Box {
  public:
    GuildIcon(int x, int y, int size, const std::string &guildId, const std::string &iconHash,
              const std::string &guildName);
    ~GuildIcon() override;

    void setOnClickCallback(std::function<void(const std::string &)> callback) {
        onClickCallback_ = std::move(callback);
    }

    const std::string &guildId() const { return guildId_; }

    void setSelected(bool selected) {
        isSelected_ = selected;
        redraw();
    }
    bool isSelected() const { return isSelected_; }

    void setCornerRadius(int radius) {
        cornerRadius_ = radius;
        redraw();
    }

    void setMaskColor(Fl_Color color) {
        maskColor_ = color;
        redraw();
    }

    void setFallbackFontSize(int size) { fallbackFontSize_ = size; }

  private:
    void draw() override;
    int handle(int event) override;
    static void animationCallback(void *data);
    void updateAnimation();
    void startAnimation();
    void stopAnimation();
    bool ensureGifLoaded();

    Fl_Image *image_{nullptr};
    std::unique_ptr<GifAnimation> gifAnimation_;
    std::string fallbackLabel_;
    int fallbackFontSize_{20};
    std::string guildId_;
    std::string iconHash_;
    int iconSize_{0};
    int cornerRadius_{12};
    Fl_Color maskColor_{0};
    bool isAnimated_{false};
    bool isHovered_{false};
    bool animationRunning_{false};
    bool isSelected_{false};
    std::function<void(const std::string &)> onClickCallback_;
};
