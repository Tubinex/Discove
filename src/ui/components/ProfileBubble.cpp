#include "ui/components/ProfileBubble.h"

#include "ui/AnimationManager.h"
#include "ui/GifAnimation.h"
#include "ui/IconManager.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Images.h"
#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <filesystem>
#include <sstream>

ProfileBubble::ProfileBubble(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {
    box(FL_NO_BOX);
}

ProfileBubble::~ProfileBubble() {
    if (m_avatarAnimationId != 0) {
        AnimationManager::get().unregisterAnimation(m_avatarAnimationId);
    }
    if (m_emojiAnimationId != 0) {
        AnimationManager::get().unregisterAnimation(m_emojiAnimationId);
    }

    if (m_circularAvatar) {
        delete m_circularAvatar;
        m_circularAvatar = nullptr;
    }

    if (m_customStatusEmoji) {
        delete m_customStatusEmoji;
        m_customStatusEmoji = nullptr;
    }
}

void ProfileBubble::draw() {
    int bubbleX = x() + BUBBLE_MARGIN;
    int bubbleY = y() + BUBBLE_MARGIN;
    int bubbleW = w() - (BUBBLE_MARGIN * 2);
    int bubbleH = h() - (BUBBLE_MARGIN * 2);

    fl_color(ThemeColors::BG_TERTIARY);

    fl_pie(bubbleX, bubbleY, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 90, 180);
    fl_pie(bubbleX + bubbleW - BUBBLE_BORDER_RADIUS * 2, bubbleY, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 0,
           90);
    fl_pie(bubbleX, bubbleY + bubbleH - BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2,
           180, 270);
    fl_pie(bubbleX + bubbleW - BUBBLE_BORDER_RADIUS * 2, bubbleY + bubbleH - BUBBLE_BORDER_RADIUS * 2,
           BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 270, 360);

    fl_rectf(bubbleX + BUBBLE_BORDER_RADIUS, bubbleY, bubbleW - BUBBLE_BORDER_RADIUS * 2, bubbleH);
    fl_rectf(bubbleX, bubbleY + BUBBLE_BORDER_RADIUS, bubbleW, bubbleH - BUBBLE_BORDER_RADIUS * 2);

    fl_color(ThemeColors::BORDER_PRIMARY);
    fl_line_style(FL_SOLID, 1);

    fl_arc(bubbleX, bubbleY, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 90, 180);
    fl_arc(bubbleX + bubbleW - BUBBLE_BORDER_RADIUS * 2, bubbleY, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 0,
           90);
    fl_arc(bubbleX, bubbleY + bubbleH - BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2,
           180, 270);
    fl_arc(bubbleX + bubbleW - BUBBLE_BORDER_RADIUS * 2, bubbleY + bubbleH - BUBBLE_BORDER_RADIUS * 2,
           BUBBLE_BORDER_RADIUS * 2, BUBBLE_BORDER_RADIUS * 2, 270, 360);

    fl_line(bubbleX + BUBBLE_BORDER_RADIUS, bubbleY, bubbleX + bubbleW - BUBBLE_BORDER_RADIUS, bubbleY);
    fl_line(bubbleX + bubbleW, bubbleY + BUBBLE_BORDER_RADIUS, bubbleX + bubbleW,
            bubbleY + bubbleH - BUBBLE_BORDER_RADIUS);
    fl_line(bubbleX + BUBBLE_BORDER_RADIUS, bubbleY + bubbleH, bubbleX + bubbleW - BUBBLE_BORDER_RADIUS,
            bubbleY + bubbleH);
    fl_line(bubbleX, bubbleY + BUBBLE_BORDER_RADIUS, bubbleX, bubbleY + bubbleH - BUBBLE_BORDER_RADIUS);

    fl_line_style(0);

    int avatarX = bubbleX + (bubbleH - AVATAR_SIZE) / 2;
    int avatarY = bubbleY + (bubbleH - AVATAR_SIZE) / 2;
    drawAvatar(avatarX, avatarY);

    int dotX = avatarX + AVATAR_SIZE - STATUS_DOT_SIZE;
    int dotY = avatarY + AVATAR_SIZE - STATUS_DOT_SIZE;
    drawStatusDot(dotX, dotY);

    int textX = avatarX + AVATAR_SIZE + 8;

    bool hasCustomStatus = m_customStatusEmoji || m_emojiGif || !m_customStatus.empty();
    bool hasSecondLine = hasCustomStatus || m_discriminator != "0";

    int usernameBaseline;
    if (hasSecondLine) {
        int totalTextHeight = 31;
        int topOffset = (bubbleH - totalTextHeight) / 2;
        usernameBaseline = bubbleY + topOffset + 14;
    } else {
        usernameBaseline = bubbleY + bubbleH / 2 + 5;
    }

    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 14);
    fl_draw(m_username.c_str(), textX, usernameBaseline);

    if (hasCustomStatus) {
        int statusY = usernameBaseline + 17;
        int currentX = textX;

        Fl_RGB_Image *emojiToShow = nullptr;
        if (m_emojiGif && m_emojiGif->isAnimated() && !m_emojiFrames.empty()) {
            size_t frameIndex = m_emojiGif->getCurrentFrameIndex();
            if (frameIndex < m_emojiFrames.size()) {
                emojiToShow = m_emojiFrames[frameIndex].get();
            }
        } else {
            emojiToShow = m_customStatusEmoji;
        }

        if (emojiToShow && emojiToShow->w() > 0 && emojiToShow->h() > 0) {
            int emojiY = statusY - CUSTOM_STATUS_EMOJI_SIZE + 2;
            emojiToShow->draw(currentX, emojiY);
            currentX += CUSTOM_STATUS_EMOJI_SIZE + 4;
        }

        if (!m_customStatus.empty()) {
            fl_color(ThemeColors::TEXT_MUTED);
            fl_font(FontLoader::Fonts::INTER_REGULAR, 12);
            fl_draw(m_customStatus.c_str(), currentX, statusY);
        }
    } else if (m_discriminator != "0") {
        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 12);
        int discrimBaseline = usernameBaseline + 17;
        fl_draw(("#" + m_discriminator).c_str(), textX, discrimBaseline);
    }

    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - BUTTON_RIGHT_PADDING;

    const char *settingsIcon = "settings";
    drawButton(buttonX, buttonY, settingsIcon, false, m_hoveredButton == 2, false, false);
    buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;

    const char *headphonesIcon = m_headphonesDeafened ? "deafened" : "headphones";
    drawButton(buttonX, buttonY, headphonesIcon, true, m_hoveredButton == 1,
               m_hoveredButton == 1 && m_hoveredButtonChevron, m_headphonesDeafened);
    buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;

    const char *micIcon = m_microphoneMuted ? "muted" : "microphone";
    drawButton(buttonX, buttonY, micIcon, true, m_hoveredButton == 0, m_hoveredButton == 0 && m_hoveredButtonChevron,
               m_microphoneMuted);
}

void ProfileBubble::drawAvatar(int avatarX, int avatarY) {
    fl_color(ThemeColors::BG_PRIMARY);
    fl_pie(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE, 0, 360);

    Fl_RGB_Image *frameToShow = nullptr;

    if (m_avatarGif && m_avatarGif->isAnimated() && !m_circularFrames.empty()) {
        size_t frameIndex = m_avatarGif->getCurrentFrameIndex();
        if (frameIndex < m_circularFrames.size()) {
            frameToShow = m_circularFrames[frameIndex].get();
        }
    } else {
        frameToShow = m_circularAvatar;
    }

    if (frameToShow && frameToShow->w() > 0 && frameToShow->h() > 0) {
        frameToShow->draw(avatarX, avatarY);
    } else {
        if (!m_username.empty()) {
            fl_color(ThemeColors::TEXT_NORMAL);
            fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 16);

            std::string initial(1, m_username[0]);
            int textW = static_cast<int>(fl_width(initial.c_str()));
            int textH = fl_height();

            fl_draw(initial.c_str(), avatarX + (AVATAR_SIZE - textW) / 2,
                    avatarY + (AVATAR_SIZE + textH) / 2 - fl_descent());
        }
    }
}

void ProfileBubble::drawStatusDot(int dotX, int dotY) {
    Fl_Color statusColor;
    if (m_status == "online") {
        statusColor = fl_rgb_color(67, 181, 129);
    } else if (m_status == "dnd") {
        statusColor = fl_rgb_color(240, 71, 71);
    } else if (m_status == "idle") {
        statusColor = fl_rgb_color(250, 166, 26);
    } else {
        statusColor = fl_rgb_color(116, 127, 141);
    }

    fl_color(ThemeColors::BG_TERTIARY);
    int borderSize = STATUS_DOT_SIZE + (STATUS_DOT_BORDER_WIDTH * 2);
    fl_pie(dotX - STATUS_DOT_BORDER_WIDTH, dotY - STATUS_DOT_BORDER_WIDTH, borderSize, borderSize, 0, 360);

    fl_color(statusColor);
    fl_pie(dotX, dotY, STATUS_DOT_SIZE, STATUS_DOT_SIZE, 0, 360);
}

void ProfileBubble::drawButton(int btnX, int btnY, const char *iconName, bool isToggle, bool hovered,
                               bool chevronHovered, bool active) {
    int buttonWidth = isToggle ? (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) : BUTTON_SIZE;

    auto drawRoundedRect = [&](int x, int y, int w, int h, Fl_Color color, bool roundTL = true, bool roundTR = true,
                               bool roundBL = true, bool roundBR = true) {
        fl_color(color);
        constexpr int r = BUBBLE_BORDER_RADIUS;

        if (roundTL)
            fl_pie(x, y, r * 2, r * 2, 90, 180);
        if (roundTR)
            fl_pie(x + w - r * 2, y, r * 2, r * 2, 0, 90);
        if (roundBL)
            fl_pie(x, y + h - r * 2, r * 2, r * 2, 180, 270);
        if (roundBR)
            fl_pie(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 270, 360);

        int leftOffset = roundTL || roundBL ? r : 0;
        int rightOffset = roundTR || roundBR ? r : 0;
        fl_rectf(x + leftOffset, y, w - leftOffset - rightOffset, h);

        if (roundTL || roundBL)
            fl_rectf(x, y + r, leftOffset, h - r * 2);
        if (roundTR || roundBR)
            fl_rectf(x + w - rightOffset, y + r, rightOffset, h - r * 2);

        if (!roundTL)
            fl_rectf(x, y, r, r);
        if (!roundTR)
            fl_rectf(x + w - r, y, r, r);
        if (!roundBL)
            fl_rectf(x, y + h - r, r, r);
        if (!roundBR)
            fl_rectf(x + w - r, y + h - r, r, r);
    };

    if (active && hovered) {
        if (isToggle) {
            drawRoundedRect(btnX, btnY, buttonWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_ACTIVE_HOVER_BASE, true,
                            true, true, true);

            if (chevronHovered) {
                int chevronX = btnX + ICON_SECTION_WIDTH + BUTTON_HOVER_GAP;
                int chevronWidth = CHEVRON_SECTION_WIDTH - BUTTON_HOVER_GAP;
                drawRoundedRect(chevronX, btnY, chevronWidth, BUTTON_SIZE,
                                ThemeColors::PROFILE_BUTTON_ACTIVE_HOVER_STRONG, false, true, false, true);
            } else {
                int iconAreaWidth = ICON_SECTION_WIDTH - BUTTON_HOVER_GAP;
                drawRoundedRect(btnX, btnY, iconAreaWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_ACTIVE_HOVER_STRONG,
                                true, false, true, false);
            }
        } else {
            drawRoundedRect(btnX, btnY, buttonWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_ACTIVE_HOVER_STRONG, true,
                            true, true, true);
        }
    } else if (active) {
        drawRoundedRect(btnX, btnY, buttonWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_ACTIVE_BG, true, true, true,
                        true);
    } else if (hovered) {
        if (isToggle) {
            drawRoundedRect(btnX, btnY, buttonWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_HOVER_BASE, true, true,
                            true, true);

            if (chevronHovered) {
                int chevronX = btnX + ICON_SECTION_WIDTH + BUTTON_HOVER_GAP;
                int chevronWidth = CHEVRON_SECTION_WIDTH - BUTTON_HOVER_GAP;
                drawRoundedRect(chevronX, btnY, chevronWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_HOVER_STRONG,
                                false, true, false, true);
            } else {
                int iconAreaWidth = ICON_SECTION_WIDTH - BUTTON_HOVER_GAP;
                drawRoundedRect(btnX, btnY, iconAreaWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_HOVER_STRONG, true,
                                false, true, false);
            }
        } else {
            drawRoundedRect(btnX, btnY, buttonWidth, BUTTON_SIZE, ThemeColors::PROFILE_BUTTON_HOVER_STRONG, true, true,
                            true, true);
        }
    }

    Fl_Color iconColor = active ? ThemeColors::PROFILE_BUTTON_ACTIVE_ICON : ThemeColors::TEXT_MUTED;
    Fl_SVG_Image *iconImg = IconManager::load_recolored_icon(iconName, ICON_SIZE, iconColor);
    if (iconImg) {
        int iconX = btnX + (ICON_SECTION_WIDTH - ICON_SIZE) / 2;
        int iconY = btnY + (BUTTON_SIZE - ICON_SIZE) / 2;
        iconImg->draw(iconX, iconY);
    }

    if (isToggle) {
        Fl_SVG_Image *arrowImg = IconManager::load_recolored_icon("chevron_down", ARROW_SIZE, iconColor);
        if (arrowImg) {
            int arrowX = btnX + ICON_SECTION_WIDTH + (CHEVRON_SECTION_WIDTH - ARROW_SIZE) / 2;
            int arrowY = btnY + (BUTTON_SIZE - ARROW_SIZE) / 2;
            arrowImg->draw(arrowX, arrowY);
        }
    }
}

int ProfileBubble::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        int button = getButtonAt(Fl::event_x(), Fl::event_y());

        if (button == 0 && m_onMicrophoneClicked) {
            m_microphoneMuted = !m_microphoneMuted;
            m_onMicrophoneClicked();
            redraw();
            return 1;
        } else if (button == 1 && m_onHeadphonesClicked) {
            m_headphonesDeafened = !m_headphonesDeafened;
            m_onHeadphonesClicked();
            redraw();
            return 1;
        } else if (button == 2 && m_onSettingsClicked) {
            m_onSettingsClicked();
            return 1;
        }
        break;
    }

    case FL_MOVE:
    case FL_ENTER:
    case FL_LEAVE: {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        int newHovered = (event == FL_LEAVE) ? -1 : getButtonAt(mx, my);
        bool newChevronHovered = false;

        if (newHovered >= 0 && event != FL_LEAVE) {
            int bubbleX = x() + BUBBLE_MARGIN;
            int bubbleY = y() + BUBBLE_MARGIN;
            int bubbleW = w() - (BUBBLE_MARGIN * 2);
            int bubbleH = h() - (BUBBLE_MARGIN * 2);
            int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
            int buttonX = bubbleX + bubbleW - BUTTON_SIZE - BUTTON_RIGHT_PADDING;

            if (newHovered == 2) {
                newChevronHovered = false;
            } else if (newHovered == 1) {
                buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;
                newChevronHovered = isChevronHovered(buttonX, buttonY, true, mx, my);
            } else if (newHovered == 0) {
                buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;
                buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;
                newChevronHovered = isChevronHovered(buttonX, buttonY, true, mx, my);
            }
        }

        if (newHovered != m_hoveredButton || newChevronHovered != m_hoveredButtonChevron) {
            m_hoveredButton = newHovered;
            m_hoveredButtonChevron = newChevronHovered;
            redraw();
        }
        return 1;
    }
    }

    return Fl_Widget::handle(event);
}

void ProfileBubble::loadAvatar() {
    if (m_avatarAnimationId != 0) {
        AnimationManager::get().unregisterAnimation(m_avatarAnimationId);
        m_avatarAnimationId = 0;
    }

    if (m_circularAvatar) {
        delete m_circularAvatar;
        m_circularAvatar = nullptr;
    }
    m_circularFrames.clear();
    m_avatarGif.reset();
    m_avatarFrameTimeAccumulated = 0.0;

    if (m_avatarUrl.empty()) {
        m_avatarImage = nullptr;
        return;
    }

    std::string urlLower = m_avatarUrl;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    bool isGif = (urlLower.find(".gif") != std::string::npos);

    if (isGif) {
        Images::loadImageAsync(m_avatarUrl, [this](Fl_RGB_Image *image) {
            if (!image) {
                Logger::warn("Failed to load GIF avatar");
                redraw();
                return;
            }
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

            std::hash<std::string> hasher;
            size_t hash = hasher(m_avatarUrl);
            std::ostringstream oss;
            oss << std::hex << hash;
            std::string cacheFile = cacheDir + "/" + oss.str() + ".gif";

            if (!std::filesystem::exists(cacheFile)) {
                Logger::warn("GIF cache file not found: " + cacheFile);
                m_circularAvatar = Images::makeCircular(image, AVATAR_SIZE);
                redraw();
                return;
            }

            m_avatarGif = std::make_unique<GifAnimation>(cacheFile);
            if (!m_avatarGif->isValid()) {
                Logger::warn("Failed to load GIF animation from: " + cacheFile);
                m_circularAvatar = Images::makeCircular(image, AVATAR_SIZE);
                m_avatarGif.reset();
                redraw();
                return;
            }

            size_t frameCount = m_avatarGif->frameCount();
            m_circularFrames.reserve(frameCount);

            for (size_t i = 0; i < frameCount; i++) {
                Fl_RGB_Image *frame = m_avatarGif->getFrame(i);
                if (frame) {
                    Fl_RGB_Image *circularFrame = Images::makeCircular(frame, AVATAR_SIZE);
                    if (circularFrame) {
                        m_circularFrames.push_back(std::unique_ptr<Fl_RGB_Image>(circularFrame));
                    }
                }
            }

            if (!m_circularFrames.empty() && m_avatarGif->isAnimated()) {
                m_avatarGif->setFrame(0);
                m_avatarFrameTimeAccumulated = 0.0;
                m_avatarAnimationId =
                    AnimationManager::get().registerAnimation([this]() { return updateAvatarAnimation(); });
            } else {
                m_circularAvatar = Images::makeCircular(image, AVATAR_SIZE);
                m_avatarGif.reset();
            }

            redraw();
        });
    } else {
        m_avatarImage = Images::getCachedImage(m_avatarUrl);

        if (m_avatarImage) {
            m_circularAvatar = Images::makeCircular(m_avatarImage, AVATAR_SIZE);
            redraw();
        } else {
            Images::loadImageAsync(m_avatarUrl, [this](Fl_RGB_Image *image) {
                m_avatarImage = image;
                if (m_avatarImage) {
                    m_circularAvatar = Images::makeCircular(m_avatarImage, AVATAR_SIZE);
                }
                redraw();
            });
        }
    }
}

void ProfileBubble::setUser(const std::string &userId, const std::string &username, const std::string &avatarUrl,
                            const std::string &discriminator) {
    m_userId = userId;
    m_username = username;
    if (m_avatarUrl != avatarUrl) {
        m_avatarUrl = avatarUrl;
        loadAvatar();
    }

    m_discriminator = discriminator;
    redraw();
}

void ProfileBubble::setStatus(const std::string &status) {
    m_status = status;
    redraw();
}

void ProfileBubble::setCustomStatus(const std::string &customStatus, const std::string &emojiUrl) {
    m_customStatus = customStatus;

    if (m_customStatusEmojiUrl != emojiUrl) {
        m_customStatusEmojiUrl = emojiUrl;

        if (m_emojiAnimationId != 0) {
            AnimationManager::get().unregisterAnimation(m_emojiAnimationId);
            m_emojiAnimationId = 0;
        }
        if (m_customStatusEmoji) {
            delete m_customStatusEmoji;
            m_customStatusEmoji = nullptr;
        }
        m_emojiFrames.clear();
        m_emojiGif.reset();
        m_emojiFrameTimeAccumulated = 0.0;

        if (!emojiUrl.empty()) {
            loadCustomStatusEmoji();
        }
    }

    redraw();
}

void ProfileBubble::loadCustomStatusEmoji() {
    if (m_customStatusEmojiUrl.empty()) {
        return;
    }

    std::string urlLower = m_customStatusEmojiUrl;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    bool isGif = (urlLower.find(".gif") != std::string::npos);

    if (isGif) {
        Images::loadImageAsync(m_customStatusEmojiUrl, [this](Fl_RGB_Image *image) {
            if (!image) {
                Logger::warn("Failed to load GIF emoji");
                redraw();
                return;
            }

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

            std::hash<std::string> hasher;
            size_t hash = hasher(m_customStatusEmojiUrl);
            std::ostringstream oss;
            oss << std::hex << hash;
            std::string cacheFile = cacheDir + "/" + oss.str() + ".gif";

            if (!std::filesystem::exists(cacheFile)) {
                Logger::warn("GIF cache file not found: " + cacheFile);
                Fl_Image *scaledImg = image->copy(CUSTOM_STATUS_EMOJI_SIZE, CUSTOM_STATUS_EMOJI_SIZE);
                if (scaledImg) {
                    m_customStatusEmoji = static_cast<Fl_RGB_Image *>(scaledImg);
                }
                redraw();
                return;
            }

            m_emojiGif = std::make_unique<GifAnimation>(cacheFile);
            if (!m_emojiGif->isValid()) {
                Logger::warn("Failed to load GIF animation from: " + cacheFile);
                Fl_Image *scaledImg = image->copy(CUSTOM_STATUS_EMOJI_SIZE, CUSTOM_STATUS_EMOJI_SIZE);
                if (scaledImg) {
                    m_customStatusEmoji = static_cast<Fl_RGB_Image *>(scaledImg);
                }
                m_emojiGif.reset();
                redraw();
                return;
            }

            size_t frameCount = m_emojiGif->frameCount();
            m_emojiFrames.reserve(frameCount);

            for (size_t i = 0; i < frameCount; i++) {
                Fl_RGB_Image *frame = m_emojiGif->getFrame(i);
                if (frame) {
                    Fl_Image *scaledFrame = frame->copy(CUSTOM_STATUS_EMOJI_SIZE, CUSTOM_STATUS_EMOJI_SIZE);
                    if (scaledFrame) {
                        m_emojiFrames.push_back(
                            std::unique_ptr<Fl_RGB_Image>(static_cast<Fl_RGB_Image *>(scaledFrame)));
                    }
                }
            }

            if (!m_emojiFrames.empty() && m_emojiGif->isAnimated()) {
                m_emojiGif->setFrame(0);
                m_emojiFrameTimeAccumulated = 0.0;
                m_emojiAnimationId =
                    AnimationManager::get().registerAnimation([this]() { return updateEmojiAnimation(); });
            } else {
                Fl_Image *scaledImg = image->copy(CUSTOM_STATUS_EMOJI_SIZE, CUSTOM_STATUS_EMOJI_SIZE);
                if (scaledImg) {
                    m_customStatusEmoji = static_cast<Fl_RGB_Image *>(scaledImg);
                }
                m_emojiGif.reset();
            }

            redraw();
        });
    } else {
        Images::loadImageAsync(m_customStatusEmojiUrl, [this](Fl_RGB_Image *image) {
            if (image && image->w() > 0 && image->h() > 0) {
                Fl_Image *scaledImg = image->copy(CUSTOM_STATUS_EMOJI_SIZE, CUSTOM_STATUS_EMOJI_SIZE);
                if (scaledImg) {
                    m_customStatusEmoji = static_cast<Fl_RGB_Image *>(scaledImg);
                }
                redraw();
            }
        });
    }
}

int ProfileBubble::getButtonAt(int mx, int my) const {
    int bubbleX = x() + BUBBLE_MARGIN;
    int bubbleY = y() + BUBBLE_MARGIN;
    int bubbleW = w() - (BUBBLE_MARGIN * 2);
    int bubbleH = h() - (BUBBLE_MARGIN * 2);

    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - BUTTON_RIGHT_PADDING;

    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 2;
    }

    buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;
    if (mx >= buttonX && mx < buttonX + (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) && my >= buttonY &&
        my < buttonY + BUTTON_SIZE) {
        return 1;
    }

    buttonX -= (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) + BUTTON_GAP;
    if (mx >= buttonX && mx < buttonX + (ICON_SECTION_WIDTH + CHEVRON_SECTION_WIDTH) && my >= buttonY &&
        my < buttonY + BUTTON_SIZE) {
        return 0;
    }

    return -1;
}

bool ProfileBubble::isChevronHovered(int btnX, int btnY, bool isToggle, int mx, int my) const {
    if (!isToggle)
        return false;

    int chevronX = btnX + ICON_SECTION_WIDTH;
    return mx >= chevronX && mx < chevronX + CHEVRON_SECTION_WIDTH && my >= btnY && my < btnY + BUTTON_SIZE;
}

bool ProfileBubble::updateAvatarAnimation() {
    if (!m_avatarGif || !m_avatarGif->isAnimated() || m_circularFrames.empty()) {
        m_avatarAnimationId = 0;
        return false;
    }

    if (window() && !window()->shown()) {
        return true;
    }

    m_avatarFrameTimeAccumulated += AnimationManager::get().getFrameTime();
    double requiredDelay = m_avatarGif->currentDelay() / 1000.0;

    if (m_avatarFrameTimeAccumulated >= requiredDelay) {
        m_avatarGif->nextFrame();
        m_avatarFrameTimeAccumulated = 0.0;
        if (visible_r()) {
            redraw();
        }
    }

    return true;
}

bool ProfileBubble::updateEmojiAnimation() {
    if (!m_emojiGif || !m_emojiGif->isAnimated() || m_emojiFrames.empty()) {
        m_emojiAnimationId = 0;
        return false;
    }

    if (window() && !window()->shown()) {
        return true;
    }

    m_emojiFrameTimeAccumulated += AnimationManager::get().getFrameTime();
    double requiredDelay = m_emojiGif->currentDelay() / 1000.0;

    if (m_emojiFrameTimeAccumulated >= requiredDelay) {
        m_emojiGif->nextFrame();
        m_emojiFrameTimeAccumulated = 0.0;
        if (visible_r()) {
            redraw();
        }
    }

    return true;
}
