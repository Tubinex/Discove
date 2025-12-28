#include "ui/components/GuildIcon.h"

#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include <cmath>
#include <filesystem>

#include "ui/GifAnimation.h"
#include "ui/Theme.h"
#include "utils/CDN.h"
#include "utils/Images.h"
#include "utils/Logger.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

GuildIcon::GuildIcon(int x, int y, int size, const std::string &guildId, const std::string &iconHash,
                     const std::string &guildName)
    : Fl_Box(x, y, size, size), guildId_(guildId), iconHash_(iconHash), iconSize_(size) {
    box(FL_NO_BOX);

    isAnimated_ = !iconHash.empty() && iconHash.rfind("a_", 0) == 0;

    if (!guildName.empty()) {
        fallbackLabel_ = guildName.substr(0, 1);
    } else {
        fallbackLabel_ = "?";
    }

    // Load static icon asynchronously
    if (!iconHash.empty()) {
        std::string url = CDNUtils::getGuildIconUrl(guildId, iconHash, size * 2, false);
        Images::loadImageAsync(url, [this](Fl_RGB_Image *img) {
            if (img && img->w() > 0 && img->h() > 0) {
                image_ = img;
                redraw();
            }
        });
    }
}

GuildIcon::~GuildIcon() {
    stopAnimation();
}

int GuildIcon::handle(int event) {
    switch (event) {
    case FL_ENTER:
        isHovered_ = true;
        if (isAnimated_ && ensureGifLoaded()) {
            startAnimation();
        }
        redraw();
        return 1;

    case FL_LEAVE:
        isHovered_ = false;
        if (isAnimated_ && gifAnimation_) {
            stopAnimation();
        }
        redraw();
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

void GuildIcon::startAnimation() {
    if (!animationRunning_ && gifAnimation_ && gifAnimation_->isAnimated()) {
        animationRunning_ = true;
        Fl::add_timeout(gifAnimation_->currentDelay() / 1000.0, animationCallback, this);
    }
}

void GuildIcon::stopAnimation() {
    if (animationRunning_) {
        Fl::remove_timeout(animationCallback, this);
        animationRunning_ = false;
    }
}

void GuildIcon::animationCallback(void *data) {
    auto *self = static_cast<GuildIcon *>(data);
    if (self) {
        self->updateAnimation();
    }
}

void GuildIcon::updateAnimation() {
    if (gifAnimation_ && gifAnimation_->isAnimated() && animationRunning_) {
        gifAnimation_->nextFrame();
        redraw();
        Fl::repeat_timeout(gifAnimation_->currentDelay() / 1000.0, animationCallback, this);
    }
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
    if (isSelected_) {
        fl_color(FL_WHITE);
        int barHeight = h();
        int barY = y();
        fl_rectf(x() - 8, barY, 4, barHeight);
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
