#include "ui/components/GuildSidebar.h"

#include "state/AppState.h"
#include "state/Store.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <map>

GuildSidebar::GuildSidebar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    clip_children(1);
    end();
}

GuildSidebar::~GuildSidebar() {}

void GuildSidebar::draw() {
    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), h());

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y(), x(), y() + h());

    drawServerHeader();
    int currentY = y() + HEADER_HEIGHT - m_scrollOffset;

    currentY += 16;

    for (auto &channel : m_uncategorizedChannels) {
        channel.yPos = currentY;
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
        bool categoryHovered = (m_hoveredCategoryId == category.id && m_hoveredChannelIndex == -1);
        drawChannelCategory(category.name.c_str(), currentY, category.collapsed, channelCount, categoryHovered);

        if (!category.collapsed) {
            for (size_t i = 0; i < category.channels.size(); ++i) {
                auto &channel = category.channels[i];
                channel.yPos = currentY;

                bool selected = (channel.id == m_selectedChannelId);
                bool hovered = (m_hoveredChannelIndex == static_cast<int>(i) && m_hoveredCategoryId == category.id);

                drawChannel(channel, currentY, selected, hovered);
                currentY += CHANNEL_HEIGHT;
            }
        }

        currentY += 16;
    }

    fl_pop_clip();
}

void GuildSidebar::drawServerHeader() {
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, 15);

    int textY = y() + 18;
    const int leftMargin = 8;
    fl_draw(m_guildName.c_str(), x() + leftMargin + 8, textY);

    fl_font(FontLoader::Fonts::INTER_REGULAR, 12);
    fl_draw("▼", x() + w() - 24, textY);

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y() + HEADER_HEIGHT - 1, x() + w(), y() + HEADER_HEIGHT - 1);
}

void GuildSidebar::drawChannelCategory(const char *title, int &yPos, bool collapsed, int channelCount, bool hovered) {
    Fl_Color textColor = hovered ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    fl_color(textColor);
    fl_font(FontLoader::Fonts::INTER_BOLD, 12);

    int textY = yPos + 19;
    const int leftMargin = 8;

    int textX = x() + leftMargin + INDENT;
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
    fl_draw(channel.name.c_str(), textX, textY);

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
            return 1;
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
            break;
        }
    }

    clearChannels();
    loadChannelsFromStore();
    redraw();
}

void GuildSidebar::loadChannelsFromStore() {
    auto state = Store::get().snapshot();

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
    int totalHeight = HEADER_HEIGHT;

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
