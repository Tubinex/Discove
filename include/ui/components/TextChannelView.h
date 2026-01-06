#pragma once

#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Scroll.H>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/Message.h"
#include "ui/VirtualScroll.h"
#include "ui/components/MessageWidget.h"

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
    void setOnSendMessage(std::function<void(const std::string &, const std::string &)> callback) {
        m_onSendMessage = callback;
    }

  private:
    void drawHeader();
    void drawWelcomeSection();
    void drawMessages();
    void drawMessageInput();
    void drawDateSeparator(const std::string &date, int &yPos);
    void loadMessagesFromStore();
    void loadMessages();
    void updatePermissions();
    void ensureEmojiAtlases(int targetSize);
    bool updateAvatarHover(int mx, int my, bool forceClear);
    bool updateAttachmentDownloadHover(int mx, int my, bool forceClear);
    bool getEmojiButtonRect(int &outX, int &outY, int &outSize) const;
    bool getInputButtonRect(int index, int &outX, int &outY, int &outSize) const;
    bool getPlusButtonRect(int &outX, int &outY, int &outSize) const;
    void enqueuePendingMessage(const std::string &content, const std::string &nonce);

    struct AvatarHitbox {
        int x = 0;
        int y = 0;
        int size = 0;
        std::string messageId;
        std::string hoverKey;
    };

    struct AttachmentDownloadHitbox {
        int x = 0;
        int y = 0;
        int size = 0;
        std::string key;
    };

    struct LayoutCacheEntry {
        MessageWidget::Layout layout;
        int width = 0;
        bool grouped = false;
        bool compactBottom = false;
    };

    int estimateMessageHeight(const Message &msg, bool isGrouped) const;
    int estimatedLineCount(const Message &msg) const;

    std::string m_channelId;
    std::string m_channelName;
    std::string m_guildId;
    bool m_welcomeVisible = false;
    bool m_canSendMessages = true;
    std::vector<Message> m_messages;
    std::vector<Message> m_pendingMessages;
    Fl_Scroll *m_scrollArea = nullptr;
    Fl_Input *m_messageInput = nullptr;
    int m_messagesScrollOffset = 0;
    int m_messagesContentHeight = 0;
    int m_messagesViewHeight = 0;
    std::function<void(const std::string &, const std::string &)> m_onSendMessage;
    bool m_inputFocused = false;
    bool m_emojiAtlasesLoaded = false;
    size_t m_emojiFrameIndex = 0;
    int m_hoveredInputButton = -1;
    bool m_plusHovered = false;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_emojiFramesInactive;
    std::vector<std::unique_ptr<Fl_RGB_Image>> m_emojiFramesActive;
    std::vector<AvatarHitbox> m_avatarHitboxes;
    std::vector<AttachmentDownloadHitbox> m_attachmentDownloadHitboxes;
    std::string m_hoveredAvatarMessageId;
    std::string m_hoveredAvatarKey;
    std::string m_hoveredAttachmentDownloadKey;
    uint64_t m_storeListenerId = 0;
    bool m_isDestroying = false;

    std::unordered_map<std::string, LayoutCacheEntry> m_layoutCache;
    std::unordered_map<std::string, int> m_heightEstimateCache;

    std::vector<int> m_itemYPositions;
    std::vector<int> m_separatorYPositions;
    std::vector<std::string> m_previousMessageIds;
    std::vector<int> m_previousItemYPositions;
    std::vector<int> m_previousItemHeights;
    int m_previousTotalHeight = 0;

    bool m_shouldScrollToBottom = false;
    bool m_messagesChanged = false;

    static constexpr int HEADER_HEIGHT = 48;
    static constexpr int MESSAGE_INPUT_HEIGHT = 74;
    static constexpr int MESSAGE_SPACING = 4;
    static constexpr int MESSAGE_GROUP_SPACING = 18;
};
