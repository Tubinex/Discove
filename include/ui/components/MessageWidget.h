#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include <FL/Fl.H>

#include "models/Message.h"

class MessageWidget {
  public:
    struct InlineItem {
        enum class Kind { Text, LineBreak };

        Kind kind = Kind::Text;
        std::string text;
        int width = 0;
        Fl_Font font = 0;
        int size = 0;
        Fl_Color color = 0;
        std::string linkUrl;
        bool isLink = false;
        bool underline = false;
        bool strikethrough = false;
    };

    struct LayoutLine {
        std::vector<InlineItem> items;
        int width = 0;
    };

    struct ReplyPreview {
        std::string author;
        std::string content;
        bool unavailable = false;
    };

    struct AttachmentLayout {
        int width = 0;
        int height = 0;
        bool isImage = false;
        bool squareCrop = false;
        std::string cacheKey;
        int xOffset = 0;
        int yOffset = 0;
    };

    struct Layout {
        bool isSystem = false;
        bool grouped = false;
        int height = 0;
        int contentX = 0;
        int contentBaseline = 0;
        int contentTop = 0;
        int contentHeight = 0;
        int attachmentsTopPadding = 0;
        int contentWidth = 0;
        bool hasReply = false;
        int replyX = 0;
        int replyBaseline = 0;
        int replyHeight = 0;
        int replyLineX = 0;
        int replyLineTop = 0;
        int replyLineBottom = 0;
        int headerBaseline = 0;
        int avatarX = 0;
        int avatarY = 0;
        int avatarSize = 0;
        int usernameX = 0;
        int timeX = 0;
        int timeBaseline = 0;
        int lineHeight = 0;
        int lineSpacing = 0;
        int viewWidth = 0;
        int systemIconX = 0;
        int systemIconY = 0;
        int systemIconSize = 0;
        std::string username;
        std::string time;
        std::string systemIconName;
        Fl_Color systemIconColor = 0;
        std::vector<LayoutLine> lines;
        std::vector<InlineItem> replyItems;
        std::vector<AttachmentLayout> attachments;
    };

    static Layout buildLayout(const Message &msg, int viewWidth, bool isGrouped, bool compactBottom,
                              const ReplyPreview *replyPreview);
    static void draw(const Message &msg, const Layout &layout, int originX, int originY, bool avatarHovered);
    static std::string getAvatarCacheKey(const Message &msg, int size);
    static std::string getAnimatedAvatarKey(const Message &msg, int size);
    static void setHoveredAvatarKey(const std::string &key);
    static void pruneAvatarCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAnimatedAvatarCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAttachmentCache(const std::unordered_set<std::string> &keepKeys);

  private:
    static std::vector<InlineItem> tokenizeText(const std::string &text, Fl_Font font, int size, Fl_Color color);
    static std::vector<LayoutLine> wrapText(const std::string &text, int maxWidth);
    static std::vector<LayoutLine> wrapTokens(const std::vector<InlineItem> &tokens, int maxWidth);
    static std::vector<InlineItem> splitLongToken(const InlineItem &token, int maxWidth);
};
