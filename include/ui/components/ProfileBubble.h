#pragma once

#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Widget.H>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/AnimationManager.h"

class GifAnimation;

class ProfileBubble : public Fl_Widget {
  public:
    ProfileBubble(int x, int y, int w, int h, const char *label = nullptr);
    ~ProfileBubble();

    void draw() override;
    int handle(int event) override;

    /**
     * @brief Set user information
     * @param userId User ID
     * @param username Username
     * @param avatarUrl Avatar URL (can be empty for default)
     * @param discriminator Discriminator (e.g., "1234" or "0")
     */
    void setUser(const std::string &userId, const std::string &username, const std::string &avatarUrl,
                 const std::string &discriminator);

    /**
     * @brief Set user status
     * @param status Status (online, dnd, idle, invisible, offline)
     */
    void setStatus(const std::string &status);

    /**
     * @brief Set custom status text and emoji
     * @param customStatus Custom status text (can be empty)
     * @param emojiUrl Custom status emoji URL (can be empty)
     */
    void setCustomStatus(const std::string &customStatus, const std::string &emojiUrl = "");

    /**
     * @brief Set callback for settings button click
     * @param cb Callback function
     */
    void setOnSettingsClicked(std::function<void()> cb) { m_onSettingsClicked = cb; }

    /**
     * @brief Set callback for microphone button click
     * @param cb Callback function
     */
    void setOnMicrophoneClicked(std::function<void()> cb) { m_onMicrophoneClicked = cb; }

    /**
     * @brief Set callback for headphones button click
     * @param cb Callback function
     */
    void setOnHeadphonesClicked(std::function<void()> cb) { m_onHeadphonesClicked = cb; }

  private:
    std::string m_userId;
    std::string m_username;
    std::string m_avatarUrl;
    std::string m_discriminator;
    std::string m_status = "online";
    std::string m_customStatus;
    std::string m_customStatusEmojiUrl;

    bool m_microphoneMuted = false;
    bool m_headphonesDeafened = false;

    int m_hoveredButton = -1;
    bool m_hoveredButtonChevron = false;

    Fl_RGB_Image *m_avatarImage = nullptr;
    Fl_RGB_Image *m_circularAvatar = nullptr;
    Fl_RGB_Image *m_customStatusEmoji = nullptr;

    std::unique_ptr<GifAnimation> m_avatarGif;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_circularFrames;
    double m_avatarFrameTimeAccumulated = 0.0;
    AnimationManager::AnimationId m_avatarAnimationId = 0;

    std::unique_ptr<GifAnimation> m_emojiGif;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_emojiFrames;
    double m_emojiFrameTimeAccumulated = 0.0;
    AnimationManager::AnimationId m_emojiAnimationId = 0;

    std::function<void()> m_onSettingsClicked;
    std::function<void()> m_onMicrophoneClicked;
    std::function<void()> m_onHeadphonesClicked;

    static constexpr int AVATAR_SIZE = 34;
    static constexpr int STATUS_DOT_SIZE = 10;
    static constexpr int STATUS_DOT_BORDER_WIDTH = 3;

    static constexpr int BUTTON_SIZE = 32;
    static constexpr int ICON_SIZE = 20;
    static constexpr int ARROW_SIZE = 14;
    static constexpr int ICON_SECTION_WIDTH = BUTTON_SIZE;
    static constexpr int CHEVRON_SECTION_WIDTH = 20;
    static constexpr int BUTTON_GAP = 8;

    static constexpr int BUBBLE_MARGIN = 8;
    static constexpr int BUBBLE_BORDER_RADIUS = 8;
    static constexpr int BUTTON_HOVER_GAP = 1;
    static constexpr int BUTTON_RIGHT_PADDING = 12;
    static constexpr int CUSTOM_STATUS_EMOJI_SIZE = 16;

    void drawAvatar(int avatarX, int avatarY);
    void drawStatusDot(int dotX, int dotY);
    void drawButton(int btnX, int btnY, const char *iconName, bool isToggle, bool hovered, bool chevronHovered,
                    bool active);
    int getButtonAt(int mx, int my) const;
    bool isChevronHovered(int btnX, int btnY, bool isToggle, int mx, int my) const;
    void loadAvatar();
    void loadCustomStatusEmoji();
    bool updateAvatarAnimation();
    bool updateEmojiAnimation();
};
