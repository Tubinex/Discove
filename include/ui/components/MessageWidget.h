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
        enum class Kind { Text, LineBreak, Emoji };

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
        bool preserveWhitespace = false;
        bool isCodeBlock = false;
        std::string emojiUrl;
        std::string emojiCacheKey;
        int emojiSize = 0;
    };

    struct LayoutLine {
        std::vector<InlineItem> items;
        int width = 0;
        bool isCodeBlock = false;
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
        int downloadXOffset = 0;
        int downloadYOffset = 0;
        int downloadSize = 0;
    };

    struct StickerLayout {
        int width = 0;
        int height = 0;
        int xOffset = 0;
        int yOffset = 0;
        bool supported = false;
        int formatType = 0;
        std::string name;
        std::string url;
        std::string cacheKey;
    };

    struct ReactionLayout {
        int width = 0;
        int height = 0;
        int xOffset = 0;
        int yOffset = 0;
        bool isCustomEmoji = false;
        bool animated = false;
        bool me = false;
        int emojiSize = 0;
        int emojiWidth = 0;
        std::string emojiName;
        std::string emojiUrl;
        std::string emojiCacheKey;
        std::string countText;
    };

    struct Layout {
        bool isSystem = false;
        bool grouped = false;
        int height = 0;
        int contentX = 0;
        int contentBaseline = 0;
        int contentTop = 0;
        int contentHeight = 0;
        int stickersHeight = 0;
        int stickersTopPadding = 0;
        int attachmentsHeight = 0;
        int attachmentsTopPadding = 0;
        int reactionsTopPadding = 0;
        int reactionsHeight = 0;
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
        std::vector<StickerLayout> stickers;
        std::vector<AttachmentLayout> attachments;
        std::vector<ReactionLayout> reactions;
    };

    static Layout buildLayout(const Message &msg, int viewWidth, bool isGrouped, bool compactBottom,
                              const ReplyPreview *replyPreview);
    static void draw(const Message &msg, const Layout &layout, int originX, int originY, bool avatarHovered);
    static std::string getAvatarCacheKey(const Message &msg, int size);
    static std::string getAnimatedAvatarKey(const Message &msg, int size);
    static std::string getAttachmentDownloadKey(const Message &msg, size_t attachmentIndex);
    static void setHoveredAvatarKey(const std::string &key);
    static void setHoveredAttachmentDownloadKey(const std::string &key);
    static void pruneAvatarCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAnimatedAvatarCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAnimatedEmojiCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneStickerCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAnimatedStickerCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneAttachmentCache(const std::unordered_set<std::string> &keepKeys);
    static void pruneEmojiCache(const std::unordered_set<std::string> &keepKeys);

  private:
    static std::vector<InlineItem> tokenizeText(const std::string &text, Fl_Font font, int size, Fl_Color color);
    static std::vector<LayoutLine> wrapText(const std::string &text, int maxWidth);
    static std::vector<LayoutLine> wrapTokens(const std::vector<InlineItem> &tokens, int maxWidth, int codeBlockMaxWidth);
    static std::vector<InlineItem> splitLongToken(const InlineItem &token, int maxWidth);
};
