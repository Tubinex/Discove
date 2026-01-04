#include "ui/components/GuildIcon.h"

#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include "ui/components/GuildFolderWidget.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <unordered_set>

#include "ui/GifAnimation.h"
#include "ui/LayoutConstants.h"
#include "ui/Theme.h"
#include "ui/components/GuildBar.h"
#include "utils/CDN.h"
#include "utils/Images.h"
#include "utils/Logger.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
std::unordered_set<GuildIcon *> validIcons;
std::mutex iconsMutex;

int getGuildBarLeftX(const Fl_Widget *widget) {
    const Fl_Widget *current = widget;
    while (current) {
        if (dynamic_cast<const GuildBar *>(current)) {
            return current->x() + LayoutConstants::kGuildIndicatorInset;
        }
        current = current->parent();
    }
    return widget ? widget->x() : 0;
}

void invalidateIndicatorArea(Fl_Widget *widget, int height) {
    if (!widget) {
        return;
    }

    int leftX = getGuildBarLeftX(widget);
    int rightX = widget->x() + widget->w();
    int damageW = std::max(0, rightX - leftX);
    widget->damage(FL_DAMAGE_USER1, leftX, widget->y(), damageW, height);
}

void drawIndicatorBar(int leftX, int y, int height, Fl_Color color) {
    int barHeight = height;
    const int width = LayoutConstants::kGuildIndicatorWidth;
    if (barHeight < width) {
        barHeight = width;
    }

    fl_color(color);
    if (barHeight <= width) {
        fl_rectf(leftX, y, width, barHeight);
        return;
    }

    int radius = width / 2;
    fl_rectf(leftX, y, width - radius, barHeight);
    fl_rectf(leftX + width - radius, y + radius, radius, barHeight - (radius * 2));
    fl_pie(leftX + width - (radius * 2), y, radius * 2, radius * 2, 0, 90);
    fl_pie(leftX + width - (radius * 2), y + barHeight - (radius * 2), radius * 2, radius * 2, 270, 360);
}
} // namespace

GuildIcon::GuildIcon(int x, int y, int size, const std::string &guildId, const std::string &iconHash,
                     const std::string &guildName)
    : Fl_Box(x, y, size, size), guildId_(guildId), iconHash_(iconHash), iconSize_(size) {
    box(FL_NO_BOX);

    {
        std::scoped_lock lock(iconsMutex);
        validIcons.insert(this);
    }

    isAnimated_ = !iconHash.empty() && iconHash.rfind("a_", 0) == 0;

    if (!guildName.empty()) {
        fallbackLabel_ = guildName.substr(0, 1);
    } else {
        fallbackLabel_ = "?";
    }

    if (!iconHash.empty()) {
        std::string url = CDNUtils::getGuildIconUrl(guildId, iconHash, size * 2, false);
        Images::loadImageAsync(url, [this](Fl_RGB_Image *img) {
            {
                std::scoped_lock lock(iconsMutex);
                if (validIcons.find(this) == validIcons.end()) {
                    return;
                }
            }
            if (img && img->w() > 0 && img->h() > 0) {
                image_ = img;
                redraw();
            }
        });
    }
}

GuildIcon::~GuildIcon() {
    {
        std::scoped_lock lock(iconsMutex);
        validIcons.erase(this);
    }
    stopAnimation();
    if (indicatorAnimationId_ != 0) {
        AnimationManager::get().unregisterAnimation(indicatorAnimationId_);
        indicatorAnimationId_ = 0;
    }
}

int GuildIcon::handle(int event) {
    switch (event) {
    case FL_ENTER:
        isHovered_ = true;
        if (isAnimated_ && ensureGifLoaded()) {
            startAnimation();
        }
        startIndicatorAnimation();
        return 1;

    case FL_LEAVE:
        isHovered_ = false;
        if (isAnimated_ && gifAnimation_) {
            stopAnimation();
        }
        startIndicatorAnimation();
        return 1;

    case FL_PUSH:
        if (onClickCallback_) {
            onClickCallback_(guildId_);
        }
        return 1;

    default:
        return Fl_Box::handle(event);
    }
}

void GuildIcon::setSelected(bool selected) {
    if (isSelected_ == selected) {
        return;
    }
    isSelected_ = selected;
    startIndicatorAnimation();
    if (auto *folder = dynamic_cast<GuildFolderWidget *>(parent())) {
        folder->notifyChildSelectionChanged();
    }
}

void GuildIcon::setIndicatorsEnabled(bool enabled) {
    if (indicatorsEnabled_ == enabled) {
        return;
    }
    indicatorsEnabled_ = enabled;
    startIndicatorAnimation();
}

float GuildIcon::indicatorTargetHeight() const {
    if (!indicatorsEnabled_) {
        return 0.0f;
    }
    if (isSelected_) {
        return static_cast<float>(h());
    }
    if (isHovered_) {
        return static_cast<float>(
            std::max(LayoutConstants::kGuildHoverIndicatorMinHeight, h() / 2));
    }
    return 0.0f;
}

void GuildIcon::startIndicatorAnimation() {
    float target = indicatorTargetHeight();
    if (std::abs(indicatorHeight_ - target) <= 0.5f) {
        indicatorHeight_ = target;
        return;
    }
    if (indicatorAnimationId_ != 0) {
        return;
    }
    indicatorAnimationId_ = AnimationManager::get().registerAnimation([this]() {
        return updateIndicatorAnimation();
    });
}

bool GuildIcon::updateIndicatorAnimation() {
    float target = indicatorTargetHeight();
    float delta = LayoutConstants::kGuildIndicatorAnimationSpeed * AnimationManager::get().getFrameTime();

    if (indicatorHeight_ < target) {
        indicatorHeight_ = std::min(indicatorHeight_ + delta, target);
    } else if (indicatorHeight_ > target) {
        indicatorHeight_ = std::max(indicatorHeight_ - delta, target);
    }

    invalidateIndicatorArea(this, h());

    if (std::abs(indicatorHeight_ - target) <= 0.5f) {
        indicatorHeight_ = target;
        indicatorAnimationId_ = 0;
        return false;
    }

    return true;
}

void GuildIcon::startAnimation() {
    if (!animationRunning_ && gifAnimation_ && gifAnimation_->isAnimated()) {
        animationRunning_ = true;
        animationId_ = AnimationManager::get().registerAnimation([this]() { return updateAnimation(); });
    }
}

void GuildIcon::stopAnimation() {
    if (animationRunning_) {
        if (animationId_ != 0) {
            AnimationManager::get().unregisterAnimation(animationId_);
            animationId_ = 0;
        }
        animationRunning_ = false;
    }
}

bool GuildIcon::updateAnimation() {
    if (!gifAnimation_ || !gifAnimation_->isAnimated() || !animationRunning_) {
        animationId_ = 0;
        animationRunning_ = false;
        return false;
    }

    if (window() && !window()->shown()) {
        return true;
    }

    frameTimeAccumulated_ += AnimationManager::get().getFrameTime();
    double requiredDelay = gifAnimation_->currentDelay() / 1000.0;

    if (frameTimeAccumulated_ >= requiredDelay) {
        gifAnimation_->nextFrame();
        frameTimeAccumulated_ = 0.0;

        if (visible_r()) {
            redraw();
        }
    }

    return true;
}

bool GuildIcon::ensureGifLoaded() {
    if (!isAnimated_)
        return false;
    if (gifAnimation_)
        return true;

    std::string url = CDNUtils::getGuildIconUrl(guildId_, iconHash_, iconSize_ * 2, false);
    std::string gifPath = Images::getCacheFilePath(url, "gif");

    if (!std::filesystem::exists(gifPath)) {
        Logger::debug("GIF not yet cached for animated icon: " + guildId_);
        return false;
    }

    try {
        gifAnimation_ = std::make_unique<GifAnimation>(gifPath, GifAnimation::ScalingStrategy::Lazy);
        if (!gifAnimation_->isValid()) {
            Logger::warn("Failed to load GIF animation from: " + gifPath);
            gifAnimation_.reset();
            return false;
        }
        Logger::debug("Loaded GIF animation for guild: " + guildId_);
        return true;
    } catch (const std::exception &e) {
        Logger::error("Exception loading GIF animation: " + std::string(e.what()));
        gifAnimation_.reset();
        return false;
    }
}

void GuildIcon::draw() {
    if (indicatorHeight_ > 0.5f) {
        int indicatorHeight = static_cast<int>(indicatorHeight_);
        int indicatorY = y() + (h() - indicatorHeight) / 2;
        drawIndicatorBar(getGuildBarLeftX(this), indicatorY, indicatorHeight, ThemeColors::TEXT_NORMAL);
    }

    Fl_Image *displayImage = nullptr;
    if (isAnimated_ && isHovered_ && gifAnimation_) {
        displayImage = gifAnimation_->currentFrame();
    } else {
        displayImage = image_;
    }

    if (displayImage && !displayImage->fail()) {
        Fl_Image *scaledImage = displayImage;
        if (displayImage->w() != w() || displayImage->h() != h()) {
            scaledImage = displayImage->copy(w(), h());
        }

        scaledImage->draw(x(), y());

        if (scaledImage != displayImage) {
            delete scaledImage;
        }

        Fl_Color maskColor = maskColor_ != 0 ? maskColor_ : ThemeColors::BG_SECONDARY;
        fl_color(maskColor);

        const int r = cornerRadius_;
        const int steps = 45;

        fl_begin_polygon();
        fl_vertex(x() - 1, y() - 1);
        fl_vertex(x() + r + 1, y() - 1);
        for (int i = 1; i < steps; i++) {
            double angle = M_PI * 1.5 - (M_PI * 0.5 * i / steps);
            fl_vertex(x() + r + std::cos(angle) * r, y() + r + std::sin(angle) * r);
        }
        fl_vertex(x() - 1, y() + r + 1);
        fl_end_polygon();

        fl_begin_polygon();
        fl_vertex(x() + w() - r - 1, y() - 1);
        fl_vertex(x() + w() + 1, y() - 1);
        fl_vertex(x() + w() + 1, y() + r + 1);
        for (int i = 1; i < steps; i++) {
            double angle = M_PI * 2.0 - (M_PI * 0.5 * i / steps);
            fl_vertex(x() + w() - r + std::cos(angle) * r, y() + r + std::sin(angle) * r);
        }
        fl_end_polygon();

        fl_begin_polygon();
        fl_vertex(x() - 1, y() + h() - r - 1);
        for (int i = 1; i < steps; i++) {
            double angle = M_PI - (M_PI * 0.5 * i / steps);
            fl_vertex(x() + r + std::cos(angle) * r, y() + h() - r + std::sin(angle) * r);
        }
        fl_vertex(x() + r + 1, y() + h() + 1);
        fl_vertex(x() - 1, y() + h() + 1);
        fl_end_polygon();

        fl_begin_polygon();
        fl_vertex(x() + w() + 1, y() + h() - r - 1);
        fl_vertex(x() + w() + 1, y() + h() + 1);
        fl_vertex(x() + w() - r - 1, y() + h() + 1);
        for (int i = 1; i < steps; i++) {
            double angle = M_PI * 0.5 - (M_PI * 0.5 * i / steps);
            fl_vertex(x() + w() - r + std::cos(angle) * r, y() + h() - r + std::sin(angle) * r);
        }
        fl_end_polygon();
    } else {
        fl_color(ThemeColors::BG_TERTIARY);

        const int r = cornerRadius_;
        fl_pie(x(), y(), r * 2, r * 2, 90, 180);
        fl_pie(x() + w() - r * 2, y(), r * 2, r * 2, 0, 90);
        fl_pie(x(), y() + h() - r * 2, r * 2, r * 2, 180, 270);
        fl_pie(x() + w() - r * 2, y() + h() - r * 2, r * 2, r * 2, 270, 360);
        fl_rectf(x() + r, y(), w() - r * 2, h());
        fl_rectf(x(), y() + r, w(), h() - r * 2);

        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FL_HELVETICA_BOLD, fallbackFontSize_);
        fl_draw(fallbackLabel_.c_str(), x(), y(), w(), h(), FL_ALIGN_CENTER);
    }
}
