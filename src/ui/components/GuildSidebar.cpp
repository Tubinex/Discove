#include "ui/components/GuildSidebar.h"

#include "state/AppState.h"
#include "state/Store.h"
#include "ui/AnimationManager.h"
#include "ui/GifAnimation.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
#include "ui/VirtualScroll.h"
#include "utils/CDN.h"
#include "utils/Fonts.h"
#include "utils/Images.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <functional>
#include <fstream>
#include <map>

namespace {
std::string ellipsizeText(const std::string &text, int maxWidth) {
    if (maxWidth <= 0) {
        return "";
    }

    int fullWidth = static_cast<int>(fl_width(text.c_str()));
    if (fullWidth <= maxWidth) {
        return text;
    }

    const char *ellipsis = "...";
    int ellipsisWidth = static_cast<int>(fl_width(ellipsis));
    if (ellipsisWidth >= maxWidth) {
        return "";
    }

    std::string truncated = text;
    while (!truncated.empty()) {
        truncated.pop_back();
        int width = static_cast<int>(fl_width(truncated.c_str()));
        if (width <= maxWidth - ellipsisWidth) {
            break;
        }
    }

    return truncated.empty() ? std::string(ellipsis) : truncated + ellipsis;
}

struct ChannelSignature {
    std::string id;
    std::string name;
    int position = 0;
    std::string parentId;
    ChannelType type = ChannelType::GUILD_TEXT;
    uint64_t permsHash = 0;

    bool operator==(const ChannelSignature &other) const {
        return id == other.id && name == other.name && position == other.position && parentId == other.parentId &&
               type == other.type && permsHash == other.permsHash;
    }

    bool operator!=(const ChannelSignature &other) const { return !(*this == other); }
};

uint64_t hashCombine(uint64_t seed, uint64_t value) {
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

uint64_t hashString(const std::string &value) {
    return static_cast<uint64_t>(std::hash<std::string>{}(value));
}

uint64_t hashOverwrites(const std::vector<PermissionOverwrite> &overwrites) {
    uint64_t seed = 0;
    for (const auto &entry : overwrites) {
        seed = hashCombine(seed, hashString(entry.id));
        seed = hashCombine(seed, static_cast<uint64_t>(entry.type));
        seed = hashCombine(seed, entry.allow);
        seed = hashCombine(seed, entry.deny);
    }
    return seed;
}

std::vector<ChannelSignature> buildChannelSignature(const AppState &state, const std::string &guildId) {
    std::vector<ChannelSignature> signatures;
    if (guildId.empty()) {
        return signatures;
    }

    auto it = state.guildChannels.find(guildId);
    if (it == state.guildChannels.end()) {
        return signatures;
    }

    const auto &channels = it->second;
    signatures.reserve(channels.size());
    for (const auto &channel : channels) {
        if (!channel) {
            continue;
        }

        ChannelSignature sig;
        sig.id = channel->id;
        sig.name = channel->name.value_or("");
        sig.position = channel->position;
        sig.parentId = channel->parentId.value_or("");
        sig.type = channel->type;
        sig.permsHash = hashOverwrites(channel->permissionOverwrites);
        signatures.push_back(std::move(sig));
    }

    return signatures;
}
} // namespace

std::map<std::string, std::map<std::string, bool>> GuildSidebar::s_guildCategoryCollapsedState;

GuildSidebar::GuildSidebar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    clip_children(1);
    end();

    m_storeListenerId = Store::get().subscribe<std::vector<ChannelSignature>>(
        [this](const AppState &state) { return buildChannelSignature(state, m_guildId); },
        [this](const std::vector<ChannelSignature> &) {
            if (m_guildId.empty()) {
                return;
            }
            Fl::lock();
            loadChannelsFromStore();
            redraw();
            Fl::unlock();
            Fl::awake();
        },
        [](const std::vector<ChannelSignature> &a, const std::vector<ChannelSignature> &b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i] != b[i]) {
                    return false;
                }
            }
            return true;
        },
        true);
}

GuildSidebar::~GuildSidebar() {
    stopBannerAnimation();
    if (m_storeListenerId) {
        Store::get().unsubscribe(m_storeListenerId);
    }
}

void GuildSidebar::draw() {
    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), h());

    int headerHeight = getHeaderHeight();

    fl_push_clip(x(), y() + headerHeight, w(), h() - headerHeight);

    constexpr int kRenderPadding = 200;
    int viewTop = y() + headerHeight;
    int viewBottom = y() + h();
    int renderTop = viewTop - kRenderPadding;
    int renderBottom = viewBottom + kRenderPadding;

    int currentY = y() + headerHeight - m_scrollOffset;

    currentY += 16;

    for (auto &channel : m_uncategorizedChannels) {
        channel.yPos = currentY;

        if (currentY + CHANNEL_HEIGHT < renderTop) {
            currentY += CHANNEL_HEIGHT;
            continue;
        }

        if (currentY > renderBottom) {
            break;
        }

        bool selected = (channel.id == m_selectedChannelId);
        bool hovered = (m_hoveredChannelIndex == static_cast<int>(&channel - &m_uncategorizedChannels[0]) &&
                        m_hoveredCategoryId.empty());
        drawChannel(channel, currentY, selected, hovered);
        currentY += CHANNEL_HEIGHT;
    }

    if (!m_uncategorizedChannels.empty()) {
        currentY += 16;
    }

    for (auto &category : m_categories) {
        category.yPos = currentY;
        int channelCount = static_cast<int>(category.channels.size());

        int categoryTotalHeight = CATEGORY_HEIGHT;
        if (!category.collapsed) {
            categoryTotalHeight += static_cast<int>(category.channels.size()) * CHANNEL_HEIGHT;
        }
        categoryTotalHeight += 16;

        if (currentY + categoryTotalHeight < renderTop) {
            currentY += categoryTotalHeight;
            continue;
        }

        if (currentY > renderBottom) {
            break;
        }

        bool categoryHovered = (m_hoveredCategoryId == category.id && m_hoveredChannelIndex == -1);
        drawChannelCategory(category.name.c_str(), currentY, category.collapsed, channelCount, categoryHovered);

        if (!category.collapsed) {
            for (size_t i = 0; i < category.channels.size(); ++i) {
                auto &channel = category.channels[i];
                channel.yPos = currentY;

                if (currentY + CHANNEL_HEIGHT < renderTop) {
                    currentY += CHANNEL_HEIGHT;
                    continue;
                }

                if (currentY > renderBottom) {
                    break;
                }

                bool selected = (channel.id == m_selectedChannelId);
                bool hovered = (m_hoveredChannelIndex == static_cast<int>(i) && m_hoveredCategoryId == category.id);

                drawChannel(channel, currentY, selected, hovered);
                currentY += CHANNEL_HEIGHT;
            }
        }

        currentY += 16;
    }

    fl_pop_clip();

    drawServerHeader();

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y(), x(), y() + h());

    fl_pop_clip();
}

int GuildSidebar::getHeaderHeight() const {
    return !m_bannerHash.empty() ? LayoutConstants::kGuildBannerHeight : LayoutConstants::kGuildSimpleHeaderHeight;
}

void GuildSidebar::drawServerHeader() {
    if (!m_bannerHash.empty()) {
        drawBanner();
    } else {
        drawSimpleHeader();
    }

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x() + 1, y() + getHeaderHeight() - 1, x() + w(), y() + getHeaderHeight() - 1);
}

void GuildSidebar::drawSimpleHeader() {
    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), LayoutConstants::kGuildSimpleHeaderHeight);

    const int fontSize = LayoutConstants::kGuildHeaderFontSize;
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, fontSize);

    const int leftMargin = 16;
    const int topMargin = 16;
    int textY = y() + topMargin + fontSize;
    fl_draw(m_guildName.c_str(), x() + leftMargin, textY);

    fl_font(FontLoader::Fonts::INTER_REGULAR, LayoutConstants::kDropdownIconFontSize);
    fl_draw("▼", x() + w() - 24, textY - 2);
}

void GuildSidebar::drawBanner() {
    int bannerY = y();
    int bannerW = w();
    const int bannerHeight = LayoutConstants::kGuildBannerHeight;

    Fl_RGB_Image *sourceImage = nullptr;
    if (m_bannerGif && m_bannerGif->isValid()) {
        sourceImage = m_bannerGif->currentFrame();
    } else if (m_bannerImage) {
        sourceImage = m_bannerImage;
    }

    if (sourceImage && sourceImage->w() > 0 && sourceImage->h() > 0) {
        fl_push_clip(x(), bannerY, bannerW, bannerHeight);

        const int depth = sourceImage->d();
        if (depth < 3) {
            Fl_RGB_Image *scaled = (Fl_RGB_Image *)sourceImage->copy(bannerW, bannerHeight);
            if (scaled) {
                scaled->draw(x(), bannerY);
                delete scaled;
            }
            fl_pop_clip();
            return;
        }

        const float targetAspect = 2.4f;
        const float sourceAspect = static_cast<float>(sourceImage->w()) / static_cast<float>(sourceImage->h());

        int cropX = 0, cropY = 0, cropW = sourceImage->w(), cropH = sourceImage->h();
        if (sourceAspect > targetAspect) {
            cropW = static_cast<int>(sourceImage->h() * targetAspect);
            cropX = (sourceImage->w() - cropW) / 2;
        } else if (sourceAspect < targetAspect) {
            cropH = static_cast<int>(sourceImage->w() / targetAspect);
            cropY = (sourceImage->h() - cropH) / 2;
        }

        const char *const *dataArray = sourceImage->data();
        if (!dataArray || !dataArray[0]) {
            fl_pop_clip();
            return;
        }

        const int srcLineSize = sourceImage->w() * depth;
        const int dstLineSize = bannerW * depth;

        unsigned char *finalData = new unsigned char[bannerW * bannerHeight * depth];

        const int gradientHeight = 64;
        const unsigned char darkR = 26;
        const unsigned char darkG = 26;
        const unsigned char darkB = 30;

        for (int dstY = 0; dstY < bannerHeight; dstY++) {
            int srcY = cropY + (dstY * cropH) / bannerHeight;
            const unsigned char *srcRow = reinterpret_cast<const unsigned char *>(dataArray[0]) + (srcY * srcLineSize);
            unsigned char *dstRow = finalData + (dstY * dstLineSize);

            float alpha = 0.0f;
            if (dstY < gradientHeight) {
                float progress = static_cast<float>(dstY) / gradientHeight;
                float easedProgress = progress * progress * progress;
                alpha = (1.0f - easedProgress) * 0.8f;
            }

            for (int dstX = 0; dstX < bannerW; dstX++) {
                int srcX = cropX + (dstX * cropW) / bannerW;
                const unsigned char *srcPixel = srcRow + (srcX * depth);
                unsigned char *dstPixel = dstRow + (dstX * depth);

                if (alpha > 0.0f) {
                    dstPixel[0] = static_cast<unsigned char>(srcPixel[0] * (1.0f - alpha) + darkR * alpha);
                    dstPixel[1] = static_cast<unsigned char>(srcPixel[1] * (1.0f - alpha) + darkG * alpha);
                    dstPixel[2] = static_cast<unsigned char>(srcPixel[2] * (1.0f - alpha) + darkB * alpha);
                } else {
                    dstPixel[0] = srcPixel[0];
                    dstPixel[1] = srcPixel[1];
                    dstPixel[2] = srcPixel[2];
                }

                if (depth == 4) {
                    dstPixel[3] = srcPixel[3];
                }
            }
        }

        Fl_RGB_Image *finalImage = new Fl_RGB_Image(finalData, bannerW, bannerHeight, depth);
        finalImage->alloc_array = 1;
        finalImage->draw(x(), bannerY);
        delete finalImage;

        fl_pop_clip();
    } else {
        fl_color(ThemeColors::BG_TERTIARY);
        fl_rectf(x(), bannerY, bannerW, bannerHeight);
    }

    const int leftMargin = 16;
    const int topMargin = 16;
    const int fontSize = LayoutConstants::kGuildHeaderFontSize;
    fl_color(FL_WHITE);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, fontSize);
    int textY = bannerY + topMargin + fontSize;
    fl_draw(m_guildName.c_str(), x() + leftMargin, textY);

    fl_font(FontLoader::Fonts::INTER_REGULAR, LayoutConstants::kDropdownIconFontSize);
    fl_draw("▼", x() + w() - 24, textY - 2);
}

void GuildSidebar::drawChannelCategory(const char *title, int &yPos, bool collapsed, int channelCount, bool hovered) {
    Fl_Color textColor = hovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    fl_color(textColor);
    fl_font(FontLoader::Fonts::INTER_BOLD, 12);

    int textY = yPos + 19;
    const int leftMargin = 8;

    int textX = x() + leftMargin + LayoutConstants::kIndent;
    fl_draw(title, textX, textY);

    int textWidth = static_cast<int>(fl_width(title));
    const char *iconName = collapsed ? "chevron_right" : "chevron_down";
    const int iconSize = 12;
    auto *icon = IconManager::load_recolored_icon(iconName, iconSize, textColor);
    if (icon) {
        int iconX = textX + textWidth + 6;
        int iconY = yPos + 8;
        icon->draw(iconX, iconY);
    }

    yPos += 28;
}

void GuildSidebar::drawChannel(const ChannelItem &channel, int yPos, bool selected, bool hovered) {
    const int leftMargin = 8;
    const int rightMargin = 8;
    const int itemX = x() + leftMargin;
    const int itemW = w() - leftMargin - rightMargin;
    const int radius = LayoutConstants::kChannelItemCornerRadius;
    const int itemY = yPos + 2;
    const int itemH = CHANNEL_HEIGHT - 4;

    if (selected || hovered) {
        Fl_Color bgColor = selected ? ThemeColors::BG_MODIFIER_SELECTED : ThemeColors::BG_MODIFIER_HOVER;
        RoundedStyle::drawRoundedRect(itemX, itemY, itemW, itemH, radius, radius, radius, radius, bgColor);
    }

    Fl_Color iconColor = (selected || hovered) ? ThemeColors::TEXT_NORMAL : ThemeColors::CHANNEL_ICON;

    const char *iconName = "channel_text";

    if (!m_rulesChannelId.empty() && channel.id == m_rulesChannelId) {
        iconName = "channel_rules";
    } else {
        switch (channel.type) {
        case 0:
            iconName = "channel_text";
            break;
        case 2:
            iconName = "channel_voice";
            break;
        case 5:
            iconName = "channel_announcement";
            break;
        case 13:
            iconName = "channel_voice";
            break;
        case 15:
            iconName = "channel_forum";
            break;
        default:
            iconName = "channel_text";
            break;
        }
    }

    auto *icon = IconManager::load_recolored_icon(iconName, LayoutConstants::kChannelIconSize, iconColor);
    if (icon) {
        int iconX = itemX + LayoutConstants::kChannelIconPadding;
        int iconY = itemY + (itemH - LayoutConstants::kChannelIconSize) / 2;
        icon->draw(iconX, iconY);
    }

    fl_color(iconColor);
    fl_font(FontLoader::Fonts::INTER_MEDIUM, 16);
    int textX = itemX + LayoutConstants::kChannelTextPadding;
    int textY = itemY + (itemH / 2) + 6;
    int rightPadding = (channel.hasUnread && !selected) ? 24 : 12;
    int maxTextWidth = itemX + itemW - rightPadding - textX;
    std::string label = ellipsizeText(channel.name, maxTextWidth);
    fl_draw(label.c_str(), textX, textY);

    if (channel.hasUnread && !selected) {
        fl_color(FL_WHITE);
        fl_pie(itemX + itemW - 16, itemY + (itemH / 2) - 4, 8, 8, 0, 360);
    }
}

int GuildSidebar::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        int categoryIndex = getCategoryAt(Fl::event_x(), Fl::event_y());
        if (categoryIndex >= 0 && categoryIndex < static_cast<int>(m_categories.size())) {
            m_categories[categoryIndex].collapsed = !m_categories[categoryIndex].collapsed;
            s_guildCategoryCollapsedState[m_guildId][m_categories[categoryIndex].id] =
                m_categories[categoryIndex].collapsed;
            redraw();
            return 1;
        }

        std::string categoryId;
        int index = getChannelAt(Fl::event_x(), Fl::event_y(), categoryId);

        if (index >= 0) {
            const auto *channels = categoryId.empty() ? &m_uncategorizedChannels : nullptr;

            if (!categoryId.empty()) {
                for (auto &cat : m_categories) {
                    if (cat.id == categoryId) {
                        channels = &cat.channels;
                        break;
                    }
                }
            }

            if (channels && index < static_cast<int>(channels->size())) {
                m_selectedChannelId = (*channels)[index].id;
                if (m_onChannelSelected) {
                    m_onChannelSelected(m_selectedChannelId);
                }
                redraw();
                return 1;
            }
        }
        break;
    }

    case FL_MOVE:
    case FL_ENTER:
    case FL_LEAVE: {
        if (event == FL_LEAVE) {
            if (m_hoveredChannelIndex != -1 || !m_hoveredCategoryId.empty()) {
                m_hoveredChannelIndex = -1;
                m_hoveredCategoryId.clear();
                redraw();
            }
            if (m_bannerHovered) {
                m_bannerHovered = false;
                if (m_bannerHasPlayedOnce) {
                    stopBannerAnimation();
                }
            }
            return 1;
        }

        int mouseY = Fl::event_y();
        int bannerHeight = getHeaderHeight();
        bool overBanner = !m_bannerHash.empty() && mouseY >= y() && mouseY < y() + bannerHeight;

        if (overBanner != m_bannerHovered) {
            m_bannerHovered = overBanner;
            if (m_bannerHovered && m_bannerHasPlayedOnce && m_isAnimatedBanner && m_bannerGif &&
                m_bannerGif->isAnimated()) {
                startBannerAnimation();
            } else if (!m_bannerHovered && m_bannerHasPlayedOnce) {
                stopBannerAnimation();
            }
        }

        int categoryIndex = getCategoryAt(Fl::event_x(), Fl::event_y());
        if (categoryIndex >= 0 && categoryIndex < static_cast<int>(m_categories.size())) {
            std::string newCategoryId = m_categories[categoryIndex].id;
            if (m_hoveredChannelIndex != -1 || m_hoveredCategoryId != newCategoryId) {
                m_hoveredChannelIndex = -1;
                m_hoveredCategoryId = newCategoryId;
                redraw();
            }
            return 1;
        }

        std::string categoryId;
        int newHovered = getChannelAt(Fl::event_x(), Fl::event_y(), categoryId);

        if (newHovered != m_hoveredChannelIndex || categoryId != m_hoveredCategoryId) {
            m_hoveredChannelIndex = newHovered;
            m_hoveredCategoryId = categoryId;
            redraw();
        }
        return 1;
    }

    case FL_MOUSEWHEEL: {
        int dy = Fl::event_dy();
        m_scrollOffset += dy * 20;

        int contentHeight = calculateContentHeight();
        int maxScrollOffset = std::max(0, contentHeight - h());

        if (m_scrollOffset < 0)
            m_scrollOffset = 0;
        if (m_scrollOffset > maxScrollOffset)
            m_scrollOffset = maxScrollOffset;

        redraw();
        return 1;
    }
    }

    return Fl_Group::handle(event);
}

void GuildSidebar::setGuild(const std::string &guildId, const std::string &guildName) {
    m_guildId = guildId;
    m_guildName = guildName;
    redraw();
}

void GuildSidebar::setBannerUrl(const std::string &bannerUrl) {
    if (m_bannerUrl != bannerUrl) {
        m_bannerUrl = bannerUrl;
        m_bannerImage = nullptr;
        if (!m_bannerUrl.empty()) {
            loadBannerImage();
        }
        redraw();
    }
}

void GuildSidebar::setBoostInfo(int premiumTier, int subscriptionCount) {
    m_premiumTier = premiumTier;
    m_subscriptionCount = subscriptionCount;
    redraw();
}

void GuildSidebar::loadBannerImage() {
    if (m_bannerUrl.empty())
        return;

    m_bannerHasPlayedOnce = false;
    m_bannerHovered = false;

    Images::loadImageAsync(m_bannerUrl, [this](Fl_RGB_Image *image) {
        if (image) {
            m_bannerImage = image;

            if (m_isAnimatedBanner && ensureBannerGifLoaded()) {
                startBannerAnimation();
            }

            Fl::awake(
                [](void *data) {
                    GuildSidebar *sidebar = static_cast<GuildSidebar *>(data);
                    sidebar->redraw();
                },
                this);
        }
    });
}

bool GuildSidebar::ensureBannerGifLoaded() {
    if (!m_isAnimatedBanner)
        return false;
    if (m_bannerGif)
        return true;

    std::string gifPath = Images::getCacheFilePath(m_bannerUrl, "gif");

    if (!std::filesystem::exists(gifPath)) {
        Logger::debug("GIF not yet cached for animated banner: " + m_guildId);
        return false;
    }

    try {
        m_bannerGif = std::make_unique<GifAnimation>(gifPath, GifAnimation::ScalingStrategy::Lazy);
        if (!m_bannerGif->isValid()) {
            Logger::warn("Failed to load GIF animation from: " + gifPath);
            m_bannerGif.reset();
            return false;
        }
        Logger::debug("Loaded GIF animation for guild banner: " + m_guildId);
        return true;
    } catch (const std::exception &e) {
        Logger::error("Exception loading GIF animation: " + std::string(e.what()));
        m_bannerGif.reset();
        return false;
    }
}

bool GuildSidebar::updateBannerAnimation() {
    if (!m_bannerGif || !m_bannerGif->isAnimated())
        return false;

    m_frameTimeAccumulated += AnimationManager::get().getFrameTime();
    double requiredDelay = m_bannerGif->currentDelay() / 1000.0;

    if (m_frameTimeAccumulated >= requiredDelay) {
        size_t currentFrame = m_bannerGif->getCurrentFrameIndex();
        m_bannerGif->nextFrame();
        m_frameTimeAccumulated = 0.0;

        if (currentFrame == m_bannerGif->frameCount() - 1 && m_bannerGif->getCurrentFrameIndex() == 0) {
            m_bannerHasPlayedOnce = true;
            if (!m_bannerHovered) {
                stopBannerAnimation();
                redraw();
                return false;
            }
        }

        redraw();
    }

    return true;
}

void GuildSidebar::startBannerAnimation() {
    if (!m_bannerGif || !m_bannerGif->isAnimated() || m_bannerAnimationId != 0)
        return;

    auto &animMgr = AnimationManager::get();
    m_bannerAnimationId = animMgr.registerAnimation([this]() { return updateBannerAnimation(); });
}

void GuildSidebar::stopBannerAnimation() {
    if (m_bannerAnimationId != 0) {
        AnimationManager::get().unregisterAnimation(m_bannerAnimationId);
        m_bannerAnimationId = 0;
    }
}

void GuildSidebar::addTextChannel(const std::string &channelId, const std::string &channelName) {
    ChannelItem item;
    item.id = channelId;
    item.name = channelName;
    item.isVoice = false;
    m_uncategorizedChannels.push_back(item);
    redraw();
}

void GuildSidebar::addVoiceChannel(const std::string &channelId, const std::string &channelName) {
    ChannelItem item;
    item.id = channelId;
    item.name = channelName;
    item.isVoice = true;
    m_uncategorizedChannels.push_back(item);
    redraw();
}

void GuildSidebar::clearChannels() {
    m_categories.clear();
    m_uncategorizedChannels.clear();
    m_selectedChannelId.clear();
    redraw();
}

void GuildSidebar::setSelectedChannel(const std::string &channelId) {
    if (m_selectedChannelId != channelId) {
        m_selectedChannelId = channelId;
        redraw();
    }
}

int GuildSidebar::getChannelAt(int mx, int my, std::string &categoryId) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    categoryId.clear();

    for (size_t i = 0; i < m_uncategorizedChannels.size(); ++i) {
        int yPos = m_uncategorizedChannels[i].yPos;
        if (my >= yPos && my < yPos + CHANNEL_HEIGHT) {
            return static_cast<int>(i);
        }
    }

    for (const auto &category : m_categories) {
        if (!category.collapsed) {
            for (size_t i = 0; i < category.channels.size(); ++i) {
                int yPos = category.channels[i].yPos;
                if (my >= yPos && my < yPos + CHANNEL_HEIGHT) {
                    categoryId = category.id;
                    return static_cast<int>(i);
                }
            }
        }
    }

    return -1;
}

void GuildSidebar::setGuildId(const std::string &guildId) {
    m_guildId = guildId;

    auto state = Store::get().snapshot();
    for (const auto &guild : state.guilds) {
        if (guild.id == m_guildId) {
            m_guildName = guild.name;
            m_premiumTier = guild.premiumTier;
            m_subscriptionCount = guild.premiumSubscriptionCount;
            m_rulesChannelId = guild.rulesChannelId;
            if (!guild.banner.empty()) {
                m_bannerHash = guild.banner;
                m_isAnimatedBanner = !guild.banner.empty() && guild.banner.rfind("a_", 0) == 0;
                std::string bannerUrl = CDNUtils::getGuildBannerUrl(guild.id, guild.banner, 512);
                setBannerUrl(bannerUrl);
            } else {
                m_bannerHash.clear();
                m_bannerUrl.clear();
                m_bannerImage = nullptr;
                m_isAnimatedBanner = false;
                m_bannerHasPlayedOnce = false;
                m_bannerHovered = false;
                stopBannerAnimation();
                m_bannerGif.reset();
            }
            break;
        }
    }

    clearChannels();
    loadChannelsFromStore();
    redraw();
}

void GuildSidebar::loadChannelsFromStore() {
    auto state = Store::get().snapshot();

    m_categories.clear();
    m_uncategorizedChannels.clear();
    m_hoveredChannelIndex = -1;
    m_hoveredCategoryId.clear();

    auto it = state.guildChannels.find(m_guildId);
    if (it == state.guildChannels.end()) {
        Logger::warn("No channels found for guild " + m_guildId);
        return;
    }

    const auto &channels = it->second;

    std::vector<std::string> userRoleIds;
    auto memberIt = state.guildMembers.find(m_guildId);
    if (memberIt != state.guildMembers.end()) {
        userRoleIds = memberIt->second.roleIds;
        Logger::debug("User has " + std::to_string(userRoleIds.size()) + " roles in guild " + m_guildId);
    }

    std::vector<Role> guildRoles;
    auto rolesIt = state.guildRoles.find(m_guildId);
    if (rolesIt != state.guildRoles.end()) {
        guildRoles = rolesIt->second;
    }

    uint64_t basePermissions = PermissionUtils::computeBasePermissions(m_guildId, userRoleIds, guildRoles);
    Logger::debug("User base permissions: " + std::to_string(basePermissions));

    std::map<std::string, CategoryItem> categoriesMap;
    std::map<std::string, bool> categoryPermissions;
    std::vector<std::shared_ptr<GuildChannel>> regularChannels;

    for (const auto &channel : channels) {
        if (!channel || !channel->name.has_value())
            continue;

        if (channel->type == ChannelType::GUILD_CATEGORY) {
            bool canView =
                PermissionUtils::canViewChannel(m_guildId, userRoleIds, channel->permissionOverwrites, basePermissions);
            categoryPermissions[channel->id] = canView;

            CategoryItem cat;
            cat.id = channel->id;
            cat.name = *channel->name;
            cat.collapsed = false;
            auto guildStateIt = s_guildCategoryCollapsedState.find(m_guildId);
            if (guildStateIt != s_guildCategoryCollapsedState.end()) {
                auto categoryStateIt = guildStateIt->second.find(channel->id);
                if (categoryStateIt != guildStateIt->second.end()) {
                    cat.collapsed = categoryStateIt->second;
                }
            }
            cat.position = channel->position;
            categoriesMap[channel->id] = cat;
        } else {
            regularChannels.push_back(channel);
        }
    }

    for (const auto &channel : regularChannels) {
        bool canView =
            PermissionUtils::canViewChannel(m_guildId, userRoleIds, channel->permissionOverwrites, basePermissions);
        if (!canView) {
            continue;
        }

        ChannelItem item;
        item.id = channel->id;
        item.name = *channel->name;
        item.type = static_cast<int>(channel->type);
        item.isVoice = channel->isVoice();
        item.hasUnread = false;

        if (channel->parentId.has_value() && !channel->parentId->empty()) {
            auto catIt = categoriesMap.find(*channel->parentId);
            if (catIt != categoriesMap.end()) {
                catIt->second.channels.push_back(item);
            } else {
                m_uncategorizedChannels.push_back(item);
            }
        } else {
            m_uncategorizedChannels.push_back(item);
        }
    }

    for (auto &[catId, cat] : categoriesMap) {
        std::sort(cat.channels.begin(), cat.channels.end(), [&](const ChannelItem &a, const ChannelItem &b) {
            int posA = 0, posB = 0;
            for (const auto &ch : channels) {
                if (ch->id == a.id)
                    posA = ch->position;
                if (ch->id == b.id)
                    posB = ch->position;
            }
            return posA < posB;
        });
    }

    std::sort(m_uncategorizedChannels.begin(), m_uncategorizedChannels.end(),
              [&](const ChannelItem &a, const ChannelItem &b) {
                  int posA = 0, posB = 0;
                  for (const auto &ch : channels) {
                      if (ch->id == a.id)
                          posA = ch->position;
                      if (ch->id == b.id)
                          posB = ch->position;
                  }
                  return posA < posB;
              });

    for (auto &[catId, cat] : categoriesMap) {
        m_categories.push_back(cat);
    }

    std::sort(m_categories.begin(), m_categories.end(),
              [](const CategoryItem &a, const CategoryItem &b) { return a.position < b.position; });

    m_categories.erase(std::remove_if(m_categories.begin(), m_categories.end(),
                                      [](const CategoryItem &cat) { return cat.channels.empty(); }),
                       m_categories.end());

    Logger::info("Loaded " + std::to_string(m_categories.size()) + " categories and " +
                 std::to_string(m_uncategorizedChannels.size()) + " uncategorized channels for guild " + m_guildId);
}

int GuildSidebar::getCategoryAt(int mx, int my) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    for (size_t i = 0; i < m_categories.size(); ++i) {
        int yPos = m_categories[i].yPos;
        if (my >= yPos && my < yPos + 28) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

int GuildSidebar::calculateContentHeight() const {
    int totalHeight = getHeaderHeight();

    totalHeight += 16;
    totalHeight += m_uncategorizedChannels.size() * CHANNEL_HEIGHT;

    if (!m_uncategorizedChannels.empty()) {
        totalHeight += 16;
    }

    for (const auto &category : m_categories) {
        totalHeight += 28;
        if (!category.collapsed) {
            totalHeight += category.channels.size() * CHANNEL_HEIGHT;
        }
        totalHeight += 16;
    }

    totalHeight += 16;
    return totalHeight;
}
