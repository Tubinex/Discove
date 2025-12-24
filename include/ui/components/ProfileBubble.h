#pragma once

#include <FL/Fl_Widget.H>
#include <functional>
#include <string>

class ProfileBubble : public Fl_Widget {
  public:
    ProfileBubble(int x, int y, int w, int h, const char *label = nullptr);

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

    bool m_microphoneMuted = false;
    bool m_headphonesDeafened = false;

    int m_hoveredButton = -1;

    std::function<void()> m_onSettingsClicked;
    std::function<void()> m_onMicrophoneClicked;
    std::function<void()> m_onHeadphonesClicked;

    static constexpr int AVATAR_SIZE = 32;
    static constexpr int STATUS_DOT_SIZE = 10;
    static constexpr int BUTTON_SIZE = 32;

    void drawAvatar(int avatarX, int avatarY);
    void drawStatusDot(int dotX, int dotY);
    void drawButton(int btnX, int btnY, const char *icon, bool hovered, bool active);
    int getButtonAt(int mx, int my) const;
};
