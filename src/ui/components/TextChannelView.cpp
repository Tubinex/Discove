#include "ui/components/TextChannelView.h"

#include "data/Database.h"
#include "net/APIClient.h"
#include "state/Store.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
#include "ui/components/MessageWidget.h"
#include "utils/Fonts.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"

#include <FL/Fl.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace {
constexpr int kInputBubbleMargin = 8;
constexpr int kInputBubbleRadius = 8;
constexpr int kInputPlusButtonSize = 36;
constexpr int kInputPlusButtonRadius = 6;
constexpr int kInputPlusIconSize = 18;
constexpr int kInputPlusButtonPadding = 12;
constexpr int kInputPlaceholderPadding = 12;
constexpr int kInputIconSize = 20;
constexpr int kInputIconButtonSize = 34;
constexpr int kInputIconButtonRadius = 6;
constexpr int kInputIconSpacing = 6;
constexpr int kInputIconRightPadding = 12;
constexpr int kEmojiAtlasCellSize = 48;
constexpr int kMessagesVerticalPadding = 16;
constexpr int kDateSeparatorPadding = 28;
constexpr int kDateSeparatorHeight = kDateSeparatorPadding * 2;
constexpr int kSystemMessageSpacing = 8;
constexpr int kMessageScrollStep = 32;
constexpr int kMessageRenderPadding = 200;

std::string trimSpaces(const std::string &text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

std::string collapseWhitespace(const std::string &text) {
    std::string result;
    result.reserve(text.size());
    bool inSpace = false;
    for (char ch : text) {
        if (ch == '\n' || ch == '\r' || ch == '\t' || ch == ' ') {
            if (!inSpace) {
                result.push_back(' ');
                inSpace = true;
            }
        } else {
            result.push_back(ch);
            inSpace = false;
        }
    }
    return trimSpaces(result);
}

std::string buildAttachmentSnippet(const Message &msg) {
    bool hasImage = false;
    bool hasVideo = false;
    for (const auto &attachment : msg.attachments) {
        if (attachment.isImage()) {
            hasImage = true;
        } else if (attachment.isVideo()) {
            hasVideo = true;
        }
    }

    if (hasImage) {
        return "Image";
    }
    if (hasVideo) {
        return "Video";
    }
    return "Attachment";
}

MessageWidget::ReplyPreview buildReplyPreview(const Message *referenced) {
    MessageWidget::ReplyPreview preview;
    if (!referenced) {
        preview.author = "Unknown";
        preview.content = "Original message unavailable";
        preview.unavailable = true;
        return preview;
    }

    preview.author = referenced->getAuthorDisplayName();

    std::string snippet = collapseWhitespace(referenced->content);
    if (snippet.empty()) {
        if (!referenced->attachments.empty()) {
            snippet = buildAttachmentSnippet(*referenced);
        }
    }
    preview.content = snippet;
    return preview;
}

std::unique_ptr<Fl_RGB_Image> copyEmojiCell(Fl_RGB_Image *atlas, int cellX, int cellY, int cellSize, int targetSize) {
    if (!atlas || cellSize <= 0 || targetSize <= 0) {
        return nullptr;
    }

    if (cellX + cellSize > atlas->w() || cellY + cellSize > atlas->h()) {
        return nullptr;
    }

    int depth = atlas->d();
    if (depth < 3) {
        return nullptr;
    }

    const unsigned char *srcData = reinterpret_cast<const unsigned char *>(atlas->data()[0]);
    if (!srcData) {
        return nullptr;
    }

    int srcLineSize = atlas->ld() ? atlas->ld() : atlas->w() * depth;
    std::vector<unsigned char> buffer(cellSize * cellSize * depth);

    for (int y = 0; y < cellSize; ++y) {
        const unsigned char *srcRow = srcData + (cellY + y) * srcLineSize + cellX * depth;
        unsigned char *dstRow = buffer.data() + y * cellSize * depth;
        std::memcpy(dstRow, srcRow, cellSize * depth);
    }

    if (depth == 4) {
        bool hasAlpha = false;
        for (size_t i = 3; i < buffer.size(); i += 4) {
            if (buffer[i] != 0) {
                hasAlpha = true;
                break;
            }
        }
        if (!hasAlpha) {
            return nullptr;
        }
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    auto *cellImage = new Fl_RGB_Image(heapData, cellSize, cellSize, depth);
    cellImage->alloc_array = 1;

    Fl_Image *scaled = cellImage->copy(targetSize, targetSize);
    delete cellImage;

    if (!scaled) {
        return nullptr;
    }

    return std::unique_ptr<Fl_RGB_Image>(static_cast<Fl_RGB_Image *>(scaled));
}

std::vector<std::unique_ptr<Fl_RGB_Image>> loadEmojiAtlasFrames(const std::string &path, int targetSize) {
    std::vector<std::unique_ptr<Fl_RGB_Image>> frames;
    if (targetSize <= 0) {
        return frames;
    }

    Fl_PNG_Image atlas(path.c_str());
    if (atlas.w() <= 0 || atlas.h() <= 0) {
        return frames;
    }

    int cellSize = kEmojiAtlasCellSize;
    if (atlas.w() < cellSize || atlas.h() < cellSize) {
        return frames;
    }

    int cols = atlas.w() / cellSize;
    int rows = atlas.h() / cellSize;
    frames.reserve(cols * rows);

    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            auto frame = copyEmojiCell(&atlas, col * cellSize, row * cellSize, cellSize, targetSize);
            if (frame) {
                frames.push_back(std::move(frame));
            }
        }
    }

    return frames;
}

std::unique_ptr<Fl_RGB_Image> tintEmojiFrame(Fl_RGB_Image *source, Fl_Color color) {
    if (!source || source->w() <= 0 || source->h() <= 0) {
        return nullptr;
    }

    int depth = source->d();
    if (depth < 3) {
        return nullptr;
    }

    int width = source->w();
    int height = source->h();
    int srcLineSize = source->ld() ? source->ld() : width * depth;

    const unsigned char *src = reinterpret_cast<const unsigned char *>(source->data()[0]);
    if (!src) {
        return nullptr;
    }

    unsigned char tintR = ThemeColors::red(color);
    unsigned char tintG = ThemeColors::green(color);
    unsigned char tintB = ThemeColors::blue(color);

    std::vector<unsigned char> buffer(width * height * depth);
    for (int y = 0; y < height; ++y) {
        const unsigned char *srcRow = src + y * srcLineSize;
        unsigned char *dstRow = buffer.data() + y * width * depth;
        for (int x = 0; x < width; ++x) {
            int idx = x * depth;
            unsigned char srcR = srcRow[idx + 0];
            unsigned char srcG = srcRow[idx + 1];
            unsigned char srcB = srcRow[idx + 2];
            unsigned char srcA = (depth == 4) ? srcRow[idx + 3] : 255;
            bool hasInk = (depth == 4) ? (srcA > 0) : (srcR || srcG || srcB);

            if (hasInk) {
                dstRow[idx + 0] = tintR;
                dstRow[idx + 1] = tintG;
                dstRow[idx + 2] = tintB;
            } else {
                dstRow[idx + 0] = srcR;
                dstRow[idx + 1] = srcG;
                dstRow[idx + 2] = srcB;
            }

            if (depth == 4) {
                dstRow[idx + 3] = srcA;
            }
        }
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    auto *tinted = new Fl_RGB_Image(heapData, width, height, depth);
    tinted->alloc_array = 1;

    return std::unique_ptr<Fl_RGB_Image>(tinted);
}

void tintEmojiFrames(std::vector<std::unique_ptr<Fl_RGB_Image>> &frames, Fl_Color color) {
    for (auto &frame : frames) {
        if (!frame) {
            continue;
        }
        auto tinted = tintEmojiFrame(frame.get(), color);
        if (tinted) {
            frame = std::move(tinted);
        }
    }
}
} // namespace

TextChannelView::TextChannelView(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    clip_children(1);
    end();

    m_storeListenerId = Store::get().subscribe([this](const AppState &state) {
        if (m_isDestroying || m_channelId.empty()) {
            return;
        }

        auto it = state.channelMessages.find(m_channelId);
        if (it != state.channelMessages.end()) {
            m_messages = it->second;
            redraw();
        } else {
            m_messages.clear();
            redraw();
        }
    });
}

TextChannelView::~TextChannelView() {
    m_isDestroying = true;

    if (m_storeListenerId) {
        Store::get().unsubscribe(m_storeListenerId);
        m_storeListenerId = 0;
    }
}

void TextChannelView::draw() {
    if (m_isDestroying) {
        return;
    }

    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), y(), w(), h());

    drawHeader();
    int contentY = y() + HEADER_HEIGHT;
    int inputAreaHeight = m_canSendMessages ? MESSAGE_INPUT_HEIGHT : 0;
    int contentH = h() - HEADER_HEIGHT - inputAreaHeight;

    fl_push_clip(x(), contentY, w(), contentH);

    if (m_welcomeVisible && m_messages.empty()) {
        m_messagesScrollOffset = 0;
        m_messagesContentHeight = 0;
        m_messagesViewHeight = 0;
        m_avatarHitboxes.clear();
        updateAvatarHover(0, 0, true);
        drawWelcomeSection();
    } else {
        drawMessages();
    }

    fl_pop_clip();
    fl_pop_clip();

    if (m_canSendMessages) {
        drawMessageInput();
    }
}

int TextChannelView::handle(int event) {
    int handled = Fl_Group::handle(event);

    switch (event) {
    case FL_MOUSEWHEEL: {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        int inputAreaHeight = m_canSendMessages ? MESSAGE_INPUT_HEIGHT : 0;
        int contentY = y() + HEADER_HEIGHT;
        int contentH = h() - HEADER_HEIGHT - inputAreaHeight;

        if (contentH <= 0 || mx < x() || mx >= x() + w() || my < contentY || my >= contentY + contentH) {
            break;
        }

        int maxScroll = std::max(0, m_messagesContentHeight - m_messagesViewHeight);
        if (maxScroll <= 0) {
            return 1;
        }

        int dy = Fl::event_dy();
        if (dy == 0) {
            return 1;
        }

        int newOffset = std::clamp(m_messagesScrollOffset - dy * kMessageScrollStep, 0, maxScroll);
        if (newOffset != m_messagesScrollOffset) {
            m_messagesScrollOffset = newOffset;
            m_avatarHitboxes.clear();
            updateAvatarHover(0, 0, true);
            redraw();
        }
        return 1;
    }
    case FL_ENTER:
    case FL_MOVE:
    case FL_LEAVE: {
        int hoveredIndex = -1;
        bool plusHovered = false;
        int mx = 0;
        int my = 0;
        if (event != FL_LEAVE) {
            mx = Fl::event_x();
            my = Fl::event_y();
            int plusX = 0;
            int plusY = 0;
            int plusSize = 0;
            if (getPlusButtonRect(plusX, plusY, plusSize)) {
                plusHovered = (mx >= plusX && mx < plusX + plusSize && my >= plusY && my < plusY + plusSize);
            }

            for (int i = 0; i < 3; ++i) {
                int iconX = 0;
                int iconY = 0;
                int iconSize = 0;
                if (getInputButtonRect(i, iconX, iconY, iconSize)) {
                    if (mx >= iconX && mx < iconX + iconSize && my >= iconY && my < iconY + iconSize) {
                        hoveredIndex = i;
                        break;
                    }
                }
            }
        }

        bool hoverChanged = false;
        if (plusHovered != m_plusHovered) {
            m_plusHovered = plusHovered;
            hoverChanged = true;
        }

        if (hoveredIndex != m_hoveredInputButton) {
            bool wasEmojiHovered = (m_hoveredInputButton == 0);
            m_hoveredInputButton = hoveredIndex;
            if (!wasEmojiHovered && m_hoveredInputButton == 0) {
                ensureEmojiAtlases(kInputIconSize);
                if (!m_emojiFramesInactive.empty() && !m_emojiFramesActive.empty()) {
                    m_emojiFrameIndex = (m_emojiFrameIndex + 1) % m_emojiFramesInactive.size();
                }
            }
            hoverChanged = true;
        }

        if (updateAvatarHover(mx, my, event == FL_LEAVE)) {
            hoverChanged = true;
        }

        if (hoverChanged) {
            redraw();
            handled = 1;
        }
        break;
    }
    default:
        break;
    }

    return handled;
}

void TextChannelView::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);
    redraw();
}

void TextChannelView::setChannel(const std::string &channelId, const std::string &channelName,
                                 const std::string &guildId, bool isWelcomeVisible) {
    m_channelId = channelId;
    m_channelName = channelName;
    m_guildId = guildId;
    m_welcomeVisible = isWelcomeVisible;
    m_messagesScrollOffset = 0;
    m_avatarHitboxes.clear();
    m_hoveredAvatarMessageId.clear();
    if (!m_hoveredAvatarKey.empty()) {
        m_hoveredAvatarKey.clear();
        MessageWidget::setHoveredAvatarKey("");
    }
    loadMessages();
    updatePermissions();
    redraw();
}

void TextChannelView::drawHeader() {
    int headerY = y();
    int headerH = HEADER_HEIGHT;

    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), headerY, w(), headerH);

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), headerY + headerH - 1, x() + w(), headerY + headerH - 1);

    const int iconSize = 24;
    const int iconMargin = 16;
    auto *icon = IconManager::load_recolored_icon("channel_text", iconSize, ThemeColors::TEXT_MUTED);
    if (icon) {
        int iconX = x() + iconMargin;
        int iconY = headerY + (headerH - iconSize) / 2;
        icon->draw(iconX, iconY);
    }

    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 16);
    int textX = x() + iconMargin + iconSize + 8;
    int textY = headerY + headerH / 2 + 6;
    fl_draw(m_channelName.c_str(), textX, textY);
}

void TextChannelView::drawWelcomeSection() {
    int contentY = y() + HEADER_HEIGHT;
    int contentH = h() - HEADER_HEIGHT - MESSAGE_INPUT_HEIGHT;

    const int circleSize = 68;
    const int circleY = contentY + 80;
    int circleX = x() + 40;

    fl_color(ThemeColors::BG_SECONDARY);
    fl_pie(circleX, circleY, circleSize, circleSize, 0, 360);

    const int hashSize = 40;
    auto *hashIcon = IconManager::load_recolored_icon("channel_text", hashSize, ThemeColors::TEXT_MUTED);
    if (hashIcon) {
        int hashX = circleX + (circleSize - hashSize) / 2;
        int hashY = circleY + (circleSize - hashSize) / 2;
        hashIcon->draw(hashX, hashY);
    }

    std::string welcomeTitle = "Welcome to #" + m_channelName + "!";
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_BOLD, 32);
    int titleY = circleY + circleSize + 38;
    fl_draw(welcomeTitle.c_str(), x() + 40, titleY);

    std::string description = "This is the start of the #" + m_channelName + " channel.";
    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FontLoader::Fonts::INTER_REGULAR, 16);
    int descY = titleY + 32;
    fl_draw(description.c_str(), x() + 40, descY);
}

void TextChannelView::drawMessages() {
    int inputAreaHeight = m_canSendMessages ? MESSAGE_INPUT_HEIGHT : 0;
    int contentY = y() + HEADER_HEIGHT;
    int contentH = h() - HEADER_HEIGHT - inputAreaHeight;
    if (contentH <= 0) {
        return;
    }

    int viewTop = contentY + kMessagesVerticalPadding;
    int viewBottom = contentY + contentH - kMessagesVerticalPadding;
    if (viewBottom < viewTop) {
        viewBottom = viewTop;
    }
    m_messagesViewHeight = std::max(0, viewBottom - viewTop);
    m_avatarHitboxes.clear();
    std::vector<const Message *> ordered;
    ordered.reserve(m_messages.size());
    std::unordered_map<std::string, const Message *> messageById;
    messageById.reserve(m_messages.size());
    for (const auto &msg : m_messages) {
        ordered.push_back(&msg);
        messageById[msg.id] = &msg;
    }

    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Message *a, const Message *b) { return a->timestamp < b->timestamp; });

    struct RenderEntry {
        enum class Type { Separator, Message };
        Type type;
        std::string date;
        const Message *msg = nullptr;
        MessageWidget::Layout layout;
        bool grouped = false;
    };

    std::vector<RenderEntry> entries;
    entries.reserve(ordered.size() * 2);

    struct MessageInfo {
        const Message *msg = nullptr;
        std::string date;
        bool grouped = false;
        bool needsSeparator = false;
    };

    std::vector<MessageInfo> messageInfos;
    messageInfos.reserve(ordered.size());

    std::string previousAuthorId;
    std::chrono::system_clock::time_point previousTimestamp;
    bool hasPrevious = false;
    bool previousWasSystem = false;
    std::string previousDate;

    for (const auto *msg : ordered) {
        auto msgTime = std::chrono::system_clock::to_time_t(msg->timestamp);
        std::tm tmStruct;
        localtime_s(&tmStruct, &msgTime);

        std::ostringstream dateStream;
        dateStream << std::put_time(&tmStruct, "%B %d, %Y");
        std::string currentDate = dateStream.str();

        bool needsSeparator = (currentDate != previousDate);

        bool isSystem = msg->isSystemMessage();
        bool isGrouped = false;
        if (!isSystem && hasPrevious && !previousWasSystem && !needsSeparator && !msg->isReply() &&
            msg->authorId == previousAuthorId) {
            auto timeDiff = std::chrono::duration_cast<std::chrono::minutes>(msg->timestamp - previousTimestamp);
            if (timeDiff.count() < 5) {
                isGrouped = true;
            }
        }

        messageInfos.push_back({msg, currentDate, isGrouped, needsSeparator});

        if (!isSystem) {
            previousAuthorId = msg->authorId;
            previousTimestamp = msg->timestamp;
            hasPrevious = true;
        } else {
            previousAuthorId.clear();
            hasPrevious = true;
        }
        previousWasSystem = isSystem;
        previousDate = currentDate;
    }

    int totalHeight = 0;
    bool previousEntryWasSeparator = false;
    previousWasSystem = false;

    for (size_t i = 0; i < messageInfos.size(); ++i) {
        const auto &info = messageInfos[i];
        if (info.needsSeparator) {
            entries.push_back({RenderEntry::Type::Separator, info.date});
            totalHeight += kDateSeparatorHeight;
            previousEntryWasSeparator = true;
        }

        if (!info.msg) {
            continue;
        }

        bool isSystem = info.msg->isSystemMessage();
        bool compactBottom = (i + 1 < messageInfos.size()) ? messageInfos[i + 1].grouped : false;

        int spacing = 0;
        if (!previousEntryWasSeparator) {
            if (info.grouped) {
                spacing = 0;
            } else if (isSystem && previousWasSystem) {
                spacing = kSystemMessageSpacing;
            } else {
                spacing = MESSAGE_GROUP_SPACING;
            }
        }
        totalHeight += spacing;

        MessageWidget::ReplyPreview replyPreview;
        MessageWidget::ReplyPreview *replyPtr = nullptr;
        if (info.msg->isReply() && info.msg->referencedMessageId.has_value()) {
            auto refIt = messageById.find(*info.msg->referencedMessageId);
            const Message *referenced = (refIt != messageById.end()) ? refIt->second : nullptr;
            replyPreview = buildReplyPreview(referenced);
            replyPtr = &replyPreview;
        }

        MessageWidget::Layout layout = MessageWidget::buildLayout(*info.msg, w(), info.grouped, compactBottom, replyPtr);
        totalHeight += layout.height;
        entries.push_back({RenderEntry::Type::Message, "", info.msg, layout, info.grouped});

        previousWasSystem = isSystem;
        previousEntryWasSeparator = false;
    }

    m_messagesContentHeight = totalHeight;
    int maxScroll = std::max(0, totalHeight - m_messagesViewHeight);
    if (m_messagesScrollOffset > maxScroll) {
        m_messagesScrollOffset = maxScroll;
    } else if (m_messagesScrollOffset < 0) {
        m_messagesScrollOffset = 0;
    }

    int yPos = viewBottom - totalHeight + m_messagesScrollOffset;
    int renderTop = viewTop - kMessageRenderPadding;
    int renderBottom = viewBottom + kMessageRenderPadding;

    std::unordered_set<std::string> keepAvatarKeys;
    std::unordered_set<std::string> keepAnimatedAvatarKeys;
    std::unordered_set<std::string> keepAttachmentKeys;

    bool drawingAfterSeparator = false;
    previousWasSystem = false;
    for (const auto &entry : entries) {
        if (entry.type == RenderEntry::Type::Separator) {
            int separatorTop = yPos;
            int separatorBottom = yPos + kDateSeparatorHeight;
            if (separatorBottom >= renderTop && separatorTop <= renderBottom) {
                drawDateSeparator(entry.date, yPos);
            } else {
                yPos += kDateSeparatorHeight;
            }
            drawingAfterSeparator = true;
            continue;
        }

        int spacing = 0;
        if (!drawingAfterSeparator) {
            if (entry.grouped) {
                spacing = 0;
            } else if (entry.msg && entry.msg->isSystemMessage() && previousWasSystem) {
                spacing = kSystemMessageSpacing;
            } else {
                spacing = MESSAGE_GROUP_SPACING;
            }
        }
        int entryTop = yPos + spacing;
        int entryBottom = entryTop + entry.layout.height;
        bool visible = entryBottom >= renderTop && entryTop <= renderBottom;

        yPos += spacing;
        if (visible && entry.msg) {
            bool avatarHovered = false;
            if (!entry.grouped && !entry.msg->isSystemMessage()) {
                AvatarHitbox hitbox;
                hitbox.x = x() + entry.layout.avatarX;
                hitbox.y = yPos + entry.layout.avatarY;
                hitbox.size = entry.layout.avatarSize;
                hitbox.messageId = entry.msg->id;
                hitbox.hoverKey = MessageWidget::getAnimatedAvatarKey(*entry.msg, entry.layout.avatarSize);
                m_avatarHitboxes.push_back(std::move(hitbox));
                avatarHovered = (entry.msg->id == m_hoveredAvatarMessageId);

                std::string avatarKey = MessageWidget::getAvatarCacheKey(*entry.msg, entry.layout.avatarSize);
                if (!avatarKey.empty()) {
                    keepAvatarKeys.insert(avatarKey);
                }
                if (!hitbox.hoverKey.empty()) {
                    keepAnimatedAvatarKeys.insert(hitbox.hoverKey);
                }
            }

            for (const auto &attachmentLayout : entry.layout.attachments) {
                if (!attachmentLayout.cacheKey.empty()) {
                    keepAttachmentKeys.insert(attachmentLayout.cacheKey);
                }
            }

            MessageWidget::draw(*entry.msg, entry.layout, x(), yPos, avatarHovered);
        }
        yPos += entry.layout.height;
        previousWasSystem = entry.msg && entry.msg->isSystemMessage();
        drawingAfterSeparator = false;
    }

    MessageWidget::pruneAvatarCache(keepAvatarKeys);
    MessageWidget::pruneAnimatedAvatarCache(keepAnimatedAvatarKeys);
    MessageWidget::pruneAttachmentCache(keepAttachmentKeys);

    if (!m_hoveredAvatarMessageId.empty()) {
        bool stillVisible = std::any_of(m_avatarHitboxes.begin(), m_avatarHitboxes.end(),
                                        [this](const AvatarHitbox &hitbox) {
                                            return hitbox.messageId == m_hoveredAvatarMessageId;
                                        });
        if (!stillVisible) {
            m_hoveredAvatarMessageId.clear();
            if (!m_hoveredAvatarKey.empty()) {
                m_hoveredAvatarKey.clear();
                MessageWidget::setHoveredAvatarKey("");
            }
        }
    }
}

void TextChannelView::drawDateSeparator(const std::string &date, int &yPos) {
    yPos += kDateSeparatorPadding;

    const int lineMargin = 16;
    const int textPadding = 8;

    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 12);
    int textWidth = static_cast<int>(fl_width(date.c_str()));
    int textX = x() + w() / 2 - textWidth / 2;
    int textY = yPos + 6;

    fl_color(ThemeColors::BG_MODIFIER_ACCENT);
    fl_line(x() + lineMargin, yPos, textX - textPadding, yPos);

    fl_color(ThemeColors::TEXT_MUTED);
    fl_draw(date.c_str(), textX, textY);

    fl_color(ThemeColors::BG_MODIFIER_ACCENT);
    fl_line(textX + textWidth + textPadding, yPos, x() + w() - lineMargin, yPos);

    yPos += kDateSeparatorPadding;
}

void TextChannelView::drawMessageInput() {
    int inputY = y() + h() - MESSAGE_INPUT_HEIGHT;
    int inputH = MESSAGE_INPUT_HEIGHT;

    int bubbleX = x() + kInputBubbleMargin;
    int bubbleY = inputY + kInputBubbleMargin;
    int bubbleW = w() - (kInputBubbleMargin * 2);
    int bubbleH = inputH - (kInputBubbleMargin * 2);

    fl_color(ThemeColors::BG_TERTIARY);

    fl_pie(bubbleX, bubbleY, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 90, 180);
    fl_pie(bubbleX + bubbleW - kInputBubbleRadius * 2, bubbleY, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 0, 90);
    fl_pie(bubbleX, bubbleY + bubbleH - kInputBubbleRadius * 2, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 180,
           270);
    fl_pie(bubbleX + bubbleW - kInputBubbleRadius * 2, bubbleY + bubbleH - kInputBubbleRadius * 2,
           kInputBubbleRadius * 2, kInputBubbleRadius * 2, 270, 360);

    fl_rectf(bubbleX + kInputBubbleRadius, bubbleY, bubbleW - kInputBubbleRadius * 2, bubbleH);
    fl_rectf(bubbleX, bubbleY + kInputBubbleRadius, bubbleW, bubbleH - kInputBubbleRadius * 2);

    fl_color(ThemeColors::BORDER_PRIMARY);
    fl_line_style(FL_SOLID, 1);

    fl_arc(bubbleX, bubbleY, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 90, 180);
    fl_arc(bubbleX + bubbleW - kInputBubbleRadius * 2, bubbleY, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 0, 90);
    fl_arc(bubbleX, bubbleY + bubbleH - kInputBubbleRadius * 2, kInputBubbleRadius * 2, kInputBubbleRadius * 2, 180,
           270);
    fl_arc(bubbleX + bubbleW - kInputBubbleRadius * 2, bubbleY + bubbleH - kInputBubbleRadius * 2,
           kInputBubbleRadius * 2, kInputBubbleRadius * 2, 270, 360);

    fl_line(bubbleX + kInputBubbleRadius, bubbleY, bubbleX + bubbleW - kInputBubbleRadius, bubbleY);
    fl_line(bubbleX + bubbleW, bubbleY + kInputBubbleRadius, bubbleX + bubbleW, bubbleY + bubbleH - kInputBubbleRadius);
    fl_line(bubbleX + kInputBubbleRadius, bubbleY + bubbleH, bubbleX + bubbleW - kInputBubbleRadius, bubbleY + bubbleH);
    fl_line(bubbleX, bubbleY + kInputBubbleRadius, bubbleX, bubbleY + bubbleH - kInputBubbleRadius);

    fl_line_style(0);

    int buttonX = bubbleX + kInputPlusButtonPadding;
    int buttonY = bubbleY + (bubbleH - kInputPlusButtonSize) / 2;

    if (m_plusHovered) {
        RoundedStyle::drawRoundedRect(buttonX, buttonY, kInputPlusButtonSize, kInputPlusButtonSize,
                                      kInputPlusButtonRadius, kInputPlusButtonRadius, kInputPlusButtonRadius,
                                      kInputPlusButtonRadius, ThemeColors::BG_ACCENT);
    }

    Fl_Color plusColor = m_plusHovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    auto *plusIcon = IconManager::load_recolored_icon("plus", kInputPlusIconSize, plusColor);
    if (plusIcon) {
        int plusX = buttonX + (kInputPlusButtonSize - kInputPlusIconSize) / 2;
        int plusY = buttonY + (kInputPlusButtonSize - kInputPlusIconSize) / 2;
        plusIcon->draw(plusX, plusY);
    }

    std::string placeholder = "Message #" + m_channelName;
    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FontLoader::Fonts::INTER_REGULAR, 15);
    int placeholderX = buttonX + kInputPlusButtonSize + kInputPlaceholderPadding;
    int placeholderY = bubbleY + bubbleH / 2 + 5;
    fl_draw(placeholder.c_str(), placeholderX, placeholderY);

    const int iconSize = kInputIconSize;
    const int iconButtonSize = kInputIconButtonSize;
    int iconButtonX = bubbleX + bubbleW - iconButtonSize - kInputIconRightPadding;
    int iconButtonY = bubbleY + (bubbleH - iconButtonSize) / 2;
    bool emojiHovered = (m_hoveredInputButton == 0);
    bool stickerHovered = (m_hoveredInputButton == 1);
    bool gifHovered = (m_hoveredInputButton == 2);

    ensureEmojiAtlases(iconSize);
    if (emojiHovered) {
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize, kInputIconButtonRadius,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      ThemeColors::BG_ACCENT);
    }
    if (!m_emojiFramesInactive.empty() && !m_emojiFramesActive.empty()) {
        size_t frameIndex = std::min(m_emojiFrameIndex, m_emojiFramesInactive.size() - 1);
        const auto &frames = emojiHovered ? m_emojiFramesActive : m_emojiFramesInactive;
        if (frames[frameIndex]) {
            int iconX = iconButtonX + (iconButtonSize - iconSize) / 2;
            int iconY = iconButtonY + (iconButtonSize - iconSize) / 2;
            frames[frameIndex]->draw(iconX, iconY);
        }
    }
    iconButtonX -= iconButtonSize + kInputIconSpacing;

    if (stickerHovered) {
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize, kInputIconButtonRadius,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      ThemeColors::BG_ACCENT);
    }
    Fl_Color stickerColor = stickerHovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    auto *stickerIcon = IconManager::load_recolored_icon("sticker", iconSize, stickerColor);
    if (stickerIcon) {
        int iconX = iconButtonX + (iconButtonSize - iconSize) / 2;
        int iconY = iconButtonY + (iconButtonSize - iconSize) / 2;
        stickerIcon->draw(iconX, iconY);
    }
    iconButtonX -= iconButtonSize + kInputIconSpacing;

    if (gifHovered) {
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize, kInputIconButtonRadius,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      ThemeColors::BG_ACCENT);
    }
    Fl_Color gifColor = gifHovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    auto *gifIcon = IconManager::load_recolored_icon("gif", iconSize, gifColor);
    if (gifIcon) {
        int iconX = iconButtonX + (iconButtonSize - iconSize) / 2;
        int iconY = iconButtonY + (iconButtonSize - iconSize) / 2;
        gifIcon->draw(iconX, iconY);
    }
}

void TextChannelView::ensureEmojiAtlases(int targetSize) {
    if (m_emojiAtlasesLoaded) {
        return;
    }

    m_emojiAtlasesLoaded = true;
    m_emojiFramesInactive = loadEmojiAtlasFrames("assets/imgs/emoji_atlas_inactive.png", targetSize);
    m_emojiFramesActive = loadEmojiAtlasFrames("assets/imgs/emoji_atlas_active.png", targetSize);
    tintEmojiFrames(m_emojiFramesInactive, ThemeColors::TEXT_MUTED);

    size_t commonFrameCount = std::min(m_emojiFramesInactive.size(), m_emojiFramesActive.size());
    if (commonFrameCount == 0) {
        m_emojiFramesInactive.clear();
        m_emojiFramesActive.clear();
        return;
    }

    m_emojiFramesInactive.resize(commonFrameCount);
    m_emojiFramesActive.resize(commonFrameCount);
    m_emojiFrameIndex = 0;
}

bool TextChannelView::updateAvatarHover(int mx, int my, bool forceClear) {
    std::string newMessageId;
    std::string newHoverKey;

    if (!forceClear) {
        for (const auto &hitbox : m_avatarHitboxes) {
            if (mx >= hitbox.x && mx < hitbox.x + hitbox.size && my >= hitbox.y && my < hitbox.y + hitbox.size) {
                newMessageId = hitbox.messageId;
                newHoverKey = hitbox.hoverKey;
                break;
            }
        }
    }

    bool changed = newMessageId != m_hoveredAvatarMessageId || newHoverKey != m_hoveredAvatarKey;
    if (changed) {
        m_hoveredAvatarMessageId = newMessageId;
        m_hoveredAvatarKey = newHoverKey;
        MessageWidget::setHoveredAvatarKey(m_hoveredAvatarKey);
    }

    return changed;
}

bool TextChannelView::getEmojiButtonRect(int &outX, int &outY, int &outSize) const {
    return getInputButtonRect(0, outX, outY, outSize);
}

bool TextChannelView::getInputButtonRect(int index, int &outX, int &outY, int &outSize) const {
    if (!m_canSendMessages) {
        return false;
    }

    int inputY = y() + h() - MESSAGE_INPUT_HEIGHT;
    int inputH = MESSAGE_INPUT_HEIGHT;

    int bubbleX = x() + kInputBubbleMargin;
    int bubbleY = inputY + kInputBubbleMargin;
    int bubbleW = w() - (kInputBubbleMargin * 2);
    int bubbleH = inputH - (kInputBubbleMargin * 2);

    if (index < 0 || index > 2) {
        return false;
    }

    outSize = kInputIconButtonSize;
    outX = bubbleX + bubbleW - outSize - kInputIconRightPadding - (index * (outSize + kInputIconSpacing));
    outY = bubbleY + (bubbleH - outSize) / 2;
    return true;
}

bool TextChannelView::getPlusButtonRect(int &outX, int &outY, int &outSize) const {
    if (!m_canSendMessages) {
        return false;
    }

    int inputY = y() + h() - MESSAGE_INPUT_HEIGHT;
    int inputH = MESSAGE_INPUT_HEIGHT;

    int bubbleX = x() + kInputBubbleMargin;
    int bubbleY = inputY + kInputBubbleMargin;
    int bubbleH = inputH - (kInputBubbleMargin * 2);

    outSize = kInputPlusButtonSize;
    outX = bubbleX + kInputPlusButtonPadding;
    outY = bubbleY + (bubbleH - outSize) / 2;
    return true;
}

void TextChannelView::loadMessagesFromStore() {
    auto state = Store::get().snapshot();
    auto it = state.channelMessages.find(m_channelId);
    if (it != state.channelMessages.end()) {
        m_messages = it->second;
        Logger::debug("TextChannelView: Loaded " + std::to_string(m_messages.size()) +
                      " messages from Store for channel " + m_channelId);
    } else {
        m_messages.clear();
    }
}

void TextChannelView::loadMessages() {
    loadMessagesFromStore();

    if (m_channelId.empty()) {
        return;
    }

    std::string channelId = m_channelId;

    Discord::APIClient::get().getChannelMessages(
        channelId, 50, std::nullopt,
        [channelId](const Discord::APIClient::Json &messagesJson) {
            std::vector<Message> messages;
            for (const auto &msgJson : messagesJson) {
                messages.push_back(Message::fromJson(msgJson));
            }

            Data::Database::get().insertMessages(messages);

            Store::get().update([&](AppState &state) { state.channelMessages[channelId] = messages; });

            Logger::info("Loaded " + std::to_string(messages.size()) + " messages for channel " + channelId);
        },
        [channelId](int code, const std::string &error) {
            Logger::error("Failed to load messages for channel " + channelId + ": " + error);
        });
}

void TextChannelView::updatePermissions() {
    if (m_guildId.empty()) {
        m_canSendMessages = true;
        return;
    }

    auto state = Store::get().snapshot();

    if (!state.currentUser.has_value()) {
        m_canSendMessages = false;
        return;
    }

    const std::string &userId = state.currentUser->id;
    auto channelIt = state.guildChannels.find(m_guildId);
    if (channelIt == state.guildChannels.end()) {
        m_canSendMessages = false;
        return;
    }

    std::shared_ptr<GuildChannel> targetChannel = nullptr;
    for (const auto &channel : channelIt->second) {
        if (channel->id == m_channelId) {
            targetChannel = channel;
            break;
        }
    }

    if (!targetChannel) {
        m_canSendMessages = false;
        return;
    }

    auto memberIt = state.guildMembers.find(m_guildId);
    std::vector<std::string> userRoleIds;
    if (memberIt != state.guildMembers.end()) {
        userRoleIds = memberIt->second.roleIds;
    }

    auto rolesIt = state.guildRoles.find(m_guildId);
    std::vector<Role> guildRoles;
    if (rolesIt != state.guildRoles.end()) {
        guildRoles = rolesIt->second;
    }

    uint64_t basePermissions = PermissionUtils::computeBasePermissions(m_guildId, userRoleIds, guildRoles);
    uint64_t channelPermissions = PermissionUtils::computeChannelPermissions(
        userId, m_guildId, userRoleIds, targetChannel->permissionOverwrites, basePermissions);
    m_canSendMessages = PermissionUtils::hasPermission(channelPermissions, Permissions::SEND_MESSAGES);
}
