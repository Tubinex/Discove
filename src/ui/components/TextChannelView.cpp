#include "ui/components/TextChannelView.h"

#include "data/Database.h"
#include "net/APIClient.h"
#include "state/Store.h"
#include "ui/EmojiManager.h"
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
#include <random>
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
constexpr int kInputTextHeight = 28;
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

std::string makeNonce() {
    using namespace std::chrono;
    static std::mt19937 rng([]() {
        std::random_device rd;
        return std::mt19937(rd());
    }());
    static std::uniform_int_distribution<int> dist(0, 9999);

    auto nowMs = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    std::ostringstream nonce;
    nonce << nowMs << std::setw(4) << std::setfill('0') << dist(rng);
    std::string value = nonce.str();
    if (value.size() > 25) {
        value.resize(25);
    }
    return value;
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

} // namespace

int TextChannelView::estimatedLineCount(const Message &msg) const {
    if (msg.content.empty()) {
        return 1;
    }
    return std::max(1, static_cast<int>(msg.content.length() / 60) + 1);
}

int TextChannelView::estimateMessageHeight(const Message &msg, bool isGrouped) const {
    constexpr int kAvatarSize = 40;
    constexpr int kAuthorHeight = 20;
    constexpr int kLineHeight = 20;
    constexpr int kReactionHeight = 34;
    constexpr int kReactionRowSpacing = 6;
    constexpr int kReactionTopPadding = 6;
    constexpr int kReactionBottomPadding = 6;
    constexpr int kReactionAvgWidth = 84;
    constexpr int kReactionSpacing = 6;
    constexpr int kAttachmentHeight = 300;
    constexpr int kReplyPreviewHeight = 24;
    constexpr int kStickerMaxSize = 192;
    constexpr int kStickerSpacing = 8;
    constexpr int kStickerTopPadding = 8;
    constexpr int kStickerTopPaddingNoContent = 6;

    if (msg.isSystemMessage()) {
        return 32;
    }

    int height = 0;

    if (!isGrouped) {
        height += std::max(kAvatarSize, kAuthorHeight + 4);
    }

    if (msg.isReply()) {
        height += kReplyPreviewHeight;
    }

    if (!msg.content.empty()) {
        height += estimatedLineCount(msg) * kLineHeight;
    } else {
        height += kLineHeight;
    }

    if (!msg.attachments.empty()) {
        height += kAttachmentHeight * static_cast<int>(msg.attachments.size());
    }

    if (!msg.stickers.empty()) {
        int contentWidth = std::max(0, w() - 72);
        int stickerSize = std::min(std::max(0, contentWidth), kStickerMaxSize);
        if (stickerSize <= 0) {
            stickerSize = kStickerMaxSize;
        }

        bool hasContent = (msg.content.find_first_not_of(" \t\r\n") != std::string::npos);
        height += hasContent ? kStickerTopPadding : kStickerTopPaddingNoContent;
        height += stickerSize * static_cast<int>(msg.stickers.size());
        if (msg.stickers.size() > 1) {
            height += kStickerSpacing * (static_cast<int>(msg.stickers.size()) - 1);
        }
    }

    if (!msg.reactions.empty()) {
        int contentWidth = std::max(0, w() - 72);
        if (contentWidth > 0) {
            int reactionsPerRow = std::max(1, contentWidth / (kReactionAvgWidth + kReactionSpacing));
            int reactionRows = (static_cast<int>(msg.reactions.size()) + reactionsPerRow - 1) / reactionsPerRow;
            int reactionRowsHeight = reactionRows * kReactionHeight + (reactionRows - 1) * kReactionRowSpacing;
            height += kReactionTopPadding + reactionRowsHeight + kReactionBottomPadding;
        } else {
            height += kReactionTopPadding + kReactionHeight + kReactionBottomPadding;
        }
    }

    height += 8;
    return height;
}

namespace {

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
    begin();
    m_messageInput = new Fl_Input(0, 0, 0, 0);
    m_messageInput->box(FL_NO_BOX);
    m_messageInput->color(ThemeColors::BG_TERTIARY);
    m_messageInput->textfont(FontLoader::Fonts::INTER_REGULAR);
    m_messageInput->textsize(15);
    m_messageInput->textcolor(ThemeColors::TEXT_NORMAL);
    m_messageInput->cursor_color(ThemeColors::TEXT_NORMAL);
    m_messageInput->selection_color(ThemeColors::BTN_PRIMARY);
    m_messageInput->when(FL_WHEN_ENTER_KEY_ALWAYS);
    m_messageInput->callback(
        [](Fl_Widget *, void *data) {
            auto *view = static_cast<TextChannelView *>(data);
            if (!view || !view->m_messageInput) {
                return;
            }

            const char *raw = view->m_messageInput->value();
            if (!raw) {
                return;
            }

            std::string message = trimSpaces(raw);
            if (message.empty()) {
                return;
            }

            if (view->m_onSendMessage) {
                std::string nonce = makeNonce();
                view->enqueuePendingMessage(message, nonce);
                view->m_onSendMessage(message, nonce);
                view->m_messageInput->value("");
                view->m_messageInput->redraw();
            }
        },
        this);
    m_messageInput->hide();
    end();

    m_storeListenerId = Store::get().subscribe([this](const AppState &state) {
        if (m_isDestroying || m_channelId.empty()) {
            return;
        }

        auto it = state.channelMessages.find(m_channelId);
        if (it != state.channelMessages.end()) {
            const auto &newMessages = it->second;

            std::unordered_map<std::string, const Message *> newMessageMap;
            for (const auto &msg : newMessages) {
                newMessageMap[msg.id] = &msg;
            }

            for (const auto &oldMsg : m_messages) {
                auto newMsgIt = newMessageMap.find(oldMsg.id);
                if (newMsgIt != newMessageMap.end()) {
                    const Message *newMsg = newMsgIt->second;

                    bool contentChanged = (oldMsg.content != newMsg->content);
                    bool attachmentsChanged = (oldMsg.attachments.size() != newMsg->attachments.size());
                    bool embedsChanged = (oldMsg.embeds.size() != newMsg->embeds.size());
                    bool editedChanged = (oldMsg.editedTimestamp != newMsg->editedTimestamp);

                    bool reactionLayoutChanged = (oldMsg.reactions.size() != newMsg->reactions.size());
                    if (contentChanged || reactionLayoutChanged || attachmentsChanged || embedsChanged ||
                        editedChanged) {
                        auto layoutIt = m_layoutCache.find(oldMsg.id);
                        if (layoutIt != m_layoutCache.end()) {
                            m_heightEstimateCache[oldMsg.id] = layoutIt->second.layout.height;
                        }

                        m_layoutCache.erase(oldMsg.id);
                    }
                }
            }

            m_messages = newMessages;
            auto pendingIt = state.pendingChannelMessages.find(m_channelId);
            if (pendingIt != state.pendingChannelMessages.end()) {
                m_pendingMessages = pendingIt->second;
            } else {
                m_pendingMessages.clear();
            }
            m_messagesChanged = true;
            redraw();
        } else {
            m_messages.clear();
            auto pendingIt = state.pendingChannelMessages.find(m_channelId);
            if (pendingIt != state.pendingChannelMessages.end()) {
                m_pendingMessages = pendingIt->second;
            } else {
                m_pendingMessages.clear();
            }
            m_messagesChanged = true;
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

    if (m_welcomeVisible && m_messages.empty() && m_pendingMessages.empty()) {
        m_messagesScrollOffset = 0;
        m_messagesContentHeight = 0;
        m_messagesViewHeight = 0;
        m_avatarHitboxes.clear();
        updateAvatarHover(0, 0, true);
        m_attachmentDownloadHitboxes.clear();
        updateAttachmentDownloadHover(0, 0, true);
        drawWelcomeSection();
    } else {
        drawMessages();
    }

    fl_pop_clip();
    fl_pop_clip();

    if (m_messageInput) {
        if (m_canSendMessages) {
            if (!m_messageInput->visible()) {
                m_messageInput->show();
            }
        } else if (m_messageInput->visible()) {
            m_messageInput->hide();
        }

        if (!m_canSendMessages && m_inputFocused) {
            m_inputFocused = false;
            if (Fl::focus() == m_messageInput) {
                Fl::focus(nullptr);
            }
        }

        if (!m_inputFocused && Fl::focus() == m_messageInput) {
            Fl::focus(nullptr);
        }
    }

    if (m_canSendMessages) {
        drawMessageInput();
    }

    if (m_messageInput && m_messageInput->visible()) {
        draw_child(*m_messageInput);
    }
}

int TextChannelView::handle(int event) {
    if (event == FL_PUSH && m_messageInput && m_messageInput->visible()) {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        bool inInput = mx >= m_messageInput->x() && mx < m_messageInput->x() + m_messageInput->w() &&
                       my >= m_messageInput->y() && my < m_messageInput->y() + m_messageInput->h();

        if (inInput) {
            m_inputFocused = true;
            Fl::focus(m_messageInput);
            redraw();
            return m_messageInput->handle(event);
        }

        if (m_inputFocused) {
            m_inputFocused = false;
            if (Fl::focus() == m_messageInput) {
                Fl::focus(nullptr);
            }
            redraw();
        }
    }

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
        if (updateAttachmentDownloadHover(mx, my, event == FL_LEAVE)) {
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
    m_messagesContentHeight = 0;
    m_messagesScrollOffset = 0;
    m_shouldScrollToBottom = true;
    m_layoutCache.clear();
    m_heightEstimateCache.clear();
    m_avatarHitboxes.clear();
    m_attachmentDownloadHitboxes.clear();
    m_hoveredAvatarMessageId.clear();
    m_hoveredAttachmentDownloadKey.clear();
    m_previousMessageIds.clear();
    m_previousItemYPositions.clear();
    m_previousItemHeights.clear();
    m_previousTotalHeight = 0;
    m_pendingMessages.clear();
    if (!m_hoveredAvatarKey.empty()) {
        m_hoveredAvatarKey.clear();
        MessageWidget::setHoveredAvatarKey("");
    }
    MessageWidget::setHoveredAttachmentDownloadKey("");
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
    m_attachmentDownloadHitboxes.clear();

    int renderTop = viewTop - kMessageRenderPadding;
    int renderBottom = viewBottom + kMessageRenderPadding;

    struct AnchorInfo {
        bool valid = false;
        std::string messageId;
        int screenY = 0;
    };

    AnchorInfo anchor;
    if (!m_shouldScrollToBottom && m_messagesViewHeight > 0 && !m_previousMessageIds.empty() &&
        m_previousMessageIds.size() == m_previousItemYPositions.size() &&
        m_previousMessageIds.size() == m_previousItemHeights.size() && m_previousTotalHeight > 0) {
        int previousYPos = viewBottom - m_previousTotalHeight + m_messagesScrollOffset;
        for (size_t i = 0; i < m_previousMessageIds.size(); ++i) {
            int messageTop = previousYPos + m_previousItemYPositions[i];
            int messageBottom = messageTop + m_previousItemHeights[i];
            if (messageBottom > viewTop) {
                anchor.valid = true;
                anchor.messageId = m_previousMessageIds[i];
                anchor.screenY = messageTop;
                break;
            }
        }
    }

    std::vector<const Message *> ordered;
    ordered.reserve(m_messages.size() + m_pendingMessages.size());
    std::unordered_map<std::string, const Message *> messageById;
    messageById.reserve(m_messages.size() + m_pendingMessages.size());
    std::unordered_set<std::string> realNonces;
    realNonces.reserve(m_messages.size());

    for (const auto &msg : m_messages) {
        ordered.push_back(&msg);
        messageById[msg.id] = &msg;
        if (msg.nonce.has_value()) {
            realNonces.insert(*msg.nonce);
        }
    }

    for (const auto &pending : m_pendingMessages) {
        if (pending.nonce.has_value() && realNonces.find(*pending.nonce) != realNonces.end()) {
            continue;
        }
        ordered.push_back(&pending);
        messageById[pending.id] = &pending;
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
        int yPos = 0;
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
    std::unordered_map<std::string, size_t> indexByMessageId;
    indexByMessageId.reserve(ordered.size());

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
        indexByMessageId[msg->id] = messageInfos.size() - 1;

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

    if (m_layoutCache.size() > 2000) {
        std::unordered_set<std::string> currentMessageIds;
        for (const auto &info : messageInfos) {
            if (info.msg)
                currentMessageIds.insert(info.msg->id);
        }
        for (auto it = m_layoutCache.begin(); it != m_layoutCache.end();) {
            if (currentMessageIds.find(it->first) == currentMessageIds.end()) {
                it = m_layoutCache.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_heightEstimateCache.begin(); it != m_heightEstimateCache.end();) {
            if (currentMessageIds.find(it->first) == currentMessageIds.end()) {
                it = m_heightEstimateCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    m_itemYPositions.clear();
    m_separatorYPositions.clear();
    m_itemYPositions.reserve(messageInfos.size());
    m_separatorYPositions.reserve(messageInfos.size());

    auto hasVisualExtras = [](const Message *msg) {
        return msg &&
               (!msg->attachments.empty() || !msg->embeds.empty() || !msg->reactions.empty() || !msg->stickers.empty());
    };

    std::vector<std::string> messageIds;
    std::vector<int> itemHeights;
    messageIds.reserve(messageInfos.size());
    itemHeights.reserve(messageInfos.size());

    int totalHeight = 0;
    for (size_t i = 0; i < messageInfos.size(); ++i) {
        const auto &info = messageInfos[i];

        int separatorY = -1;
        if (info.needsSeparator) {
            separatorY = totalHeight;
            totalHeight += kDateSeparatorHeight;
        }
        m_separatorYPositions.push_back(separatorY);

        if (!info.msg) {
            m_itemYPositions.push_back(totalHeight);
            messageIds.emplace_back();
            itemHeights.push_back(0);
            continue;
        }

        bool isSystem = info.msg->isSystemMessage();
        bool compactBottom = false;
        if (i + 1 < messageInfos.size()) {
            const auto &nextInfo = messageInfos[i + 1];
            compactBottom = nextInfo.grouped;
            if (compactBottom) {
                bool currentHasExtras =
                    !info.msg->attachments.empty() || !info.msg->reactions.empty() || !info.msg->stickers.empty();
                bool nextHasExtras = nextInfo.msg && (!nextInfo.msg->attachments.empty() || !nextInfo.msg->reactions.empty() ||
                                                     !nextInfo.msg->stickers.empty());
                compactBottom = !currentHasExtras && !nextHasExtras;
            }
        }

        int spacing = 0;
        if (i > 0) {
            if (!info.needsSeparator) {
                const auto &prev = messageInfos[i - 1];
                if (info.grouped) {
                    spacing = 0;
                } else if (isSystem && prev.msg && prev.msg->isSystemMessage()) {
                    spacing = kSystemMessageSpacing;
                } else {
                    spacing = MESSAGE_GROUP_SPACING;
                }
            }
        }
        totalHeight += spacing;
        m_itemYPositions.push_back(totalHeight);

        int messageHeight;
        try {
            auto cacheIt = m_layoutCache.find(info.msg->id);
            if (cacheIt != m_layoutCache.end() && cacheIt->second.width == w() &&
                cacheIt->second.grouped == info.grouped && cacheIt->second.compactBottom == compactBottom) {
                messageHeight = cacheIt->second.layout.height;
            } else if (cacheIt != m_layoutCache.end() && cacheIt->second.grouped == info.grouped &&
                       cacheIt->second.compactBottom == compactBottom) {
                messageHeight = cacheIt->second.layout.height;
            } else {
                auto estimateIt = m_heightEstimateCache.find(info.msg->id);
                if (estimateIt != m_heightEstimateCache.end()) {
                    messageHeight = estimateIt->second;
                } else {
                    messageHeight = estimateMessageHeight(*info.msg, info.grouped);
                    m_heightEstimateCache[info.msg->id] = messageHeight;
                }
            }
        } catch (...) {
            messageHeight = estimateMessageHeight(*info.msg, info.grouped);
            m_heightEstimateCache[info.msg->id] = messageHeight;
        }

        totalHeight += messageHeight;
        messageIds.push_back(info.msg->id);
        itemHeights.push_back(messageHeight);
    }

    int extraBottomPadding = m_canSendMessages ? 0 : MESSAGE_INPUT_HEIGHT;
    totalHeight += extraBottomPadding;

    int oldContentHeight = m_messagesContentHeight;
    m_messagesContentHeight = totalHeight;
    int maxScroll = std::max(0, totalHeight - m_messagesViewHeight);

    bool wasAtBottom = (m_messagesScrollOffset <= 10) || m_shouldScrollToBottom;
    if (m_shouldScrollToBottom) {
        m_messagesScrollOffset = 0;
        m_shouldScrollToBottom = false;
    } else if (oldContentHeight > 0 && totalHeight != oldContentHeight) {
        int heightDiff = totalHeight - oldContentHeight;
        if (wasAtBottom) {
            m_messagesScrollOffset = 0;
        } else if (anchor.valid) {
            auto anchorIt = indexByMessageId.find(anchor.messageId);
            if (anchorIt != indexByMessageId.end()) {
                size_t anchorIndex = anchorIt->second;
                if (anchorIndex < m_itemYPositions.size()) {
                    int newOffset =
                        anchor.screenY - viewBottom + totalHeight - m_itemYPositions[anchorIndex];
                    m_messagesScrollOffset = std::clamp(newOffset, 0, maxScroll);
                } else {
                    m_messagesScrollOffset += heightDiff;
                }
            } else {
                m_messagesScrollOffset += heightDiff;
            }
        } else {
            m_messagesScrollOffset += heightDiff;
        }
    }

    m_messagesChanged = false;
    if (m_messagesScrollOffset > maxScroll) {
        m_messagesScrollOffset = maxScroll;
    } else if (m_messagesScrollOffset < 0) {
        m_messagesScrollOffset = 0;
    }

    int yPos = viewBottom - totalHeight + m_messagesScrollOffset;
    for (size_t i = 0; i < messageInfos.size(); ++i) {
        const auto &info = messageInfos[i];

        if (i >= m_itemYPositions.size() || i >= m_separatorYPositions.size()) {
            continue;
        }

        if (info.needsSeparator && m_separatorYPositions[i] >= 0) {
            int sepY = yPos + m_separatorYPositions[i];
            if (sepY + kDateSeparatorHeight >= renderTop && sepY <= renderBottom) {
                RenderEntry entry;
                entry.type = RenderEntry::Type::Separator;
                entry.date = info.date;
                entry.yPos = sepY;
                entries.push_back(entry);
            }
        }

        if (!info.msg) {
            continue;
        }

        bool isSystem = info.msg->isSystemMessage();
        bool compactBottom = false;
        if (i + 1 < messageInfos.size()) {
            const auto &nextInfo = messageInfos[i + 1];
            compactBottom = nextInfo.grouped;
            if (compactBottom) {
                bool currentHasExtras =
                    !info.msg->attachments.empty() || !info.msg->reactions.empty() || !info.msg->stickers.empty();
                bool nextHasExtras = nextInfo.msg && (!nextInfo.msg->attachments.empty() || !nextInfo.msg->reactions.empty() ||
                                                     !nextInfo.msg->stickers.empty());
                compactBottom = !currentHasExtras && !nextHasExtras;
            }
        }

        int messageY = yPos + m_itemYPositions[i];
        int estimatedHeight = 100;
        auto estimateIt = m_heightEstimateCache.find(info.msg->id);
        if (estimateIt != m_heightEstimateCache.end()) {
            estimatedHeight = estimateIt->second;
        }

        if (messageY + estimatedHeight < renderTop || messageY > renderBottom) {
            continue;
        }

        auto cacheIt = m_layoutCache.find(info.msg->id);
        bool needsLayout = (cacheIt == m_layoutCache.end() || cacheIt->second.width != w() ||
                            cacheIt->second.grouped != info.grouped || cacheIt->second.compactBottom != compactBottom);

        if (needsLayout) {
            try {
                MessageWidget::ReplyPreview replyPreview;
                MessageWidget::ReplyPreview *replyPtr = nullptr;
                if (info.msg->isReply() && info.msg->referencedMessageId.has_value()) {
                    auto refIt = messageById.find(*info.msg->referencedMessageId);
                    const Message *referenced = (refIt != messageById.end()) ? refIt->second : nullptr;
                    replyPreview = buildReplyPreview(referenced);
                    replyPtr = &replyPreview;
                }

                MessageWidget::Layout layout =
                    MessageWidget::buildLayout(*info.msg, w(), info.grouped, compactBottom, replyPtr);

                int layoutHeight = layout.height;

                LayoutCacheEntry cacheEntry;
                cacheEntry.layout = std::move(layout);
                cacheEntry.width = w();
                cacheEntry.grouped = info.grouped;
                cacheEntry.compactBottom = compactBottom;
                m_layoutCache[info.msg->id] = std::move(cacheEntry);
                m_heightEstimateCache[info.msg->id] = layoutHeight;
            } catch (...) {
                continue;
            }
            cacheIt = m_layoutCache.find(info.msg->id);
        }

        if (cacheIt == m_layoutCache.end()) {
            continue;
        }

        int messageHeight = cacheIt->second.layout.height;
        if (messageY + messageHeight < renderTop || messageY > renderBottom) {
            continue;
        }

        RenderEntry entry;
        entry.type = RenderEntry::Type::Message;
        entry.msg = info.msg;
        entry.layout = cacheIt->second.layout;
        entry.grouped = info.grouped;
        entry.yPos = messageY;
        entries.push_back(entry);
    }

    std::unordered_set<std::string> keepAvatarKeys;
    std::unordered_set<std::string> keepAnimatedAvatarKeys;
    std::unordered_set<std::string> keepStickerKeys;
    std::unordered_set<std::string> keepAttachmentKeys;
    std::unordered_set<std::string> keepEmojiKeys;
    for (auto &entry : entries) {
        try {
            if (entry.type == RenderEntry::Type::Separator) {
                int separatorY = entry.yPos;
                drawDateSeparator(entry.date, separatorY);
                continue;
            }

            if (entry.msg) {
                int messageY = entry.yPos;
                bool avatarHovered = false;
                if (!entry.grouped && !entry.msg->isSystemMessage()) {
                    AvatarHitbox hitbox;
                    hitbox.x = x() + entry.layout.avatarX;
                    hitbox.y = messageY + entry.layout.avatarY;
                    hitbox.size = entry.layout.avatarSize;
                    hitbox.messageId = entry.msg->id;
                    hitbox.hoverKey = MessageWidget::getAnimatedAvatarKey(*entry.msg, entry.layout.avatarSize);

                    std::string animatedAvatarKey = hitbox.hoverKey;
                    std::string avatarKey = MessageWidget::getAvatarCacheKey(*entry.msg, entry.layout.avatarSize);

                    m_avatarHitboxes.push_back(std::move(hitbox));
                    avatarHovered = (entry.msg->id == m_hoveredAvatarMessageId);

                    if (!avatarKey.empty()) {
                        keepAvatarKeys.insert(avatarKey);
                    }
                    if (!animatedAvatarKey.empty()) {
                        keepAnimatedAvatarKeys.insert(animatedAvatarKey);
                    }
                }

                for (const auto &attachmentLayout : entry.layout.attachments) {
                    if (!attachmentLayout.cacheKey.empty()) {
                        keepAttachmentKeys.insert(attachmentLayout.cacheKey);
                    }
                }

                for (const auto &stickerLayout : entry.layout.stickers) {
                    if (!stickerLayout.cacheKey.empty()) {
                        keepStickerKeys.insert(stickerLayout.cacheKey);
                    }
                }

                for (const auto &line : entry.layout.lines) {
                    for (const auto &item : line.items) {
                        if (item.kind == MessageWidget::InlineItem::Kind::Emoji && !item.emojiCacheKey.empty()) {
                            keepEmojiKeys.insert(item.emojiCacheKey);
                        }
                    }
                }

                for (const auto &reactionLayout : entry.layout.reactions) {
                    if (!reactionLayout.emojiCacheKey.empty()) {
                        keepEmojiKeys.insert(reactionLayout.emojiCacheKey);
                    }
                }

                MessageWidget::draw(*entry.msg, entry.layout, x(), messageY, avatarHovered);

                if (!entry.layout.isSystem && !entry.layout.attachments.empty() && !entry.msg->attachments.empty()) {
                    int attachmentsTop =
                        messageY + entry.layout.contentTop + entry.layout.contentHeight + entry.layout.stickersHeight +
                        entry.layout.attachmentsTopPadding;
                    size_t count = std::min(entry.layout.attachments.size(), entry.msg->attachments.size());

                    for (size_t i = 0; i < count; ++i) {
                        const auto &attachmentLayout = entry.layout.attachments[i];
                        if (attachmentLayout.isImage || attachmentLayout.downloadSize <= 0) {
                            continue;
                        }

                        int boxX = x() + entry.layout.contentX + attachmentLayout.xOffset;
                        int boxY = attachmentsTop + attachmentLayout.yOffset;
                        int buttonX = boxX + attachmentLayout.downloadXOffset;
                        int buttonY = boxY + attachmentLayout.downloadYOffset;

                        AttachmentDownloadHitbox hitbox;
                        hitbox.x = buttonX;
                        hitbox.y = buttonY;
                        hitbox.size = attachmentLayout.downloadSize;
                        hitbox.key = MessageWidget::getAttachmentDownloadKey(*entry.msg, i);
                        m_attachmentDownloadHitboxes.push_back(std::move(hitbox));
                    }
                }
            }
        } catch (...) {
            continue;
        }
    }

    MessageWidget::pruneAvatarCache(keepAvatarKeys);
    MessageWidget::pruneAnimatedAvatarCache(keepAnimatedAvatarKeys);
    MessageWidget::pruneAnimatedEmojiCache(keepEmojiKeys);
    MessageWidget::pruneStickerCache(keepStickerKeys);
    MessageWidget::pruneAnimatedStickerCache(keepStickerKeys);
    MessageWidget::pruneAttachmentCache(keepAttachmentKeys);
    MessageWidget::pruneEmojiCache(keepEmojiKeys);
    EmojiManager::pruneCache(keepEmojiKeys);

    if (!m_hoveredAvatarMessageId.empty()) {
        bool stillVisible =
            std::any_of(m_avatarHitboxes.begin(), m_avatarHitboxes.end(),
                        [this](const AvatarHitbox &hitbox) { return hitbox.messageId == m_hoveredAvatarMessageId; });
        if (!stillVisible) {
            m_hoveredAvatarMessageId.clear();
            if (!m_hoveredAvatarKey.empty()) {
                m_hoveredAvatarKey.clear();
                MessageWidget::setHoveredAvatarKey("");
            }
        }
    }

    if (!m_hoveredAttachmentDownloadKey.empty()) {
        bool stillVisible =
            std::any_of(m_attachmentDownloadHitboxes.begin(), m_attachmentDownloadHitboxes.end(),
                        [this](const AttachmentDownloadHitbox &hitbox) {
                            return hitbox.key == m_hoveredAttachmentDownloadKey;
                        });
        if (!stillVisible) {
            m_hoveredAttachmentDownloadKey.clear();
            MessageWidget::setHoveredAttachmentDownloadKey("");
        }
    }

    m_previousMessageIds = std::move(messageIds);
    m_previousItemYPositions = m_itemYPositions;
    m_previousItemHeights = std::move(itemHeights);
    m_previousTotalHeight = totalHeight;
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

    int inputX = buttonX + kInputPlusButtonSize + kInputPlaceholderPadding;
    int inputTextY = bubbleY + (bubbleH - kInputTextHeight) / 2;
    int inputRight = bubbleX + bubbleW - kInputIconRightPadding - (kInputIconButtonSize * 3) -
                     (kInputIconSpacing * 2) - kInputIconSpacing;
    int inputW = std::max(1, inputRight - inputX);

    if (m_messageInput) {
        m_messageInput->resize(inputX, inputTextY, inputW, kInputTextHeight);
        m_messageInput->activate();
    }

    bool showPlaceholder = true;
    if (m_messageInput) {
        const char *value = m_messageInput->value();
        bool hasText = value && value[0] != '\0';
        bool hasFocus = (Fl::focus() == m_messageInput);
        showPlaceholder = !hasText && !hasFocus;
    }

    if (showPlaceholder) {
        std::string placeholder = "Message #" + m_channelName;
        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 15);
        int placeholderX = inputX;
        int placeholderY = bubbleY + bubbleH / 2 + 5;
        fl_draw(placeholder.c_str(), placeholderX, placeholderY);
    }

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

bool TextChannelView::updateAttachmentDownloadHover(int mx, int my, bool forceClear) {
    std::string newKey;

    if (!forceClear) {
        for (const auto &hitbox : m_attachmentDownloadHitboxes) {
            if (mx >= hitbox.x && mx < hitbox.x + hitbox.size && my >= hitbox.y && my < hitbox.y + hitbox.size) {
                newKey = hitbox.key;
                break;
            }
        }
    }

    bool changed = newKey != m_hoveredAttachmentDownloadKey;
    if (changed) {
        m_hoveredAttachmentDownloadKey = newKey;
        MessageWidget::setHoveredAttachmentDownloadKey(m_hoveredAttachmentDownloadKey);
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

    auto pendingIt = state.pendingChannelMessages.find(m_channelId);
    if (pendingIt != state.pendingChannelMessages.end()) {
        m_pendingMessages = pendingIt->second;
    } else {
        m_pendingMessages.clear();
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

void TextChannelView::enqueuePendingMessage(const std::string &content, const std::string &nonce) {
    if (m_channelId.empty() || content.empty() || nonce.empty()) {
        return;
    }

    auto snapshot = Store::get().snapshot();
    if (!snapshot.currentUser.has_value()) {
        return;
    }

    Message pending;
    pending.id = "pending:" + nonce;
    pending.channelId = m_channelId;
    pending.content = content;
    pending.timestamp = std::chrono::system_clock::now();
    pending.nonce = nonce;
    pending.isPending = true;
    pending.type = MessageType::DEFAULT;
    pending.authorId = snapshot.currentUser->id;
    pending.authorUsername = snapshot.currentUser->username;
    pending.authorGlobalName = snapshot.currentUser->globalName;
    pending.authorDiscriminator = snapshot.currentUser->discriminator;
    pending.authorAvatarHash = snapshot.currentUser->avatarHash;

    if (!m_guildId.empty()) {
        pending.guildId = m_guildId;
        auto memberIt = snapshot.guildMembers.find(m_guildId);
        if (memberIt != snapshot.guildMembers.end() && memberIt->second.userId == pending.authorId) {
            pending.authorNickname = memberIt->second.nick;
        }
    }

    m_shouldScrollToBottom = true;
    Store::get().update([channelId = m_channelId, pending = std::move(pending)](AppState &state) mutable {
        auto &pendingList = state.pendingChannelMessages[channelId];
        auto it = std::find_if(pendingList.begin(), pendingList.end(), [&](const Message &msg) {
            return msg.nonce.has_value() && msg.nonce == pending.nonce;
        });
        if (it == pendingList.end()) {
            pendingList.push_back(std::move(pending));
        }
    });
}
