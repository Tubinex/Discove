#include "ui/components/MessageWidget.h"

#include "ui/AnimationManager.h"
#include "ui/GifAnimation.h"
#include "ui/IconManager.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Images.h"

#include <FL/fl_draw.H>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr int kLeftMargin = 16;
constexpr int kRightMargin = 16;
constexpr int kAvatarSize = 40;
constexpr int kAvatarMargin = 16;
constexpr int kUsernameFontSize = 16;
constexpr int kTimestampFontSize = 12;
constexpr int kEditedFontSize = 12;
constexpr int kContentFontSize = 16;
constexpr int kHeaderTopPadding = -4;
constexpr int kTimestampBaselineAdjust = 5;
constexpr int kHeaderToContentPadding = 6;
constexpr int kContentBottomPadding = 8;
constexpr int kGroupedTopPadding = 3;
constexpr int kGroupedBottomPadding = 3;
constexpr int kLineSpacing = 4;
constexpr int kSystemFontSize = 14;
constexpr int kSystemLineSpacing = 2;
constexpr int kSystemTopPadding = 6;
constexpr int kSystemBottomPadding = 6;
constexpr int kSystemIconSize = 16;
constexpr int kSystemIconGap = 8;
constexpr int kSystemTimeGap = 10;
constexpr int kTimestampGap = 8;
constexpr Fl_Color kBoostColor = 0xFF73FAFF;
constexpr int kAttachmentTopPadding = 8;
constexpr int kAttachmentTopPaddingNoContent = 6;
constexpr int kAttachmentSpacing = 8;
constexpr int kAttachmentMaxWidth = 520;
constexpr int kAttachmentMaxHeight = 300;
constexpr int kAttachmentMinSize = 20;
constexpr int kAttachmentFileHeight = 76;
constexpr int kAttachmentCornerRadius = 8;
constexpr int kAttachmentIconSize = 40;
constexpr int kAttachmentFileMaxWidth = 460;
constexpr int kAttachmentDownloadButtonSize = 36;
constexpr int kAttachmentDownloadIconSize = 22;
constexpr int kAttachmentDownloadButtonRadius = 6;
constexpr int kAttachmentDownloadPadding = 12;
constexpr int kAttachmentNameFontSize = 16;
constexpr int kAttachmentMetaFontSize = 11;
constexpr int kReplyFontSize = 12;
constexpr int kReplyTopPadding = 2;
constexpr int kReplyToHeaderPadding = 4;
constexpr int kReplyToContentPadding = 4;
constexpr int kReplyLineTextGap = 6;
constexpr int kReplyLineBaselineOffset = 2;
constexpr int kReplyLineCornerRadius = 4;
constexpr int kReplyLineCornerSegments = 6;
constexpr double kReplyHalfPi = 1.5707963267948966;
constexpr int kReplyLineYOffset = -1;
constexpr int kReplyLineThickness = 2;
constexpr int kEmojiSize = 22;
constexpr int kEmojiOnlySize = 48;
constexpr int kEmojiRequestSize = 48;
constexpr int kEmojiBaselineOffset = 3;
constexpr int kReactionHeight = 28;
constexpr int kReactionCornerRadius = 6;
constexpr int kReactionEmojiSize = 16;
constexpr int kReactionEmojiFontSize = 14;
constexpr int kReactionFontSize = 12;
constexpr int kReactionPaddingX = 8;
constexpr int kReactionSpacing = 6;
constexpr int kReactionRowSpacing = 6;
constexpr int kReactionEmojiGap = 6;
constexpr int kReactionTopPadding = 10;
constexpr int kReactionTopPaddingNoContent = 8;
constexpr int kReactionBottomPadding = 6;
constexpr Fl_Color kReactionBg = 0x242428FF;
constexpr Fl_Color kReactionTextColor = ThemeColors::TEXT_NORMAL;
constexpr Fl_Color kReactionSelfBg = 0x292b50FF;
constexpr Fl_Color kReactionSelfBorder = 0x5865f2FF;
} // namespace

namespace {
std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> avatar_cache;
std::unordered_set<std::string> avatar_pending;

struct AnimatedAvatarState {
    std::unique_ptr<GifAnimation> animation;
    std::vector<std::unique_ptr<Fl_RGB_Image>> frames;
    AnimationManager::AnimationId animationId = 0;
    double frameTimeAccumulated = 0.0;
    bool running = false;
};

std::unordered_map<std::string, AnimatedAvatarState> avatar_gif_cache;
std::unordered_set<std::string> avatar_gif_pending;
std::string hovered_avatar_key;
std::string hovered_attachment_download_key;

std::string buildAvatarCacheKey(const std::string &url, int size) { return url + "#" + std::to_string(size); }
std::string buildEmojiCacheKey(const std::string &url, int size) { return url + "#" + std::to_string(size); }

bool isGifUrl(const std::string &url) {
    size_t pos = url.rfind(".gif");
    if (pos == std::string::npos) {
        return false;
    }
    size_t end = pos + 4;
    return end == url.size() || url[end] == '?' || url[end] == '#';
}

std::string makeStaticAvatarUrl(const std::string &url) {
    if (!isGifUrl(url)) {
        return url;
    }

    std::string staticUrl = url;
    size_t pos = staticUrl.rfind(".gif");
    if (pos != std::string::npos) {
        staticUrl.replace(pos, 4, ".png");
    }
    return staticUrl;
}

std::string getStaticAvatarUrl(const Message &msg, int size) {
    std::string url = msg.getAuthorAvatarUrl(size);
    if (url.empty()) {
        return "";
    }
    return makeStaticAvatarUrl(url);
}

std::string getAnimatedAvatarUrl(const Message &msg, int size) {
    std::string url = msg.getAuthorAvatarUrl(size);
    if (url.empty() || !isGifUrl(url)) {
        return "";
    }
    return url;
}

bool updateAvatarAnimation(const std::string &key) {
    auto it = avatar_gif_cache.find(key);
    if (it == avatar_gif_cache.end()) {
        return false;
    }

    auto &state = it->second;
    if (!state.running || !state.animation || state.frames.empty()) {
        state.running = false;
        state.animationId = 0;
        return false;
    }

    if (key != hovered_avatar_key) {
        state.running = false;
        state.animationId = 0;
        state.frameTimeAccumulated = 0.0;
        state.animation->setFrame(0);
        return false;
    }

    state.frameTimeAccumulated += AnimationManager::get().getFrameTime();
    double requiredDelay = state.animation->currentDelay() / 1000.0;
    if (state.frameTimeAccumulated >= requiredDelay) {
        state.animation->nextFrame();
        state.frameTimeAccumulated = 0.0;
        Fl::redraw();
    }

    return true;
}

void startAvatarAnimation(const std::string &key) {
    auto it = avatar_gif_cache.find(key);
    if (it == avatar_gif_cache.end()) {
        return;
    }

    auto &state = it->second;
    if (state.running || !state.animation || !state.animation->isAnimated() || state.frames.size() <= 1) {
        return;
    }

    state.running = true;
    state.animationId = AnimationManager::get().registerAnimation([key]() { return updateAvatarAnimation(key); });
}

void stopAvatarAnimation(const std::string &key) {
    auto it = avatar_gif_cache.find(key);
    if (it == avatar_gif_cache.end()) {
        return;
    }

    auto &state = it->second;
    if (!state.running) {
        return;
    }

    if (state.animationId != 0) {
        AnimationManager::get().unregisterAnimation(state.animationId);
        state.animationId = 0;
    }

    state.running = false;
    state.frameTimeAccumulated = 0.0;
    if (state.animation) {
        state.animation->setFrame(0);
    }
}

bool ensureAnimatedAvatar(const std::string &gifUrl, int size) {
    if (gifUrl.empty()) {
        return false;
    }

    std::string cacheKey = buildAvatarCacheKey(gifUrl, size);
    auto existing = avatar_gif_cache.find(cacheKey);
    if (existing != avatar_gif_cache.end()) {
        return !existing->second.frames.empty();
    }

    if (avatar_gif_pending.find(cacheKey) != avatar_gif_pending.end()) {
        return false;
    }

    avatar_gif_pending.insert(cacheKey);

    Images::loadImageAsync(gifUrl, [gifUrl, size, cacheKey](Fl_RGB_Image *image) {
        auto clearPending = [&]() { avatar_gif_pending.erase(cacheKey); };

        if (!image) {
            clearPending();
            return;
        }

        std::string gifPath = Images::getCacheFilePath(gifUrl, "gif");
        if (!std::filesystem::exists(gifPath)) {
            clearPending();
            return;
        }

        auto animation = std::make_unique<GifAnimation>(gifPath, GifAnimation::ScalingStrategy::Lazy);
        if (!animation || !animation->isValid()) {
            clearPending();
            return;
        }

        AnimatedAvatarState state;
        state.animation = std::move(animation);

        size_t frameCount = state.animation->frameCount();
        state.frames.reserve(frameCount);

        for (size_t i = 0; i < frameCount; ++i) {
            Fl_RGB_Image *frame = state.animation->getFrame(i);
            if (!frame) {
                state.frames.emplace_back(nullptr);
                continue;
            }
            Fl_RGB_Image *circularFrame = Images::makeCircular(frame, size);
            state.frames.emplace_back(circularFrame);
        }

        bool hasFrame = std::any_of(state.frames.begin(), state.frames.end(),
                                    [](const std::unique_ptr<Fl_RGB_Image> &frame) { return frame != nullptr; });
        if (!hasFrame) {
            clearPending();
            return;
        }

        state.animation->setFrame(0);
        avatar_gif_cache[cacheKey] = std::move(state);
        clearPending();

        if (hovered_avatar_key == cacheKey) {
            startAvatarAnimation(cacheKey);
        }

        Fl::redraw();
    });

    return false;
}

Fl_RGB_Image *getAvatarImage(const Message &msg, int size) {
    std::string url = getStaticAvatarUrl(msg, size);
    if (url.empty()) {
        return nullptr;
    }

    std::string cacheKey = buildAvatarCacheKey(url, size);
    auto it = avatar_cache.find(cacheKey);
    if (it != avatar_cache.end()) {
        return it->second.get();
    }

    if (avatar_pending.find(cacheKey) == avatar_pending.end()) {
        avatar_pending.insert(cacheKey);
        Images::loadImageAsync(url, [cacheKey, size](Fl_RGB_Image *image) {
            if (image && image->w() > 0 && image->h() > 0) {
                Fl_RGB_Image *circular = Images::makeCircular(image, size);
                if (circular) {
                    avatar_cache[cacheKey] = std::unique_ptr<Fl_RGB_Image>(circular);
                }
            }
            avatar_pending.erase(cacheKey);
            Fl::redraw();
        });
    }

    return nullptr;
}

std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> attachment_cache;
std::unordered_set<std::string> attachment_pending;
std::unordered_map<std::string, std::unique_ptr<Fl_RGB_Image>> emoji_cache;
std::unordered_set<std::string> emoji_pending;

std::string getAttachmentUrl(const Attachment &attachment) {
    if (attachment.contentType.has_value() && attachment.isImage()) {
        if (!attachment.url.empty()) {
            return attachment.url;
        }
    }
    if (!attachment.proxyUrl.empty()) {
        return attachment.proxyUrl;
    }
    return attachment.url;
}

std::string buildAttachmentCacheKey(const std::string &url, int width, int height, bool squareCrop) {
    return url + "#" + std::to_string(width) + "x" + std::to_string(height) + (squareCrop ? "#square" : "#fit");
}

Fl_RGB_Image *cropAndScaleToSquare(Fl_RGB_Image *source, int size) {
    if (!source || size <= 0 || source->w() <= 0 || source->h() <= 0) {
        return nullptr;
    }

    int srcW = source->w();
    int srcH = source->h();
    int depth = source->d();
    if (depth <= 0) {
        return nullptr;
    }

    const char *const *dataArray = source->data();
    if (!dataArray || !dataArray[0]) {
        return nullptr;
    }
    const unsigned char *srcData = reinterpret_cast<const unsigned char *>(dataArray[0]);
    int srcLineSize = source->ld() ? source->ld() : srcW * depth;

    int cropSize = std::min(srcW, srcH);
    int cropX = (srcW - cropSize) / 2;
    int cropY = (srcH - cropSize) / 2;

    std::vector<unsigned char> buffer(cropSize * cropSize * depth);
    for (int y = 0; y < cropSize; ++y) {
        const unsigned char *srcRow = srcData + (cropY + y) * srcLineSize + cropX * depth;
        unsigned char *dstRow = buffer.data() + y * cropSize * depth;
        std::memcpy(dstRow, srcRow, cropSize * depth);
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    auto *cropped = new Fl_RGB_Image(heapData, cropSize, cropSize, depth);
    cropped->alloc_array = 1;

    Fl_Image *scaled = cropped->copy(size, size);
    delete cropped;
    if (!scaled) {
        return nullptr;
    }

    return static_cast<Fl_RGB_Image *>(scaled);
}

bool isAttachmentImage(const Attachment &attachment) {
    if (attachment.contentType.has_value()) {
        return attachment.isImage();
    }
    return attachment.width.has_value() && attachment.height.has_value();
}

std::string formatFileSize(uint64_t size) {
    constexpr double kKB = 1024.0;
    constexpr double kMB = 1024.0 * 1024.0;
    constexpr double kGB = 1024.0 * 1024.0 * 1024.0;

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(1);

    if (size >= static_cast<uint64_t>(kGB)) {
        out << (size / kGB) << " GB";
    } else if (size >= static_cast<uint64_t>(kMB)) {
        out << (size / kMB) << " MB";
    } else if (size >= static_cast<uint64_t>(kKB)) {
        out << (size / kKB) << " KB";
    } else {
        out.unsetf(std::ios::fixed);
        out << size << " B";
    }

    return out.str();
}

Fl_RGB_Image *getAttachmentImage(const Attachment &attachment, int width, int height, bool squareCrop) {
    std::string url = getAttachmentUrl(attachment);
    if (url.empty() || width <= 0 || height <= 0) {
        return nullptr;
    }

    std::string cacheKey = buildAttachmentCacheKey(url, width, height, squareCrop);
    auto it = attachment_cache.find(cacheKey);
    if (it != attachment_cache.end()) {
        return it->second.get();
    }

    if (attachment_pending.find(cacheKey) == attachment_pending.end()) {
        attachment_pending.insert(cacheKey);
        Images::loadImageAsync(url, [cacheKey, url, width, height, squareCrop](Fl_RGB_Image *image) {
            if (image && image->w() > 0 && image->h() > 0) {
                Fl_RGB_Image *scaledRgb = nullptr;
                if (squareCrop) {
                    scaledRgb = cropAndScaleToSquare(image, width);
                } else {
                    Fl_Image *scaled = image->copy(width, height);
                    if (scaled) {
                        scaledRgb = static_cast<Fl_RGB_Image *>(scaled);
                    }
                }

                if (scaledRgb) {
                    Fl_RGB_Image *rounded = Images::makeRoundedRect(scaledRgb, width, height, kAttachmentCornerRadius);
                    if (rounded) {
                        attachment_cache[cacheKey] = std::unique_ptr<Fl_RGB_Image>(rounded);
                        delete scaledRgb;
                    } else {
                        attachment_cache[cacheKey] = std::unique_ptr<Fl_RGB_Image>(scaledRgb);
                    }
                    Images::evictFromMemory(url);
                }
            }
            attachment_pending.erase(cacheKey);
            Fl::redraw();
        });
    }

    return nullptr;
}

std::string selectJoinMessage(const Message &msg, const std::string &username) {
    (void)username;
    static const std::vector<std::string> kTemplates = {
        "A wild {user} appeared.",   "{user} just showed up!",
        "{user} just landed.",       "{user} just slid into the server.",
        "{user} is here.",           "{user} joined the party.",
        "Say hi to {user}.",         "Good to see you, {user}.",
        "Glad you're here, {user}.", "Welcome, {user}. We hope you brought pizza.",
        "Welcome {user}. Say hi!",   "Everyone welcome {user}!",
        "Yay you made it, {user}!"};

    if (kTemplates.empty()) {
        return "{user} joined the server.";
    }

    uint64_t idValue = 0;
    bool parsed = false;
    if (!msg.authorId.empty()) {
        try {
            idValue = std::stoull(msg.authorId);
            parsed = true;
        } catch (...) {
            parsed = false;
        }
    }

    if (!parsed) {
        idValue = static_cast<uint64_t>(std::hash<std::string>{}(msg.authorId));
    }

    size_t index = static_cast<size_t>(idValue % kTemplates.size());
    return kTemplates[index];
}

struct SystemMessageSpec {
    std::string templateText;
    std::string iconName;
    Fl_Color iconColor = ThemeColors::STATUS_ONLINE;
    Fl_Color highlightColor = ThemeColors::TEXT_NORMAL;
};

SystemMessageSpec buildSystemMessageSpec(const Message &msg, const std::string &username,
                                         const std::vector<std::string> &mentions) {
    (void)username;
    SystemMessageSpec spec;
    spec.iconName = "arrow";
    spec.iconColor = ThemeColors::STATUS_ONLINE;
    spec.highlightColor = ThemeColors::TEXT_NORMAL;

    switch (msg.type) {
    case MessageType::USER_JOIN:
        spec.templateText = selectJoinMessage(msg, username);
        return spec;
    case MessageType::RECIPIENT_ADD: {
        std::string target = mentions.empty() ? "someone" : mentions.front();
        spec.templateText = "{user} added " + target + ".";
        return spec;
    }
    case MessageType::RECIPIENT_REMOVE: {
        std::string target = mentions.empty() ? "someone" : mentions.front();
        spec.templateText = "{user} removed " + target + ".";
        return spec;
    }
    case MessageType::CALL:
        spec.templateText = "{user} started a call.";
        return spec;
    case MessageType::CHANNEL_NAME_CHANGE:
        if (!msg.content.empty()) {
            spec.templateText = "{user} changed the channel name to " + msg.content + ".";
        } else {
            spec.templateText = "{user} changed the channel name.";
        }
        return spec;
    case MessageType::CHANNEL_ICON_CHANGE:
        spec.templateText = "{user} changed the channel icon.";
        return spec;
    case MessageType::CHANNEL_PINNED_MESSAGE:
        spec.templateText = "{user} pinned a message to this channel.";
        return spec;
    case MessageType::GUILD_BOOST:
        spec.iconName = "boost";
        spec.iconColor = kBoostColor;
        spec.highlightColor = kBoostColor;
        spec.templateText = "{user} just boosted the server!";
        return spec;
    case MessageType::GUILD_BOOST_TIER_1:
        spec.iconName = "boost";
        spec.iconColor = kBoostColor;
        spec.highlightColor = kBoostColor;
        spec.templateText = "{user} just boosted the server to Level 1!";
        return spec;
    case MessageType::GUILD_BOOST_TIER_2:
        spec.iconName = "boost";
        spec.iconColor = kBoostColor;
        spec.highlightColor = kBoostColor;
        spec.templateText = "{user} just boosted the server to Level 2!";
        return spec;
    case MessageType::GUILD_BOOST_TIER_3:
        spec.iconName = "boost";
        spec.iconColor = kBoostColor;
        spec.highlightColor = kBoostColor;
        spec.templateText = "{user} just boosted the server to Level 3!";
        return spec;
    case MessageType::CHANNEL_FOLLOW_ADD:
        spec.templateText = msg.content.empty() ? "Added a channel follower." : msg.content;
        return spec;
    case MessageType::GUILD_DISCOVERY_DISQUALIFIED:
        spec.templateText = "This server is no longer eligible for Server Discovery.";
        return spec;
    case MessageType::GUILD_DISCOVERY_REQUALIFIED:
        spec.templateText = "This server is eligible for Server Discovery again.";
        return spec;
    case MessageType::GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING:
        spec.templateText = "This server entered the Discovery grace period.";
        return spec;
    case MessageType::GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING:
        spec.templateText = "Final warning: this server will lose Discovery eligibility soon.";
        return spec;
    case MessageType::THREAD_CREATED:
        if (!msg.content.empty()) {
            spec.templateText = "{user} started a thread: " + msg.content + ".";
        } else {
            spec.templateText = "{user} started a thread.";
        }
        return spec;
    case MessageType::CHAT_INPUT_COMMAND:
        if (!msg.content.empty()) {
            spec.templateText = "{user} used /" + msg.content + ".";
        } else {
            spec.templateText = "{user} used a slash command.";
        }
        return spec;
    case MessageType::THREAD_STARTER_MESSAGE:
        spec.templateText = "Thread started.";
        return spec;
    case MessageType::GUILD_INVITE_REMINDER:
        spec.templateText = "Invite friends to this server.";
        return spec;
    case MessageType::CONTEXT_MENU_COMMAND:
        spec.templateText = msg.content.empty() ? "{user} used a context menu command." : msg.content;
        return spec;
    case MessageType::AUTO_MODERATION_ACTION:
        spec.templateText = msg.content.empty() ? "An AutoMod action was taken." : msg.content;
        return spec;
    case MessageType::ROLE_SUBSCRIPTION_PURCHASE:
        spec.templateText = "{user} purchased a role subscription.";
        return spec;
    case MessageType::INTERACTION_PREMIUM_UPSELL:
        spec.templateText = "Discover premium features.";
        return spec;
    case MessageType::STAGE_START:
        spec.templateText = "{user} started a Stage.";
        return spec;
    case MessageType::STAGE_END:
        spec.templateText = "The Stage has ended.";
        return spec;
    case MessageType::STAGE_SPEAKER:
        spec.templateText = "{user} started speaking in the Stage.";
        return spec;
    case MessageType::STAGE_TOPIC:
        spec.templateText = msg.content.empty() ? "Stage topic updated." : "Stage topic: " + msg.content + ".";
        return spec;
    case MessageType::GUILD_APPLICATION_PREMIUM_SUBSCRIPTION:
        spec.templateText = "{user} upgraded the server with an application subscription.";
        return spec;
    case MessageType::POLL:
        spec.templateText = msg.content.empty() ? "Created a poll." : msg.content;
        return spec;
    default:
        spec.templateText = msg.content.empty() ? "System message" : msg.content;
        return spec;
    }
}

std::vector<MessageWidget::InlineItem> tokenizeStyledText(const std::string &text, Fl_Font font, int size,
                                                          Fl_Color color, const std::string &linkUrl = "",
                                                          bool underline = false, bool strikethrough = false) {
    std::vector<MessageWidget::InlineItem> tokens;
    std::string current;

    auto flushCurrent = [&]() {
        if (!current.empty()) {
            MessageWidget::InlineItem item;
            item.kind = MessageWidget::InlineItem::Kind::Text;
            item.text = current;
            item.font = font;
            item.size = size;
            item.color = color;
            item.linkUrl = linkUrl;
            item.isLink = !linkUrl.empty();
            item.underline = underline;
            item.strikethrough = strikethrough;
            tokens.push_back(std::move(item));
            current.clear();
        }
    };

    for (char ch : text) {
        if (ch == '\n') {
            flushCurrent();
            MessageWidget::InlineItem item;
            item.kind = MessageWidget::InlineItem::Kind::LineBreak;
            item.font = font;
            item.size = size;
            item.color = color;
            item.underline = underline;
            item.strikethrough = strikethrough;
            tokens.push_back(std::move(item));
        } else if (ch == ' ' || ch == '\t' || ch == '\r') {
            flushCurrent();
            if (tokens.empty() || tokens.back().kind == MessageWidget::InlineItem::Kind::LineBreak ||
                tokens.back().text != " ") {
                MessageWidget::InlineItem item;
                item.kind = MessageWidget::InlineItem::Kind::Text;
                item.text = " ";
                item.font = font;
                item.size = size;
                item.color = color;
                item.linkUrl = linkUrl;
                item.isLink = !linkUrl.empty();
                item.underline = underline;
                item.strikethrough = strikethrough;
                tokens.push_back(std::move(item));
            }
        } else {
            current.push_back(ch);
        }
    }

    flushCurrent();
    return tokens;
}

struct LinkRun {
    std::string text;
    std::string linkUrl;
    Fl_Color color = ThemeColors::TEXT_NORMAL;
};

struct MarkdownRun {
    std::string text;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;
    std::string linkUrl;
    Fl_Color color = ThemeColors::TEXT_NORMAL;
};

bool isHttpUrl(const std::string &url) { return url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0; }

bool isUrlStartAt(const std::string &text, size_t pos) {
    return text.compare(pos, 8, "https://") == 0 || text.compare(pos, 7, "http://") == 0;
}

bool isTrailingPunct(char ch) {
    switch (ch) {
    case '.':
    case ',':
    case '!':
    case '?':
    case ':':
    case ';':
    case ')':
    case ']':
    case '}':
        return true;
    default:
        return false;
    }
}

bool isAllDigits(const std::string &text) {
    return !text.empty() &&
           std::all_of(text.begin(), text.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

std::string buildEmojiUrl(const std::string &id, bool animated) {
    std::ostringstream out;
    out << "https://cdn.discordapp.com/emojis/" << id << (animated ? ".gif" : ".png") << "?size=" << kEmojiRequestSize;
    return out.str();
}

bool tryParseTimestamp(const std::string &text, size_t pos, size_t &outLength, std::string &outFormatted) {
    if (pos >= text.size() || text[pos] != '<') {
        return false;
    }

    size_t start = pos + 1;
    if (start >= text.size() || text[start] != 't') {
        return false;
    }
    start++;
    if (start >= text.size() || text[start] != ':') {
        return false;
    }
    start++;

    size_t colonOrEnd = start;
    while (colonOrEnd < text.size() && text[colonOrEnd] != ':' && text[colonOrEnd] != '>') {
        colonOrEnd++;
    }

    if (colonOrEnd >= text.size()) {
        return false;
    }

    std::string timestampStr = text.substr(start, colonOrEnd - start);
    if (!isAllDigits(timestampStr)) {
        return false;
    }

    char style = 'f';
    size_t closePos = colonOrEnd;
    if (text[colonOrEnd] == ':') {
        size_t stylePos = colonOrEnd + 1;
        if (stylePos < text.size() && text[stylePos] != '>') {
            style = text[stylePos];
            closePos = stylePos + 1;
        }
    }

    if (closePos >= text.size() || text[closePos] != '>') {
        return false;
    }

    try {
        int64_t timestamp = std::stoll(timestampStr);
        auto timePoint = std::chrono::system_clock::from_time_t(static_cast<time_t>(timestamp));
        auto timeT = std::chrono::system_clock::to_time_t(timePoint);
        std::tm tm;
        localtime_s(&tm, &timeT);

        auto now = std::chrono::system_clock::now();
        auto nowT = std::chrono::system_clock::to_time_t(now);
        std::tm nowTm;
        localtime_s(&nowTm, &nowT);

        std::ostringstream oss;
        switch (style) {
        case 't':
            oss << std::put_time(&tm, "%H:%M");
            break;
        case 'T':
            oss << std::put_time(&tm, "%H:%M:%S");
            break;
        case 'd':
            oss << std::put_time(&tm, "%d/%m/%Y");
            break;
        case 'D':
            oss << std::put_time(&tm, "%d %B %Y");
            break;
        case 'f':
            oss << std::put_time(&tm, "%d %B %Y %H:%M");
            break;
        case 'F':
            oss << std::put_time(&tm, "%A, %d %B %Y %H:%M");
            break;
        case 'R': {
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - timePoint).count();
            if (diff < 0) {
                diff = -diff;
                if (diff < 60)
                    oss << "in " << diff << " second" << (diff == 1 ? "" : "s");
                else if (diff < 3600)
                    oss << "in " << (diff / 60) << " minute" << ((diff / 60) == 1 ? "" : "s");
                else if (diff < 86400)
                    oss << "in " << (diff / 3600) << " hour" << ((diff / 3600) == 1 ? "" : "s");
                else if (diff < 2592000)
                    oss << "in " << (diff / 86400) << " day" << ((diff / 86400) == 1 ? "" : "s");
                else if (diff < 31536000)
                    oss << "in " << (diff / 2592000) << " month" << ((diff / 2592000) == 1 ? "" : "s");
                else
                    oss << "in " << (diff / 31536000) << " year" << ((diff / 31536000) == 1 ? "" : "s");
            } else {
                if (diff < 60)
                    oss << diff << " second" << (diff == 1 ? "" : "s") << " ago";
                else if (diff < 3600)
                    oss << (diff / 60) << " minute" << ((diff / 60) == 1 ? "" : "s") << " ago";
                else if (diff < 86400)
                    oss << (diff / 3600) << " hour" << ((diff / 3600) == 1 ? "" : "s") << " ago";
                else if (diff < 2592000)
                    oss << (diff / 86400) << " day" << ((diff / 86400) == 1 ? "" : "s") << " ago";
                else if (diff < 31536000)
                    oss << (diff / 2592000) << " month" << ((diff / 2592000) == 1 ? "" : "s") << " ago";
                else
                    oss << (diff / 31536000) << " year" << ((diff / 31536000) == 1 ? "" : "s") << " ago";
            }
            break;
        }
        default:
            oss << std::put_time(&tm, "%d %B %Y %H:%M");
            break;
        }

        outFormatted = oss.str();
        outLength = closePos - pos + 1;
        return true;
    } catch (...) {
        return false;
    }
}

bool tryParseMention(const std::string &text, size_t pos, size_t &outLength, std::string &outId, char &outType) {
    if (pos >= text.size() || text[pos] != '<') {
        return false;
    }

    size_t start = pos + 1;
    if (start >= text.size()) {
        return false;
    }

    char type = text[start];
    if (type == '@') {
        start++;
        if (start < text.size() && (text[start] == '!' || text[start] == '&')) {
            outType = text[start];
            start++;
        } else {
            outType = '@';
        }
    } else if (type == '#') {
        outType = '#';
        start++;
    } else {
        return false;
    }

    size_t idEnd = text.find('>', start);
    if (idEnd == std::string::npos) {
        return false;
    }

    std::string id = text.substr(start, idEnd - start);
    if (!isAllDigits(id)) {
        return false;
    }

    outId = id;
    outLength = idEnd - pos + 1;
    return true;
}

bool tryParseEmoji(const std::string &text, size_t pos, size_t &outLength, std::string &outUrl) {
    if (pos >= text.size() || text[pos] != '<') {
        return false;
    }

    bool animated = false;
    size_t start = pos + 1;
    if (start < text.size() && text[start] == 'a') {
        animated = true;
        start++;
    }
    if (start >= text.size() || text[start] != ':') {
        return false;
    }

    size_t nameStart = start + 1;
    size_t nameEnd = text.find(':', nameStart);
    if (nameEnd == std::string::npos) {
        return false;
    }

    size_t idStart = nameEnd + 1;
    size_t idEnd = text.find('>', idStart);
    if (idEnd == std::string::npos) {
        return false;
    }

    std::string id = text.substr(idStart, idEnd - idStart);
    if (!isAllDigits(id)) {
        return false;
    }

    outUrl = buildEmojiUrl(id, animated);
    outLength = idEnd - pos + 1;
    return true;
}

Fl_RGB_Image *getEmojiImage(const std::string &url, int size) {
    if (url.empty() || size <= 0) {
        return nullptr;
    }

    std::string cacheKey = buildEmojiCacheKey(url, size);
    auto it = emoji_cache.find(cacheKey);
    if (it != emoji_cache.end()) {
        return it->second.get();
    }

    if (emoji_pending.find(cacheKey) == emoji_pending.end()) {
        emoji_pending.insert(cacheKey);
        Images::loadImageAsync(url, [cacheKey, url, size](Fl_RGB_Image *image) {
            if (image && image->w() > 0 && image->h() > 0) {
                Fl_Image *scaled = image->copy(size, size);
                if (scaled) {
                    emoji_cache[cacheKey] = std::unique_ptr<Fl_RGB_Image>(static_cast<Fl_RGB_Image *>(scaled));
                }
                Images::evictFromMemory(url);
            }
            emoji_pending.erase(cacheKey);
            Fl::redraw();
        });
    }

    return nullptr;
}

std::vector<LinkRun> parseLinkRuns(const std::string &text, Fl_Color baseColor, Fl_Color linkColor) {
    std::vector<LinkRun> runs;
    size_t i = 0;
    size_t last = 0;

    auto flushNormal = [&](size_t start, size_t end) {
        if (end > start) {
            runs.push_back({text.substr(start, end - start), "", baseColor});
        }
    };

    while (i < text.size()) {
        if (text[i] == '[') {
            size_t close = text.find(']', i + 1);
            if (close != std::string::npos && close + 1 < text.size() && text[close + 1] == '(') {
                size_t urlStart = close + 2;
                size_t urlEnd = text.find(')', urlStart);
                if (urlEnd != std::string::npos) {
                    std::string label = text.substr(i + 1, close - i - 1);
                    std::string url = text.substr(urlStart, urlEnd - urlStart);

                    if (!url.empty() && url.front() == '<' && url.back() == '>') {
                        url = url.substr(1, url.size() - 2);
                    }

                    if (isHttpUrl(url)) {
                        flushNormal(last, i);
                        runs.push_back({label, url, linkColor});
                        i = urlEnd + 1;
                        last = i;
                        continue;
                    }
                }
            }
        }

        if (isUrlStartAt(text, i)) {
            flushNormal(last, i);
            size_t end = i;
            while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) {
                end++;
            }

            std::string url = text.substr(i, end - i);
            std::string suffix;
            while (!url.empty() && isTrailingPunct(url.back())) {
                suffix.insert(suffix.begin(), url.back());
                url.pop_back();
            }

            if (!url.empty()) {
                runs.push_back({url, url, linkColor});
            }
            if (!suffix.empty()) {
                runs.push_back({suffix, "", baseColor});
            }

            i = end;
            last = i;
            continue;
        }

        i++;
    }

    flushNormal(last, text.size());
    return runs;
}

bool hasClosingMarker(const std::vector<LinkRun> &runs, size_t runIndex, size_t start, const std::string &marker) {
    for (size_t i = runIndex; i < runs.size(); ++i) {
        const std::string &text = runs[i].text;
        size_t offset = (i == runIndex) ? start : 0;
        if (text.find(marker, offset) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<MarkdownRun> parseMarkdownRuns(const std::vector<LinkRun> &runs) {
    std::vector<MarkdownRun> output;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strikethrough = false;

    for (size_t runIndex = 0; runIndex < runs.size(); ++runIndex) {
        const auto &run = runs[runIndex];
        std::string current;
        auto flushCurrent = [&]() {
            if (!current.empty()) {
                MarkdownRun out;
                out.text = current;
                out.bold = bold;
                out.italic = italic;
                out.underline = underline;
                out.strikethrough = strikethrough;
                out.linkUrl = run.linkUrl;
                out.color = run.color;
                output.push_back(std::move(out));
                current.clear();
            }
        };

        bool skipMarkdown = !run.linkUrl.empty() && run.text == run.linkUrl && isHttpUrl(run.text);
        if (skipMarkdown) {
            current = run.text;
            flushCurrent();
            continue;
        }

        size_t i = 0;
        while (i < run.text.size()) {
            if (run.text.compare(i, 3, "***") == 0) {
                bool canToggle =
                    (bold && italic) || (!bold && !italic && hasClosingMarker(runs, runIndex, i + 3, "***"));
                if (canToggle) {
                    flushCurrent();
                    bold = !bold;
                    italic = !italic;
                    i += 3;
                    continue;
                }
            }
            if (run.text.compare(i, 2, "**") == 0) {
                bool canToggle = bold || hasClosingMarker(runs, runIndex, i + 2, "**");
                if (canToggle) {
                    flushCurrent();
                    bold = !bold;
                    i += 2;
                    continue;
                }
            }
            if (run.text.compare(i, 2, "__") == 0) {
                bool canToggle = underline || hasClosingMarker(runs, runIndex, i + 2, "__");
                if (canToggle) {
                    flushCurrent();
                    underline = !underline;
                    i += 2;
                    continue;
                }
            }
            if (run.text.compare(i, 2, "~~") == 0) {
                bool canToggle = strikethrough || hasClosingMarker(runs, runIndex, i + 2, "~~");
                if (canToggle) {
                    flushCurrent();
                    strikethrough = !strikethrough;
                    i += 2;
                    continue;
                }
            }
            if (run.text[i] == '*') {
                bool canToggle = italic || hasClosingMarker(runs, runIndex, i + 1, "*");
                if (canToggle) {
                    flushCurrent();
                    italic = !italic;
                    i += 1;
                    continue;
                }
            }
            if (run.text[i] == '_') {
                bool canToggle = italic || hasClosingMarker(runs, runIndex, i + 1, "_");
                if (canToggle) {
                    flushCurrent();
                    italic = !italic;
                    i += 1;
                    continue;
                }
            }

            current.push_back(run.text[i]);
            i += 1;
        }

        flushCurrent();
    }

    return output;
}

Fl_Font resolveMarkdownFont(Fl_Font baseFont, bool bold, bool italic) {
    if (bold && italic) {
        return FontLoader::Fonts::INTER_BOLD_ITALIC;
    }
    if (bold) {
        return FontLoader::Fonts::INTER_BOLD;
    }
    if (italic) {
        return FontLoader::Fonts::INTER_REGULAR_ITALIC;
    }
    return baseFont;
}

std::vector<MessageWidget::InlineItem> tokenizeEmojiText(const std::string &text, Fl_Font font, int size,
                                                         Fl_Color color, const std::string &linkUrl, bool underline,
                                                         bool strikethrough, const Message *msg) {
    std::vector<MessageWidget::InlineItem> tokens;
    std::string current;
    size_t i = 0;

    auto flushText = [&]() {
        if (!current.empty()) {
            auto textTokens = tokenizeStyledText(current, font, size, color, linkUrl, underline, strikethrough);
            tokens.insert(tokens.end(), textTokens.begin(), textTokens.end());
            current.clear();
        }
    };

    while (i < text.size()) {
        size_t length = 0;

        if (text.compare(i, 9, "@everyone") == 0) {
            flushText();
            auto mentionTokens = tokenizeStyledText("@everyone", FontLoader::Fonts::INTER_SEMIBOLD, size,
                                                    ThemeColors::TEXT_LINK, "", false, false);
            tokens.insert(tokens.end(), mentionTokens.begin(), mentionTokens.end());
            i += 9;
            continue;
        }

        if (text.compare(i, 5, "@here") == 0) {
            flushText();
            auto mentionTokens = tokenizeStyledText("@here", FontLoader::Fonts::INTER_SEMIBOLD, size,
                                                    ThemeColors::TEXT_LINK, "", false, false);
            tokens.insert(tokens.end(), mentionTokens.begin(), mentionTokens.end());
            i += 5;
            continue;
        }

        std::string timestampText;
        if (tryParseTimestamp(text, i, length, timestampText)) {
            flushText();
            auto timestampTokens =
                tokenizeStyledText(timestampText, font, size, ThemeColors::TEXT_LINK, "", true, false);
            tokens.insert(tokens.end(), timestampTokens.begin(), timestampTokens.end());
            i += length;
            continue;
        }

        std::string mentionId;
        char mentionType;
        if (tryParseMention(text, i, length, mentionId, mentionType)) {
            flushText();
            std::string mentionText;
            if (mentionType == '#') {
                mentionText = "#unknown-channel";
            } else if (mentionType == '&') {
                mentionText = "@Unknown Role";
                if (msg && msg->guildId.has_value()) {
                    // TODO: Look up role name from AppState
                }
            } else {
                mentionText = "@Unknown User";
                if (msg) {
                    for (size_t j = 0; j < msg->mentionIds.size(); ++j) {
                        if (msg->mentionIds[j] == mentionId) {
                            if (j < msg->mentionDisplayNames.size() && !msg->mentionDisplayNames[j].empty()) {
                                mentionText = "@" + msg->mentionDisplayNames[j];
                            }
                            break;
                        }
                    }
                }
            }
            auto mentionTokens = tokenizeStyledText(mentionText, FontLoader::Fonts::INTER_SEMIBOLD, size,
                                                    ThemeColors::TEXT_LINK, "", false, false);
            tokens.insert(tokens.end(), mentionTokens.begin(), mentionTokens.end());
            i += length;
            continue;
        }

        std::string emojiUrl;
        if (tryParseEmoji(text, i, length, emojiUrl)) {
            flushText();
            MessageWidget::InlineItem item;
            item.kind = MessageWidget::InlineItem::Kind::Emoji;
            item.emojiUrl = emojiUrl;
            item.emojiSize = kEmojiSize;
            item.width = kEmojiSize;
            item.emojiCacheKey = buildEmojiCacheKey(emojiUrl, kEmojiSize);
            tokens.push_back(std::move(item));
            i += length;
            continue;
        }

        current.push_back(text[i]);
        i += 1;
    }

    flushText();
    return tokens;
}

int drawInlineItem(const MessageWidget::InlineItem &item, int x, int baseline, bool useMuted) {
    if (item.kind == MessageWidget::InlineItem::Kind::Emoji) {
        int size = item.emojiSize > 0 ? item.emojiSize : kEmojiSize;
        int drawY = baseline - size + kEmojiBaselineOffset;
        Fl_RGB_Image *emoji = getEmojiImage(item.emojiUrl, size);
        if (emoji && emoji->w() > 0 && emoji->h() > 0) {
            emoji->draw(x, drawY);
        }
        return size;
    }

    if (item.kind != MessageWidget::InlineItem::Kind::Text || item.text.empty()) {
        return 0;
    }

    fl_color(useMuted ? ThemeColors::TEXT_MUTED : item.color);
    fl_font(item.font, item.size);
    fl_draw(item.text.c_str(), x, baseline);

    int width = item.width;
    if (width <= 0) {
        width = static_cast<int>(fl_width(item.text.c_str()));
    }

    if (item.underline) {
        int underlineY = baseline + 1;
        fl_line(x, underlineY, x + width, underlineY);
    }
    if (item.strikethrough) {
        int ascent = fl_height() - fl_descent();
        int strikeY = baseline - ascent / 2;
        fl_line(x, strikeY, x + width, strikeY);
    }

    return width;
}

std::string ellipsizeText(const std::string &text, int maxWidth, Fl_Font font, int size) {
    if (maxWidth <= 0 || text.empty()) {
        return "";
    }

    fl_font(font, size);
    int fullWidth = static_cast<int>(fl_width(text.c_str()));
    if (fullWidth <= maxWidth) {
        return text;
    }

    const char *ellipsis = "...";
    int ellipsisWidth = static_cast<int>(fl_width(ellipsis));
    if (ellipsisWidth >= maxWidth) {
        return "";
    }

    std::string trimmed = text;
    while (!trimmed.empty()) {
        trimmed.pop_back();
        int width = static_cast<int>(fl_width(trimmed.c_str()));
        if (width <= maxWidth - ellipsisWidth) {
            break;
        }
    }

    return trimmed.empty() ? std::string(ellipsis) : trimmed + ellipsis;
}

std::vector<MessageWidget::InlineItem> buildSystemTokens(const std::string &templ, const std::string &username,
                                                         Fl_Font baseFont, Fl_Font highlightFont, int size,
                                                         Fl_Color baseColor, Fl_Color highlightColor) {
    std::vector<MessageWidget::InlineItem> tokens;
    const std::string token = "{user}";
    size_t pos = 0;
    while (pos < templ.size()) {
        size_t found = templ.find(token, pos);
        if (found == std::string::npos) {
            auto partTokens = tokenizeStyledText(templ.substr(pos), baseFont, size, baseColor);
            tokens.insert(tokens.end(), partTokens.begin(), partTokens.end());
            break;
        }

        if (found > pos) {
            auto partTokens = tokenizeStyledText(templ.substr(pos, found - pos), baseFont, size, baseColor);
            tokens.insert(tokens.end(), partTokens.begin(), partTokens.end());
        }

        auto userTokens = tokenizeStyledText(username, highlightFont, size, highlightColor);
        tokens.insert(tokens.end(), userTokens.begin(), userTokens.end());
        pos = found + token.size();
    }

    return tokens;
}

std::vector<MessageWidget::InlineItem> tokenizeTextWithMessage(const std::string &text, Fl_Font font, int size,
                                                               Fl_Color color, const Message *msg) {
    std::vector<MessageWidget::InlineItem> tokens;
    auto runs = parseLinkRuns(text, color, ThemeColors::TEXT_LINK);
    auto mdRuns = parseMarkdownRuns(runs);
    for (const auto &mdRun : mdRuns) {
        Fl_Font runFont = resolveMarkdownFont(font, mdRun.bold, mdRun.italic);
        auto runTokens = tokenizeEmojiText(mdRun.text, runFont, size, mdRun.color, mdRun.linkUrl, mdRun.underline,
                                           mdRun.strikethrough, msg);
        tokens.insert(tokens.end(), runTokens.begin(), runTokens.end());
    }
    return tokens;
}
} // namespace

MessageWidget::Layout MessageWidget::buildLayout(const Message &msg, int viewWidth, bool isGrouped, bool compactBottom,
                                                 const ReplyPreview *replyPreview) {
    Layout layout;
    layout.isSystem = msg.isSystemMessage();
    layout.grouped = isGrouped;
    layout.username = msg.getAuthorDisplayName();
    layout.viewWidth = viewWidth;
    layout.avatarSize = kAvatarSize;

    auto msgTime = std::chrono::system_clock::to_time_t(msg.timestamp);
    std::tm msgTm;
    localtime_s(&msgTm, &msgTime);

    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm nowTm;
    localtime_s(&nowTm, &nowTime);

    std::ostringstream timeStream;
    bool isToday = (msgTm.tm_year == nowTm.tm_year && msgTm.tm_mon == nowTm.tm_mon && msgTm.tm_mday == nowTm.tm_mday);
    bool isYesterday = false;

    if (!isToday) {
        auto yesterday = now - std::chrono::hours(24);
        auto yesterdayTime = std::chrono::system_clock::to_time_t(yesterday);
        std::tm yesterdayTm;
        localtime_s(&yesterdayTm, &yesterdayTime);
        isYesterday = (msgTm.tm_year == yesterdayTm.tm_year && msgTm.tm_mon == yesterdayTm.tm_mon &&
                       msgTm.tm_mday == yesterdayTm.tm_mday);
    }

    if (isToday) {
        timeStream << std::put_time(&msgTm, "%I:%M %p");
    } else if (isYesterday) {
        timeStream << "Yesterday at " << std::put_time(&msgTm, "%I:%M %p");
    } else {
        timeStream << std::put_time(&msgTm, "%d/%m/%Y %I:%M %p");
    }

    layout.time = timeStream.str();

    layout.avatarX = kLeftMargin;
    layout.avatarY = 0;
    layout.usernameX = kLeftMargin + layout.avatarSize + kAvatarMargin;
    layout.contentX = layout.usernameX;
    layout.contentWidth = std::max(0, viewWidth - layout.contentX - kRightMargin);

    int bottomPadding = kContentBottomPadding;
    bool hasReply = false;

    if (!layout.isSystem && replyPreview != nullptr && !replyPreview->author.empty()) {
        hasReply = true;
        layout.hasReply = true;
        layout.replyX = layout.contentX;
        layout.replyLineX = layout.avatarX + layout.avatarSize / 2;
        if (layout.replyLineX < kLeftMargin) {
            layout.replyLineX = kLeftMargin;
        }

        fl_font(FontLoader::Fonts::INTER_REGULAR, kReplyFontSize);
        int replyFontHeight = fl_height();
        int replyAscent = replyFontHeight - fl_descent();
        layout.replyHeight = replyFontHeight;

        int replyTop = isGrouped ? kGroupedTopPadding : kReplyTopPadding;
        layout.replyLineTop = replyTop;
        layout.replyLineBottom = replyTop + layout.replyHeight;
        layout.replyBaseline = replyTop + replyAscent;

        int maxReplyWidth = layout.contentWidth;
        std::string snippet = replyPreview->content;

        fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReplyFontSize);
        int authorWidth = static_cast<int>(fl_width(replyPreview->author.c_str()));
        fl_font(FontLoader::Fonts::INTER_REGULAR, kReplyFontSize);
        int spaceWidth = static_cast<int>(fl_width(" "));
        int availableSnippetWidth = std::max(0, maxReplyWidth - authorWidth - spaceWidth);
        if (availableSnippetWidth > 0 && !snippet.empty()) {
            snippet = ellipsizeText(snippet, availableSnippetWidth, FontLoader::Fonts::INTER_REGULAR, kReplyFontSize);
        }

        layout.replyItems = tokenizeStyledText(replyPreview->author, FontLoader::Fonts::INTER_SEMIBOLD, kReplyFontSize,
                                               ThemeColors::TEXT_NORMAL);
        if (!snippet.empty()) {
            auto spacer =
                tokenizeStyledText(" ", FontLoader::Fonts::INTER_REGULAR, kReplyFontSize, ThemeColors::TEXT_MUTED);
            layout.replyItems.insert(layout.replyItems.end(), spacer.begin(), spacer.end());
            auto snippetItems =
                tokenizeStyledText(snippet, FontLoader::Fonts::INTER_REGULAR, kReplyFontSize, ThemeColors::TEXT_MUTED);
            layout.replyItems.insert(layout.replyItems.end(), snippetItems.begin(), snippetItems.end());
        }

        if (!isGrouped) {
            layout.avatarY = replyTop + layout.replyHeight + kReplyToHeaderPadding;
        }
    }

    if (!layout.isSystem && !isGrouped) {
        fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kUsernameFontSize);
        int usernameAscent = fl_height() - fl_descent();
        layout.headerBaseline = layout.avatarY + kHeaderTopPadding + usernameAscent;
        int usernameWidth = static_cast<int>(fl_width(layout.username.c_str()));

        fl_font(FontLoader::Fonts::INTER_REGULAR, kTimestampFontSize);
        int timeAscent = fl_height() - fl_descent();
        layout.timeX = layout.usernameX + usernameWidth + kTimestampGap;
        layout.timeBaseline = layout.avatarY + kHeaderTopPadding + timeAscent + kTimestampBaselineAdjust;
    }

    std::vector<std::string> mentionNames = msg.mentionDisplayNames.empty() ? msg.mentionIds : msg.mentionDisplayNames;
    int lineAscent = 0;
    int contentHeight = 0;
    int contentTop = 0;

    if (layout.isSystem) {
        SystemMessageSpec spec = buildSystemMessageSpec(msg, layout.username, mentionNames);
        layout.systemIconName = spec.iconName;
        layout.systemIconColor = spec.iconColor;
        layout.systemIconSize = kSystemIconSize;

        layout.contentX = kLeftMargin + kSystemIconSize + kSystemIconGap;
        layout.contentWidth = std::max(0, viewWidth - kRightMargin - layout.contentX);

        auto tokens = buildSystemTokens(spec.templateText, layout.username, FontLoader::Fonts::INTER_REGULAR,
                                        FontLoader::Fonts::INTER_SEMIBOLD, kSystemFontSize, ThemeColors::TEXT_MUTED,
                                        spec.highlightColor);

        fl_font(FontLoader::Fonts::INTER_REGULAR, kSystemFontSize);
        int fontHeight = fl_height();
        int fontAscent = fontHeight - fl_descent();

        layout.lines = wrapTokens(tokens, layout.contentWidth);
        layout.lineHeight = std::max(fontHeight, kSystemIconSize);
        layout.lineSpacing = kSystemLineSpacing;
        lineAscent = fontAscent;

        int lineCount = layout.lines.empty() ? 1 : static_cast<int>(layout.lines.size());
        contentHeight = lineCount * layout.lineHeight + (lineCount - 1) * layout.lineSpacing;
        contentTop = kSystemTopPadding;
        layout.contentBaseline = contentTop + lineAscent;

        int lastLineWidth = (layout.lines.empty() ? 0 : layout.lines.back().width);
        int lastLineBaseline = layout.contentBaseline + (lineCount - 1) * (layout.lineHeight + layout.lineSpacing);

        fl_font(FontLoader::Fonts::INTER_REGULAR, kTimestampFontSize);
        int timeWidth = static_cast<int>(fl_width(layout.time.c_str()));
        int timeXCandidate = layout.contentX + lastLineWidth + kSystemTimeGap;
        int contentRight = layout.contentX + layout.contentWidth;

        if (timeXCandidate + timeWidth <= contentRight) {
            layout.timeX = timeXCandidate;
            layout.timeBaseline = lastLineBaseline;
        } else {
            layout.timeX = layout.contentX;
            layout.timeBaseline = lastLineBaseline + layout.lineHeight + layout.lineSpacing;
            contentHeight += layout.lineHeight + layout.lineSpacing;
        }

        layout.height = contentTop + contentHeight + kSystemBottomPadding;

        layout.systemIconX = kLeftMargin;
        layout.systemIconY = contentTop + (layout.lineHeight - kSystemIconSize) / 2;
    } else {
        auto tokens = tokenizeTextWithMessage(msg.content, FontLoader::Fonts::INTER_REGULAR, kContentFontSize,
                                              ThemeColors::TEXT_NORMAL, &msg);
        bool emojiOnly = true;
        int maxEmojiSize = 0;
        for (const auto &token : tokens) {
            if (token.kind == InlineItem::Kind::Emoji) {
                maxEmojiSize = std::max(maxEmojiSize, token.emojiSize > 0 ? token.emojiSize : kEmojiSize);
                continue;
            }
            if (token.kind == InlineItem::Kind::LineBreak) {
                continue;
            }
            if (token.kind == InlineItem::Kind::Text && token.text.find_first_not_of(" \t\r\n") == std::string::npos) {
                continue;
            }
            emojiOnly = false;
            break;
        }

        if (emojiOnly && maxEmojiSize > 0) {
            for (auto &token : tokens) {
                if (token.kind == InlineItem::Kind::Emoji && !token.emojiUrl.empty()) {
                    token.emojiSize = kEmojiOnlySize;
                    token.width = kEmojiOnlySize;
                    token.emojiCacheKey = buildEmojiCacheKey(token.emojiUrl, kEmojiOnlySize);
                }
            }
            maxEmojiSize = kEmojiOnlySize;
        }

        bool hasContent = msg.content.find_first_not_of(" \t\r\n") != std::string::npos;
        if (hasContent && msg.wasEdited()) {
            auto editedTokens = tokenizeStyledText(" (edited)", FontLoader::Fonts::INTER_REGULAR, kEditedFontSize,
                                                   ThemeColors::TEXT_MUTED);
            tokens.insert(tokens.end(), editedTokens.begin(), editedTokens.end());
        }

        fl_font(FontLoader::Fonts::INTER_REGULAR, kContentFontSize);
        layout.lines = wrapTokens(tokens, layout.contentWidth);
        layout.lineHeight = fl_height();
        layout.lineSpacing = kLineSpacing;
        lineAscent = layout.lineHeight - fl_descent();

        if (!hasContent) {
            layout.lines.clear();
            contentHeight = 0;
            lineAscent = 0;
        } else {
            if (maxEmojiSize > 0) {
                layout.lineHeight = std::max(layout.lineHeight, maxEmojiSize);
                lineAscent = layout.lineHeight - fl_descent();
            }
            int lineCount = layout.lines.empty() ? 1 : static_cast<int>(layout.lines.size());
            contentHeight = lineCount * layout.lineHeight + (lineCount - 1) * layout.lineSpacing;
        }

        if (isGrouped) {
            if (hasReply) {
                contentTop = layout.replyLineBottom + kReplyToContentPadding;
            } else {
                contentTop = kGroupedTopPadding;
            }
        } else {
            int contentPad = hasContent ? kHeaderToContentPadding : 2;
            contentTop = layout.headerBaseline + contentPad;
        }

        layout.contentBaseline = contentTop + lineAscent;
        bottomPadding = layout.grouped ? kGroupedBottomPadding : (compactBottom ? 0 : kContentBottomPadding);
        if (isGrouped) {
            layout.height = contentTop + contentHeight + bottomPadding;
        } else {
            layout.height = std::max(layout.avatarY + layout.avatarSize, contentTop + contentHeight + bottomPadding);
        }
    }

    layout.contentTop = contentTop;
    layout.contentHeight = contentHeight;
    int attachmentsBottomPadding = 0;

    if (!layout.isSystem && !msg.attachments.empty()) {
        int maxWidth = std::min(layout.contentWidth, kAttachmentMaxWidth);
        int cursorY = 0;
        if (maxWidth > 0) {
            size_t idx = 0;

            while (idx < msg.attachments.size()) {
                const Attachment &attachment = msg.attachments[idx];
                if (!isAttachmentImage(attachment)) {
                    MessageWidget::AttachmentLayout attachmentLayout;
                    attachmentLayout.isImage = false;
                    attachmentLayout.squareCrop = false;
                    attachmentLayout.width = std::min(maxWidth, kAttachmentFileMaxWidth);
                    attachmentLayout.height = kAttachmentFileHeight;
                    attachmentLayout.xOffset = 0;
                    attachmentLayout.yOffset = cursorY;
                    attachmentLayout.downloadSize = kAttachmentDownloadButtonSize;
                    attachmentLayout.downloadXOffset = std::max(
                        0, attachmentLayout.width - kAttachmentDownloadButtonSize - kAttachmentDownloadPadding);
                    attachmentLayout.downloadYOffset = std::max(0, (attachmentLayout.height - kAttachmentDownloadButtonSize) / 2);
                    layout.attachments.push_back(attachmentLayout);

                    cursorY += attachmentLayout.height + kAttachmentSpacing;
                    idx++;
                    continue;
                }

                size_t start = idx;
                size_t end = idx;
                while (end < msg.attachments.size() && isAttachmentImage(msg.attachments[end])) {
                    end++;
                }

                size_t rowStart = start;
                while (rowStart < end) {
                    size_t rowCount = std::min<size_t>(2, end - rowStart);
                    int gapTotal = kAttachmentSpacing * static_cast<int>(rowCount - 1);
                    int slotWidth = (maxWidth - gapTotal) / static_cast<int>(rowCount);
                    if (slotWidth <= 0) {
                        slotWidth = 1;
                    }
                    int rowHeight = 0;
                    int rowX = 0;

                    std::vector<MessageWidget::AttachmentLayout> rowLayouts;
                    rowLayouts.reserve(rowCount);

                    for (size_t r = 0; r < rowCount; ++r) {
                        const Attachment &rowAttachment = msg.attachments[rowStart + r];
                        MessageWidget::AttachmentLayout attachmentLayout;
                        attachmentLayout.isImage = true;
                        attachmentLayout.squareCrop = (rowCount == 2);

                        int scaledW = slotWidth;
                        int scaledH = slotWidth;

                        if (!attachmentLayout.squareCrop) {
                            int naturalW = rowAttachment.width.value_or(kAttachmentMaxWidth);
                            int naturalH = rowAttachment.height.value_or(kAttachmentMaxHeight);
                            if (naturalW <= 0) {
                                naturalW = kAttachmentMaxWidth;
                            }
                            if (naturalH <= 0) {
                                naturalH = kAttachmentMaxHeight;
                            }

                            scaledW = std::min(naturalW, kAttachmentMaxWidth);
                            scaledH = std::min(naturalH, kAttachmentMaxHeight);

                            if (naturalW > kAttachmentMaxWidth || naturalH > kAttachmentMaxHeight) {
                                double aspect = static_cast<double>(naturalH) / static_cast<double>(naturalW);
                                if (naturalW > kAttachmentMaxWidth) {
                                    scaledW = kAttachmentMaxWidth;
                                    scaledH = static_cast<int>(std::round(static_cast<double>(scaledW) * aspect));
                                }
                                if (scaledH > kAttachmentMaxHeight) {
                                    scaledH = kAttachmentMaxHeight;
                                    scaledW = static_cast<int>(std::round(static_cast<double>(scaledH) / aspect));
                                }
                            }

                            scaledW = std::max(kAttachmentMinSize, std::min(scaledW, slotWidth));
                            scaledH = std::max(kAttachmentMinSize, scaledH);
                        } else {
                            int squareSize = std::min(slotWidth, kAttachmentMaxHeight);
                            scaledW = squareSize;
                            scaledH = squareSize;
                        }

                        attachmentLayout.width = scaledW;
                        attachmentLayout.height = scaledH;
                        attachmentLayout.xOffset = rowX;
                        attachmentLayout.yOffset = cursorY;

                        std::string attachmentUrl = getAttachmentUrl(rowAttachment);
                        if (!attachmentUrl.empty()) {
                            attachmentLayout.cacheKey =
                                buildAttachmentCacheKey(attachmentUrl, attachmentLayout.width, attachmentLayout.height,
                                                        attachmentLayout.squareCrop);
                        }

                        rowHeight = std::max(rowHeight, attachmentLayout.height);
                        rowLayouts.push_back(std::move(attachmentLayout));
                        rowX += slotWidth + kAttachmentSpacing;
                    }

                    for (auto &layoutEntry : rowLayouts) {
                        layout.attachments.push_back(std::move(layoutEntry));
                    }

                    cursorY += rowHeight + kAttachmentSpacing;
                    rowStart += rowCount;
                }

                idx = end;
            }
        }

        if (!layout.attachments.empty()) {
            if (cursorY > 0) {
                cursorY -= kAttachmentSpacing;
            }
            layout.attachmentsTopPadding =
                (layout.contentHeight > 0) ? kAttachmentTopPadding : kAttachmentTopPaddingNoContent;
            layout.attachmentsHeight = layout.attachmentsTopPadding + cursorY;
            attachmentsBottomPadding = kContentBottomPadding;
        }
    }

    if (!layout.isSystem && !msg.reactions.empty()) {
        int maxWidth = layout.contentWidth;
        if (maxWidth > 0) {
            int cursorX = 0;
            int cursorY = 0;

            fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionFontSize);
            fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionEmojiFontSize);

            for (const auto &reaction : msg.reactions) {
                MessageWidget::ReactionLayout reactionLayout;
                reactionLayout.me = reaction.me;
                reactionLayout.isCustomEmoji = !reaction.emojiId.empty();
                reactionLayout.animated = reaction.emojiAnimated;
                reactionLayout.emojiName = reaction.emojiName;
                reactionLayout.emojiSize = kReactionEmojiSize;
                reactionLayout.countText = std::to_string(std::max(0, reaction.count));

                int emojiWidth = kReactionEmojiSize;
                if (reactionLayout.isCustomEmoji) {
                    reactionLayout.emojiUrl = buildEmojiUrl(reaction.emojiId, reaction.emojiAnimated);
                    if (!reactionLayout.emojiUrl.empty()) {
                        reactionLayout.emojiCacheKey =
                            buildEmojiCacheKey(reactionLayout.emojiUrl, reactionLayout.emojiSize);
                    }
                } else {
                    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionEmojiFontSize);
                    if (!reactionLayout.emojiName.empty()) {
                        emojiWidth = static_cast<int>(fl_width(reactionLayout.emojiName.c_str()));
                    }
                }
                if (emojiWidth <= 0) {
                    emojiWidth = kReactionEmojiSize;
                }
                reactionLayout.emojiWidth = emojiWidth;

                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionFontSize);
                int countWidth = static_cast<int>(fl_width(reactionLayout.countText.c_str()));
                int pillWidth = kReactionPaddingX + emojiWidth + kReactionEmojiGap + countWidth + kReactionPaddingX;
                if (pillWidth < kReactionHeight) {
                    pillWidth = kReactionHeight;
                }

                if (cursorX > 0 && cursorX + pillWidth > maxWidth) {
                    cursorX = 0;
                    cursorY += kReactionHeight + kReactionRowSpacing;
                }

                reactionLayout.width = pillWidth;
                reactionLayout.height = kReactionHeight;
                reactionLayout.xOffset = cursorX;
                reactionLayout.yOffset = cursorY;
                layout.reactions.push_back(std::move(reactionLayout));

                cursorX += pillWidth + kReactionSpacing;
            }

            if (!layout.reactions.empty()) {
                layout.reactionsTopPadding = (layout.contentHeight > 0 || layout.attachmentsHeight > 0)
                                                 ? kReactionTopPadding
                                                 : kReactionTopPaddingNoContent;
                layout.reactionsHeight =
                    layout.reactionsTopPadding + cursorY + kReactionHeight + kReactionBottomPadding;
            }
        }
    }

    if (!layout.isSystem) {
        int contentBottom =
            layout.contentTop + layout.contentHeight + layout.attachmentsHeight + layout.reactionsHeight;

        int finalBottomPadding = bottomPadding;
        if (!layout.attachments.empty()) {
            finalBottomPadding = std::max(finalBottomPadding, attachmentsBottomPadding);
        }
        if (!layout.reactions.empty()) {
            finalBottomPadding = std::max(finalBottomPadding, kContentBottomPadding);
        }

        if (layout.grouped) {
            layout.height = contentBottom + finalBottomPadding;
        } else {
            layout.height = std::max(layout.avatarY + layout.avatarSize, contentBottom + finalBottomPadding);
        }
    }

    return layout;
}

void MessageWidget::draw(const Message &msg, const Layout &layout, int originX, int originY, bool avatarHovered) {
    const bool isPending = msg.isPending;
    if (layout.isSystem) {
        if (!layout.systemIconName.empty()) {
            auto *icon =
                IconManager::load_recolored_icon(layout.systemIconName, layout.systemIconSize, layout.systemIconColor);
            if (icon) {
                icon->draw(originX + layout.systemIconX, originY + layout.systemIconY);
            }
        }

        int lineY = originY + layout.contentBaseline;
        for (const auto &line : layout.lines) {
            int cursorX = originX + layout.contentX;
            for (const auto &item : line.items) {
                cursorX += drawInlineItem(item, cursorX, lineY, isPending);
            }
            lineY += layout.lineHeight + layout.lineSpacing;
        }

        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, kTimestampFontSize);
        fl_draw(layout.time.c_str(), originX + layout.timeX, originY + layout.timeBaseline);
        return;
    }

    if (!layout.grouped) {
        fl_color(ThemeColors::BG_TERTIARY);
        fl_pie(originX + layout.avatarX, originY + layout.avatarY, layout.avatarSize, layout.avatarSize, 0, 360);

        Fl_RGB_Image *avatar = nullptr;

        if (avatarHovered) {
            std::string gifUrl = getAnimatedAvatarUrl(msg, layout.avatarSize);
            if (!gifUrl.empty()) {
                std::string gifKey = buildAvatarCacheKey(gifUrl, layout.avatarSize);
                if (ensureAnimatedAvatar(gifUrl, layout.avatarSize)) {
                    if (gifKey == hovered_avatar_key) {
                        startAvatarAnimation(gifKey);
                    }
                    auto it = avatar_gif_cache.find(gifKey);
                    if (it != avatar_gif_cache.end() && it->second.animation) {
                        size_t frameIndex = it->second.animation->getCurrentFrameIndex();
                        if (frameIndex < it->second.frames.size() && it->second.frames[frameIndex]) {
                            avatar = it->second.frames[frameIndex].get();
                        }
                    }
                }
            }
        }

        if (!avatar) {
            avatar = getAvatarImage(msg, layout.avatarSize);
        }

        if (avatar && avatar->w() > 0 && avatar->h() > 0) {
            avatar->draw(originX + layout.avatarX, originY + layout.avatarY);
        } else {
            fl_color(isPending ? ThemeColors::TEXT_MUTED : ThemeColors::TEXT_NORMAL);
            fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 20);
            std::string initial = layout.username.empty() ? "U" : std::string(1, layout.username[0]);
            int textW = static_cast<int>(fl_width(initial.c_str()));
            int textH = fl_height();
            fl_draw(initial.c_str(), originX + layout.avatarX + (layout.avatarSize - textW) / 2,
                    originY + layout.avatarY + (layout.avatarSize + textH) / 2 - fl_descent());
        }

        fl_color(isPending ? ThemeColors::TEXT_MUTED : ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kUsernameFontSize);
        fl_draw(layout.username.c_str(), originX + layout.usernameX, originY + layout.headerBaseline);

        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, kTimestampFontSize);
        fl_draw(layout.time.c_str(), originX + layout.timeX, originY + layout.timeBaseline);
    }

    if (layout.hasReply && !layout.replyItems.empty()) {
        fl_color(ThemeColors::BG_MODIFIER_ACCENT);
        fl_line_style(FL_SOLID, kReplyLineThickness);
        int lineX = originX + layout.replyLineX;
        int lineTop = originY + layout.replyLineTop + kReplyLineYOffset;
        int baselineY = originY + layout.replyBaseline + kReplyLineYOffset;
        int lineY = baselineY - kReplyLineBaselineOffset;
        int radius = kReplyLineCornerRadius;
        int lineBottom = originY + layout.avatarY - 2;
        if (lineBottom < lineY + radius) {
            lineBottom = lineY + radius;
        }

        int verticalStart = lineY + radius;
        if (verticalStart < lineTop) {
            verticalStart = lineTop;
        }
        if (lineBottom > verticalStart) {
            fl_line(lineX, verticalStart, lineX, lineBottom);
        }

        int hookStartX = lineX + radius;
        int hookEndX = originX + layout.replyX - kReplyLineTextGap;
        if (hookEndX > hookStartX) {
            fl_line(hookStartX, lineY, hookEndX, lineY);
        }

        if (lineY + radius >= lineTop) {
            int cx = lineX + radius;
            int cy = lineY + radius;
            int prevX = lineX;
            int prevY = lineY + radius;
            for (int i = 1; i <= kReplyLineCornerSegments; ++i) {
                double t = static_cast<double>(i) / static_cast<double>(kReplyLineCornerSegments);
                double angle = kReplyHalfPi * t;
                int px = cx - static_cast<int>(std::round(static_cast<double>(radius) * std::cos(angle)));
                int py = cy - static_cast<int>(std::round(static_cast<double>(radius) * std::sin(angle)));
                fl_line(prevX, prevY, px, py);
                prevX = px;
                prevY = py;
            }
        }

        fl_line_style(0);

        int replyY = originY + layout.replyBaseline;
        int cursorX = originX + layout.replyX;
        for (const auto &item : layout.replyItems) {
            cursorX += drawInlineItem(item, cursorX, replyY, isPending);
        }
    }

    int lineY = originY + layout.contentBaseline;
    for (const auto &line : layout.lines) {
        int cursorX = originX + layout.contentX;
        for (const auto &item : line.items) {
            cursorX += drawInlineItem(item, cursorX, lineY, isPending);
        }
        lineY += layout.lineHeight + layout.lineSpacing;
    }

    if (!layout.isSystem && !layout.attachments.empty() && !msg.attachments.empty()) {
        int attachmentsTop = originY + layout.contentTop + layout.contentHeight + layout.attachmentsTopPadding;
        size_t count = std::min(layout.attachments.size(), msg.attachments.size());

        for (size_t i = 0; i < count; ++i) {
            const auto &attachment = msg.attachments[i];
            const auto &attachmentLayout = layout.attachments[i];
            int boxW = attachmentLayout.width;
            int boxH = attachmentLayout.height;
            int boxX = originX + layout.contentX + attachmentLayout.xOffset;
            int boxY = attachmentsTop + attachmentLayout.yOffset;

            if (attachmentLayout.isImage) {
                Fl_RGB_Image *image = getAttachmentImage(attachment, boxW, boxH, attachmentLayout.squareCrop);
                if (image && image->w() > 0 && image->h() > 0) {
                    image->draw(boxX, boxY);
                } else {
                    RoundedStyle::drawRoundedRect(boxX, boxY, boxW, boxH, kAttachmentCornerRadius,
                                                  kAttachmentCornerRadius, kAttachmentCornerRadius,
                                                  kAttachmentCornerRadius, ThemeColors::BG_SECONDARY);
                }
            } else {
                RoundedStyle::drawRoundedRect(boxX, boxY, boxW, boxH, kAttachmentCornerRadius, kAttachmentCornerRadius,
                                              kAttachmentCornerRadius, kAttachmentCornerRadius,
                                              ThemeColors::BG_TERTIARY);
                RoundedStyle::drawRoundedOutline(boxX, boxY, boxW, boxH, kAttachmentCornerRadius,
                                                 kAttachmentCornerRadius, kAttachmentCornerRadius,
                                                 kAttachmentCornerRadius, ThemeColors::BG_MODIFIER_ACCENT);

                int iconPadY = std::max(0, (boxH - kAttachmentIconSize) / 2);
                int iconLeftPad = iconPadY + 4;
                int iconGap = 6;
                int iconX = boxX + iconLeftPad;
                int iconY = boxY + iconPadY;
                auto *icon = IconManager::load_icon("attachment", kAttachmentIconSize);
                if (icon && icon->w() > 0 && icon->h() > 0) {
                    icon->draw(iconX, iconY);
                }

                int textX = iconX + kAttachmentIconSize + iconGap;

                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentNameFontSize);
                int nameHeight = fl_height();
                int nameAscent = nameHeight - fl_descent();

                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentMetaFontSize);
                int metaHeight = fl_height();
                int metaAscent = metaHeight - fl_descent();

                int lineGap = 4;
                int totalTextHeight = nameHeight + lineGap + metaHeight;
                int textStartY = boxY + (boxH - totalTextHeight) / 2;

                fl_color(ThemeColors::TEXT_LINK);
                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentNameFontSize);
                int nameBaseline = textStartY + nameAscent;
                int downloadReserve =
                    (attachmentLayout.downloadSize > 0) ? (attachmentLayout.downloadSize + kAttachmentDownloadPadding) : 0;
                int textMaxWidth = boxX + boxW - iconPadY - downloadReserve - textX;
                std::string filename =
                    ellipsizeText(attachment.filename, std::max(0, textMaxWidth), FontLoader::Fonts::INTER_SEMIBOLD,
                                  kAttachmentNameFontSize);
                int filenameWidth = filename.empty() ? 0 : static_cast<int>(fl_width(filename.c_str()));
                std::string meta = formatFileSize(attachment.size);

                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentMetaFontSize);
                int metaWidth = static_cast<int>(fl_width(meta.c_str()));
                int textBlockWidth = std::max(filenameWidth, metaWidth);
                int textStartX = textX;
                if (textMaxWidth > textBlockWidth) {
                    int centerOffset = (textMaxWidth - textBlockWidth) / 2;
                    textStartX = textX + std::min(centerOffset, iconGap);
                }

                fl_color(ThemeColors::TEXT_LINK);
                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentNameFontSize);
                fl_draw(filename.c_str(), textStartX, nameBaseline + 1);

                fl_color(ThemeColors::TEXT_MUTED);
                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kAttachmentMetaFontSize);
                int metaBaseline = nameBaseline + lineGap + metaAscent;
                fl_draw(meta.c_str(), textStartX, metaBaseline);

                if (attachmentLayout.downloadSize > 0) {
                    int downloadX = boxX + attachmentLayout.downloadXOffset;
                    int downloadY = boxY + attachmentLayout.downloadYOffset;
                    std::string downloadKey = getAttachmentDownloadKey(msg, i);
                    bool downloadHovered = (downloadKey == hovered_attachment_download_key);

                    if (downloadHovered) {
                        RoundedStyle::drawRoundedRect(downloadX, downloadY, attachmentLayout.downloadSize,
                                                      attachmentLayout.downloadSize, kAttachmentDownloadButtonRadius,
                                                      kAttachmentDownloadButtonRadius, kAttachmentDownloadButtonRadius,
                                                      kAttachmentDownloadButtonRadius, ThemeColors::BG_ACCENT);
                    }

                    Fl_Color downloadColor =
                        downloadHovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
                    auto *downloadIcon =
                        IconManager::load_recolored_icon("download", kAttachmentDownloadIconSize, downloadColor);
                    if (downloadIcon && downloadIcon->w() > 0 && downloadIcon->h() > 0) {
                        int iconDrawX = downloadX + (attachmentLayout.downloadSize - kAttachmentDownloadIconSize) / 2;
                        int iconDrawY = downloadY + (attachmentLayout.downloadSize - kAttachmentDownloadIconSize) / 2;
                        downloadIcon->draw(iconDrawX, iconDrawY);
                    }
                }
            }
        }
    }

    if (!layout.isSystem && !layout.reactions.empty()) {
        int reactionsTop =
            originY + layout.contentTop + layout.contentHeight + layout.attachmentsHeight + layout.reactionsTopPadding;

        for (const auto &reactionLayout : layout.reactions) {
            int boxX = originX + layout.contentX + reactionLayout.xOffset;
            int boxY = reactionsTop + reactionLayout.yOffset;
            Fl_Color bgColor = reactionLayout.me ? kReactionSelfBg : kReactionBg;

            RoundedStyle::drawRoundedRect(boxX, boxY, reactionLayout.width, reactionLayout.height,
                                          kReactionCornerRadius, kReactionCornerRadius, kReactionCornerRadius,
                                          kReactionCornerRadius, bgColor);

            if (reactionLayout.me) {
                RoundedStyle::drawRoundedOutline(boxX, boxY, reactionLayout.width, reactionLayout.height,
                                                 kReactionCornerRadius, kReactionCornerRadius, kReactionCornerRadius,
                                                 kReactionCornerRadius, kReactionSelfBorder);
            }

            int emojiX = boxX + kReactionPaddingX;
            int emojiY = boxY + (reactionLayout.height - reactionLayout.emojiSize) / 2;
            int textX = emojiX + reactionLayout.emojiWidth + kReactionEmojiGap;

            if (reactionLayout.isCustomEmoji && !reactionLayout.emojiUrl.empty()) {
                Fl_RGB_Image *emoji = getEmojiImage(reactionLayout.emojiUrl, reactionLayout.emojiSize);
                if (emoji && emoji->w() > 0 && emoji->h() > 0) {
                    emoji->draw(emojiX, emojiY);
                } else if (!reactionLayout.emojiName.empty()) {
                    fl_color(kReactionTextColor);
                    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionEmojiFontSize);
                    int emojiBaseline = boxY + (reactionLayout.height + fl_height()) / 2 - fl_descent();
                    fl_draw(reactionLayout.emojiName.c_str(), emojiX, emojiBaseline);
                }
            } else if (!reactionLayout.emojiName.empty()) {
                fl_color(kReactionTextColor);
                fl_font(FontLoader::Fonts::INTER_SEMIBOLD, kReactionEmojiFontSize);
                int emojiBaseline = boxY + (reactionLayout.height + fl_height()) / 2 - fl_descent();
                fl_draw(reactionLayout.emojiName.c_str(), emojiX, emojiBaseline);
            }

            if (!reactionLayout.countText.empty()) {
                fl_color(kReactionTextColor);
                fl_font(FontLoader::Fonts::INTER_BOLD, kReactionFontSize);
                int textBaseline = boxY + (reactionLayout.height + fl_height()) / 2 - fl_descent();
                fl_draw(reactionLayout.countText.c_str(), textX, textBaseline);
            }
        }
    }
}

std::string MessageWidget::getAnimatedAvatarKey(const Message &msg, int size) {
    std::string gifUrl = getAnimatedAvatarUrl(msg, size);
    if (gifUrl.empty()) {
        return "";
    }
    return buildAvatarCacheKey(gifUrl, size);
}

std::string MessageWidget::getAvatarCacheKey(const Message &msg, int size) {
    std::string url = getStaticAvatarUrl(msg, size);
    if (url.empty()) {
        return "";
    }
    return buildAvatarCacheKey(url, size);
}

std::string MessageWidget::getAttachmentDownloadKey(const Message &msg, size_t attachmentIndex) {
    return msg.id + ":" + std::to_string(attachmentIndex);
}

void MessageWidget::setHoveredAvatarKey(const std::string &key) {
    if (key == hovered_avatar_key) {
        return;
    }

    if (!hovered_avatar_key.empty()) {
        stopAvatarAnimation(hovered_avatar_key);
    }

    hovered_avatar_key = key;

    if (!hovered_avatar_key.empty()) {
        startAvatarAnimation(hovered_avatar_key);
    }
}

void MessageWidget::setHoveredAttachmentDownloadKey(const std::string &key) {
    hovered_attachment_download_key = key;
}

void MessageWidget::pruneAvatarCache(const std::unordered_set<std::string> &keepKeys) {
    for (auto it = avatar_cache.begin(); it != avatar_cache.end();) {
        if (keepKeys.find(it->first) == keepKeys.end()) {
            it = avatar_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void MessageWidget::pruneAnimatedAvatarCache(const std::unordered_set<std::string> &keepKeys) {
    for (auto it = avatar_gif_cache.begin(); it != avatar_gif_cache.end();) {
        if (keepKeys.find(it->first) == keepKeys.end()) {
            stopAvatarAnimation(it->first);
            it = avatar_gif_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void MessageWidget::pruneAttachmentCache(const std::unordered_set<std::string> &keepKeys) {
    for (auto it = attachment_cache.begin(); it != attachment_cache.end();) {
        if (keepKeys.find(it->first) == keepKeys.end()) {
            size_t split = it->first.find('#');
            if (split != std::string::npos) {
                Images::evictFromMemory(it->first.substr(0, split));
            }
            it = attachment_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void MessageWidget::pruneEmojiCache(const std::unordered_set<std::string> &keepKeys) {
    for (auto it = emoji_cache.begin(); it != emoji_cache.end();) {
        if (keepKeys.find(it->first) == keepKeys.end()) {
            size_t split = it->first.find('#');
            if (split != std::string::npos) {
                Images::evictFromMemory(it->first.substr(0, split));
            }
            it = emoji_cache.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<MessageWidget::InlineItem> MessageWidget::tokenizeText(const std::string &text, Fl_Font font, int size,
                                                                   Fl_Color color) {
    return tokenizeTextWithMessage(text, font, size, color, nullptr);
}

std::vector<MessageWidget::LayoutLine> MessageWidget::wrapText(const std::string &text, int maxWidth) {
    auto tokens = tokenizeText(text, FontLoader::Fonts::INTER_REGULAR, kContentFontSize, ThemeColors::TEXT_NORMAL);
    return wrapTokens(tokens, maxWidth);
}

std::vector<MessageWidget::LayoutLine> MessageWidget::wrapTokens(const std::vector<InlineItem> &tokens, int maxWidth) {
    std::vector<LayoutLine> lines;
    LayoutLine currentLine;
    int currentWidth = 0;

    if (maxWidth <= 0) {
        lines.push_back(currentLine);
        return lines;
    }

    auto pushLine = [&]() {
        currentLine.width = currentWidth;
        lines.push_back(currentLine);
        currentLine = LayoutLine{};
        currentWidth = 0;
    };

    auto trimLeadingWhitespace = [](const std::string &str) -> std::string {
        size_t start = 0;
        while (start < str.size() && (str[start] == ' ' || str[start] == '\t')) {
            start++;
        }
        return str.substr(start);
    };

    for (auto token : tokens) {
        if (token.kind == InlineItem::Kind::LineBreak) {
            pushLine();
            continue;
        }

        if (token.kind == InlineItem::Kind::Emoji) {
            if (token.width <= 0) {
                token.width = token.emojiSize > 0 ? token.emojiSize : kEmojiSize;
            }

            if (currentWidth + token.width > maxWidth && !currentLine.items.empty()) {
                pushLine();
            }

            currentLine.items.push_back(token);
            currentWidth += token.width;
            continue;
        }

        if (token.kind == InlineItem::Kind::Text) {
            if (currentLine.items.empty() && !token.text.empty()) {
                std::string trimmed = trimLeadingWhitespace(token.text);
                if (trimmed.empty()) {
                    continue;
                }
                if (trimmed != token.text) {
                    token.text = trimmed;
                    fl_font(token.font, token.size);
                    token.width = static_cast<int>(fl_width(token.text.c_str()));
                }
            }
        }

        fl_font(token.font, token.size);
        token.width = static_cast<int>(fl_width(token.text.c_str()));
        if (token.width > maxWidth && token.text != " ") {
            auto splitTokens = splitLongToken(token, maxWidth);
            for (auto part : splitTokens) {
                if (currentWidth + part.width > maxWidth && !currentLine.items.empty()) {
                    pushLine();
                }
                if (currentLine.items.empty() && part.kind == InlineItem::Kind::Text) {
                    std::string trimmed = trimLeadingWhitespace(part.text);
                    if (trimmed.empty()) {
                        continue;
                    }
                    if (trimmed != part.text) {
                        part.text = trimmed;
                        fl_font(part.font, part.size);
                        part.width = static_cast<int>(fl_width(part.text.c_str()));
                    }
                }
                currentLine.items.push_back(part);
                currentWidth += part.width;
            }
            continue;
        }

        if (token.text == " " && currentLine.items.empty()) {
            continue;
        }

        if (currentWidth + token.width > maxWidth && !currentLine.items.empty()) {
            pushLine();
            if (token.kind == InlineItem::Kind::Text) {
                std::string trimmed = trimLeadingWhitespace(token.text);
                if (trimmed.empty()) {
                    continue;
                }
                if (trimmed != token.text) {
                    token.text = trimmed;
                    fl_font(token.font, token.size);
                    token.width = static_cast<int>(fl_width(token.text.c_str()));
                }
            }
        }

        currentLine.items.push_back(token);
        currentWidth += token.width;
    }

    currentLine.width = currentWidth;
    lines.push_back(currentLine);
    return lines;
}

std::vector<MessageWidget::InlineItem> MessageWidget::splitLongToken(const InlineItem &token, int maxWidth) {
    std::vector<InlineItem> parts;
    if (token.text.empty() || maxWidth <= 0) {
        return parts;
    }

    std::string current;
    for (char ch : token.text) {
        std::string candidate = current;
        candidate.push_back(ch);
        fl_font(token.font, token.size);
        int candidateWidth = static_cast<int>(fl_width(candidate.c_str()));
        if (!current.empty() && candidateWidth > maxWidth) {
            InlineItem part;
            part.kind = InlineItem::Kind::Text;
            part.text = current;
            part.width = static_cast<int>(fl_width(current.c_str()));
            part.font = token.font;
            part.size = token.size;
            part.color = token.color;
            part.linkUrl = token.linkUrl;
            part.isLink = token.isLink;
            part.underline = token.underline;
            part.strikethrough = token.strikethrough;
            parts.push_back(std::move(part));
            current.clear();
            current.push_back(ch);
        } else {
            current = candidate;
        }
    }

    if (!current.empty()) {
        fl_font(token.font, token.size);
        InlineItem part;
        part.kind = InlineItem::Kind::Text;
        part.text = current;
        part.width = static_cast<int>(fl_width(current.c_str()));
        part.font = token.font;
        part.size = token.size;
        part.color = token.color;
        part.linkUrl = token.linkUrl;
        part.isLink = token.isLink;
        part.underline = token.underline;
        part.strikethrough = token.strikethrough;
        parts.push_back(std::move(part));
    }

    return parts;
}
