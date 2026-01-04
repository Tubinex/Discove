#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scroll.H>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "models/Message.h"

class Fl_RGB_Image;

class TextChannelView : public Fl_Group {
  public:
    TextChannelView(int x, int y, int w, int h, const char *label = nullptr);
    ~TextChannelView();

    void draw() override;
    int handle(int event) override;
    void resize(int x, int y, int w, int h) override;

    /**
     * @brief Set the channel being displayed
     * @param channelId Channel ID
     * @param channelName Channel name
     * @param guildId Guild ID
     * @param isWelcomeVisible Whether to show the welcome message
     */
    void setChannel(const std::string &channelId, const std::string &channelName, const std::string &guildId = "",
                    bool isWelcomeVisible = false);

    /**
     * @brief Get the current channel ID
     * @return Channel ID
     */
    std::string getChannelId() const { return m_channelId; }

    /**
     * @brief Set callback for sending messages
     * @param callback Callback function
     */
    void setOnSendMessage(std::function<void(const std::string &)> callback) { m_onSendMessage = callback; }

  private:
    void drawHeader();
    void drawWelcomeSection();
    void drawMessages();
    void drawMessageInput();
    void drawMessage(const Message &msg, int &yPos, const std::string &previousAuthorId,
                     const std::chrono::system_clock::time_point &previousTimestamp);
    void drawDateSeparator(const std::string &date, int &yPos);
    void loadMessagesFromStore();
    void updatePermissions();
    void ensureEmojiAtlases(int targetSize);
    bool getEmojiButtonRect(int &outX, int &outY, int &outSize) const;
    bool getInputButtonRect(int index, int &outX, int &outY, int &outSize) const;
    bool getPlusButtonRect(int &outX, int &outY, int &outSize) const;

    std::string m_channelId;
    std::string m_channelName;
    std::string m_guildId;
    bool m_welcomeVisible = false;
    bool m_canSendMessages = true;
    std::vector<Message> m_messages;
    Fl_Scroll *m_scrollArea = nullptr;
    Fl_Input *m_messageInput = nullptr;
    std::function<void(const std::string &)> m_onSendMessage;
    bool m_emojiAtlasesLoaded = false;
    size_t m_emojiFrameIndex = 0;
    int m_hoveredInputButton = -1;
    bool m_plusHovered = false;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_emojiFramesInactive;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_emojiFramesActive;

    static constexpr int HEADER_HEIGHT = 48;
    static constexpr int MESSAGE_INPUT_HEIGHT = 74;
    static constexpr int MESSAGE_SPACING = 4;
    static constexpr int MESSAGE_GROUP_SPACING = 17;
};
