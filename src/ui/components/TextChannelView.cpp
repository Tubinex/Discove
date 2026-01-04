#include "ui/components/TextChannelView.h"

#include "state/Store.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
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

std::unique_ptr<Fl_RGB_Image> copyEmojiCell(Fl_RGB_Image *atlas, int cellX, int cellY, int cellSize,
                                            int targetSize) {
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
}

TextChannelView::~TextChannelView() {}

void TextChannelView::draw() {
    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), y(), w(), h());

    drawHeader();
    int contentY = y() + HEADER_HEIGHT;
    int inputAreaHeight = m_canSendMessages ? MESSAGE_INPUT_HEIGHT : 0;
    int contentH = h() - HEADER_HEIGHT - inputAreaHeight;

    fl_push_clip(x(), contentY, w(), contentH);

    if (m_welcomeVisible && m_messages.empty()) {
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
    case FL_ENTER:
    case FL_MOVE:
    case FL_LEAVE: {
        int hoveredIndex = -1;
        bool plusHovered = false;
        if (event != FL_LEAVE) {
            int mx = Fl::event_x();
            int my = Fl::event_y();
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
    loadMessagesFromStore();
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
    int contentY = y() + HEADER_HEIGHT;
    int yPos = contentY + 16;

    std::string previousAuthorId;
    std::chrono::system_clock::time_point previousTimestamp;
    std::string previousDate;

    for (const auto &msg : m_messages) {
        auto msgTime = std::chrono::system_clock::to_time_t(msg.timestamp);
        std::tm tmStruct;
        localtime_s(&tmStruct, &msgTime);

        std::ostringstream dateStream;
        dateStream << std::put_time(&tmStruct, "%B %d, %Y");
        std::string currentDate = dateStream.str();

        if (currentDate != previousDate && !previousDate.empty()) {
            drawDateSeparator(currentDate, yPos);
        }
        previousDate = currentDate;
        drawMessage(msg, yPos, previousAuthorId, previousTimestamp);

        previousAuthorId = msg.authorId;
        previousTimestamp = msg.timestamp;
    }
}

void TextChannelView::drawDateSeparator(const std::string &date, int &yPos) {
    yPos += 12;

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

    yPos += 24;
}

void TextChannelView::drawMessage(const Message &msg, int &yPos, const std::string &previousAuthorId,
                                  const std::chrono::system_clock::time_point &previousTimestamp) {
    const int leftMargin = 16;
    const int avatarSize = 40;
    const int avatarMargin = 16;

    bool isGrouped = false;
    if (msg.authorId == previousAuthorId) {
        auto timeDiff = std::chrono::duration_cast<std::chrono::minutes>(msg.timestamp - previousTimestamp);
        if (timeDiff.count() < 5) {
            isGrouped = true;
        }
    }

    if (isGrouped) {
        yPos += MESSAGE_SPACING;

        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 16);
        int textX = x() + leftMargin + avatarSize + avatarMargin;
        int textY = yPos + 16;
        fl_draw(msg.content.c_str(), textX, textY);

        yPos += 20;
    } else {
        yPos += MESSAGE_GROUP_SPACING;

        fl_color(ThemeColors::BG_TERTIARY);
        fl_pie(x() + leftMargin, yPos, avatarSize, avatarSize, 0, 360);

        // TODO: Load actual avatar from user profile
        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 20);
        fl_draw("U", x() + leftMargin + avatarSize / 2 - 6, yPos + avatarSize / 2 + 7);

        std::string username = "User"; // TODO: Get from user profile
        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 16);
        int usernameX = x() + leftMargin + avatarSize + avatarMargin;
        int usernameY = yPos + 16;
        fl_draw(username.c_str(), usernameX, usernameY);

        auto msgTime = std::chrono::system_clock::to_time_t(msg.timestamp);
        std::tm tmStruct;
        localtime_s(&tmStruct, &msgTime);

        std::ostringstream timeStream;
        timeStream << std::put_time(&tmStruct, "%I:%M %p");
        std::string timeStr = timeStream.str();

        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 12);
        int usernameWidth = static_cast<int>(fl_width(username.c_str()));
        int timeX = usernameX + usernameWidth + 8;
        fl_draw(timeStr.c_str(), timeX, usernameY - 2);

        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_REGULAR, 16);
        int contentY = usernameY + 4;
        fl_draw(msg.content.c_str(), usernameX, contentY);

        yPos += avatarSize + 8;
    }
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
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      kInputIconButtonRadius, ThemeColors::BG_ACCENT);
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
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      kInputIconButtonRadius, ThemeColors::BG_ACCENT);
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
        RoundedStyle::drawRoundedRect(iconButtonX, iconButtonY, iconButtonSize, iconButtonSize,
                                      kInputIconButtonRadius, kInputIconButtonRadius, kInputIconButtonRadius,
                                      kInputIconButtonRadius, ThemeColors::BG_ACCENT);
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
    } else {
        m_messages.clear();
    }
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
