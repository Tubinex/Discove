#include "ui/components/ProfileBubble.h"

#include "ui/GifAnimation.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Images.h"
#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <filesystem>
#include <sstream>

ProfileBubble::ProfileBubble(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {
    box(FL_NO_BOX);
}

ProfileBubble::~ProfileBubble() {
    Fl::remove_timeout(animationTimerCallback, this);

    if (m_circularAvatar) {
        delete m_circularAvatar;
        m_circularAvatar = nullptr;
    }
}

void ProfileBubble::draw() {
    const int MARGIN = 8;
    const int BORDER_RADIUS = 8;

    int bubbleX = x() + MARGIN;
    int bubbleY = y() + MARGIN;
    int bubbleW = w() - (MARGIN * 2);
    int bubbleH = h() - (MARGIN * 2);

    fl_color(ThemeColors::BG_SECONDARY_ALT);

    fl_pie(bubbleX, bubbleY, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 90, 180);
    fl_pie(bubbleX + bubbleW - BORDER_RADIUS * 2, bubbleY, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 0, 90);
    fl_pie(bubbleX, bubbleY + bubbleH - BORDER_RADIUS * 2, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 180, 270);
    fl_pie(bubbleX + bubbleW - BORDER_RADIUS * 2, bubbleY + bubbleH - BORDER_RADIUS * 2, BORDER_RADIUS * 2,
           BORDER_RADIUS * 2, 270, 360);

    fl_rectf(bubbleX + BORDER_RADIUS, bubbleY, bubbleW - BORDER_RADIUS * 2, bubbleH);
    fl_rectf(bubbleX, bubbleY + BORDER_RADIUS, bubbleW, bubbleH - BORDER_RADIUS * 2);

    int avatarX = bubbleX + (bubbleH - AVATAR_SIZE) / 2;
    int avatarY = bubbleY + (bubbleH - AVATAR_SIZE) / 2;
    drawAvatar(avatarX, avatarY);

    int dotX = avatarX + AVATAR_SIZE - STATUS_DOT_SIZE;
    int dotY = avatarY + AVATAR_SIZE - STATUS_DOT_SIZE;
    drawStatusDot(dotX, dotY);

    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 13);

    int textX = avatarX + AVATAR_SIZE + 8;
    int textY = bubbleY + bubbleH / 2 - 8;

    fl_draw(m_username.c_str(), textX, textY);
    if (m_discriminator != "0") {
        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 12);
        fl_draw(("#" + m_discriminator).c_str(), textX, textY + 16);
    }

    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - 4;

    drawButton(buttonX, buttonY, "⚙", m_hoveredButton == 2, false);
    buttonX -= BUTTON_SIZE + 4;

    drawButton(buttonX, buttonY, "🎧", m_hoveredButton == 1, m_headphonesDeafened);
    buttonX -= BUTTON_SIZE + 4;

    drawButton(buttonX, buttonY, "🎤", m_hoveredButton == 0, m_microphoneMuted);
}

void ProfileBubble::drawAvatar(int avatarX, int avatarY) {
    fl_color(ThemeColors::BG_PRIMARY);
    fl_pie(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE, 0, 360);

    Fl_RGB_Image *frameToShow = nullptr;

    if (m_isAnimated && !m_circularFrames.empty() && m_currentFrame < m_circularFrames.size()) {
        frameToShow = m_circularFrames[m_currentFrame].get();
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

    fl_color(ThemeColors::BG_SECONDARY_ALT);
    int borderSize = STATUS_DOT_SIZE + (STATUS_DOT_BORDER_WIDTH * 2);
    fl_pie(dotX - STATUS_DOT_BORDER_WIDTH, dotY - STATUS_DOT_BORDER_WIDTH, borderSize, borderSize, 0, 360);

    fl_color(statusColor);
    fl_pie(dotX, dotY, STATUS_DOT_SIZE, STATUS_DOT_SIZE, 0, 360);
}

void ProfileBubble::drawButton(int btnX, int btnY, const char *icon, bool hovered, bool active) {
    if (hovered) {
        fl_color(ThemeColors::BG_MODIFIER_HOVER);
        fl_pie(btnX, btnY, BUTTON_SIZE, BUTTON_SIZE, 0, 360);
    }

    if (active) {
        fl_color(fl_rgb_color(240, 71, 71));
    } else {
        fl_color(ThemeColors::TEXT_MUTED);
    }

    fl_font(FL_HELVETICA, 18);

    int textW = static_cast<int>(fl_width(icon));
    int textH = fl_height();

    fl_draw(icon, btnX + (BUTTON_SIZE - textW) / 2, btnY + (BUTTON_SIZE + textH) / 2 - fl_descent());
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
        int newHovered = (event == FL_LEAVE) ? -1 : getButtonAt(Fl::event_x(), Fl::event_y());

        if (newHovered != m_hoveredButton) {
            m_hoveredButton = newHovered;
            redraw();
        }
        return 1;
    }
    }

    return Fl_Widget::handle(event);
}

void ProfileBubble::loadAvatar() {
    Fl::remove_timeout(animationTimerCallback, this);

    if (m_circularAvatar) {
        delete m_circularAvatar;
        m_circularAvatar = nullptr;
    }
    m_circularFrames.clear();
    m_frameDelays.clear();
    m_currentFrame = 0;
    m_isAnimated = false;

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
                m_isAnimated = false;
                redraw();
                return;
            }

            auto gif = std::make_unique<GifAnimation>(cacheFile);
            if (!gif->isValid()) {
                Logger::warn("Failed to load GIF animation from: " + cacheFile);
                m_circularAvatar = Images::makeCircular(image, AVATAR_SIZE);
                m_isAnimated = false;
                redraw();
                return;
            }

            size_t frameCount = gif->frameCount();
            m_circularFrames.reserve(frameCount);
            m_frameDelays.reserve(frameCount);

            for (size_t i = 0; i < frameCount; i++) {
                Fl_RGB_Image *frame = gif->getFrame(i);
                if (frame) {
                    Fl_RGB_Image *circularFrame = Images::makeCircular(frame, AVATAR_SIZE);
                    if (circularFrame) {
                        m_circularFrames.push_back(std::unique_ptr<Fl_RGB_Image>(circularFrame));
                        m_frameDelays.push_back(gif->currentDelay());
                    }
                }
                gif->nextFrame();
            }

            if (!m_circularFrames.empty()) {
                m_isAnimated = true;
                m_currentFrame = 0;
                Fl::add_timeout(m_frameDelays[0] / 1000.0, animationTimerCallback, this);
            } else {
                m_circularAvatar = Images::makeCircular(image, AVATAR_SIZE);
                m_isAnimated = false;
            }

            redraw();
        });
    } else {
        m_avatarImage = Images::getCachedImage(m_avatarUrl);

        if (m_avatarImage) {
            m_circularAvatar = Images::makeCircular(m_avatarImage, AVATAR_SIZE);
            m_isAnimated = false;
            redraw();
        } else {
            Images::loadImageAsync(m_avatarUrl, [this](Fl_RGB_Image *image) {
                m_avatarImage = image;
                if (m_avatarImage) {
                    m_circularAvatar = Images::makeCircular(m_avatarImage, AVATAR_SIZE);
                    m_isAnimated = false;
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

int ProfileBubble::getButtonAt(int mx, int my) const {
    const int MARGIN = 8;
    int bubbleX = x() + MARGIN;
    int bubbleY = y() + MARGIN;
    int bubbleW = w() - (MARGIN * 2);
    int bubbleH = h() - (MARGIN * 2);

    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - 4;

    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 2;
    }

    buttonX -= BUTTON_SIZE + 4;
    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 1;
    }

    buttonX -= BUTTON_SIZE + 4;
    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 0;
    }

    return -1;
}

void ProfileBubble::advanceFrame() {
    if (!m_isAnimated || m_circularFrames.empty()) {
        return;
    }

    m_currentFrame = (m_currentFrame + 1) % m_circularFrames.size();
    redraw();

    if (m_currentFrame < m_frameDelays.size()) {
        double delay = m_frameDelays[m_currentFrame] / 1000.0;
        Fl::add_timeout(delay, animationTimerCallback, this);
    }
}

void ProfileBubble::animationTimerCallback(void *userdata) {
    ProfileBubble *bubble = static_cast<ProfileBubble *>(userdata);
    if (bubble) {
        bubble->advanceFrame();
    }
}
