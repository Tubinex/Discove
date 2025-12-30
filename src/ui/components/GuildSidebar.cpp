#include "ui/components/GuildSidebar.h"

#include "state/AppState.h"
#include "state/Store.h"
#include "ui/Theme.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

GuildSidebar::GuildSidebar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    end();
}

GuildSidebar::~GuildSidebar() {}

void GuildSidebar::draw() {
    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), h());

    drawServerHeader();
    int currentY = y() + HEADER_HEIGHT - m_scrollOffset;

    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FL_HELVETICA, 11);
    fl_draw("Events", x() + INDENT, currentY + 18);
    currentY += CATEGORY_HEIGHT;

    fl_draw("Server Boosts", x() + INDENT, currentY + 18);
    currentY += CATEGORY_HEIGHT;

    int textChannelCount = static_cast<int>(m_textChannels.size());
    drawChannelCategory("TEXT CHANNELS", currentY, m_textChannelsCollapsed, textChannelCount);

    if (!m_textChannelsCollapsed) {
        for (size_t i = 0; i < m_textChannels.size(); ++i) {
            auto &channel = m_textChannels[i];
            channel.yPos = currentY;

            bool selected = (channel.id == m_selectedChannelId);
            bool hovered = (m_hoveredChannelIndex == static_cast<int>(i) && !m_hoveredIsVoice);

            drawChannel(channel, currentY, selected, hovered);
            currentY += CHANNEL_HEIGHT;
        }
    }

    int voiceChannelCount = static_cast<int>(m_voiceChannels.size());
    drawChannelCategory("VOICE CHANNELS", currentY, m_voiceChannelsCollapsed, voiceChannelCount);

    if (!m_voiceChannelsCollapsed) {
        for (size_t i = 0; i < m_voiceChannels.size(); ++i) {
            auto &channel = m_voiceChannels[i];
            channel.yPos = currentY;

            bool selected = (channel.id == m_selectedChannelId);
            bool hovered = (m_hoveredChannelIndex == static_cast<int>(i) && m_hoveredIsVoice);

            drawChannel(channel, currentY, selected, hovered);
            currentY += CHANNEL_HEIGHT;
        }
    }
}

void GuildSidebar::drawServerHeader() {
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FL_HELVETICA_BOLD, 15);

    int textY = y() + 18;
    fl_draw(m_guildName.c_str(), x() + 16, textY);

    fl_font(FL_HELVETICA, 12);
    fl_draw("▼", x() + w() - 24, textY);

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y() + HEADER_HEIGHT - 1, x() + w(), y() + HEADER_HEIGHT - 1);
}

void GuildSidebar::drawChannelCategory(const char *title, int &yPos, bool collapsed, int channelCount) {
    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FL_HELVETICA_BOLD, 11);

    const char *arrow = collapsed ? "▸" : "▾";
    fl_draw(arrow, x() + INDENT, yPos + 18);

    fl_draw(title, x() + INDENT + 16, yPos + 18);
    yPos += CATEGORY_HEIGHT;
}

void GuildSidebar::drawChannel(const ChannelItem &channel, int yPos, bool selected, bool hovered) {
    if (selected || hovered) {
        if (selected) {
            fl_color(ThemeColors::BG_MODIFIER_SELECTED);
        } else {
            fl_color(ThemeColors::BG_MODIFIER_HOVER);
        }
        fl_rectf(x() + 4, yPos, w() - 8, CHANNEL_HEIGHT);
    }

    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FL_HELVETICA, 16);

    if (channel.isVoice) {
        fl_draw("🔊", x() + INDENT + 8, yPos + 20);
    } else {
        fl_draw("#", x() + INDENT + 8, yPos + 20);
    }

    if (selected) {
        fl_color(FL_WHITE);
    } else if (hovered) {
        fl_color(ThemeColors::TEXT_NORMAL);
    } else {
        fl_color(ThemeColors::TEXT_MUTED);
    }
    fl_font(FL_HELVETICA, 14);

    fl_draw(channel.name.c_str(), x() + INDENT + 32, yPos + 20);
    if (channel.hasUnread && !selected) {
        fl_color(FL_WHITE);
        fl_pie(x() + w() - 20, yPos + CHANNEL_HEIGHT / 2 - 4, 8, 8, 0, 360);
    }
}

int GuildSidebar::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        bool isVoice = false;
        int index = getChannelAt(Fl::event_x(), Fl::event_y(), isVoice);

        if (index >= 0) {
            const auto &channels = isVoice ? m_voiceChannels : m_textChannels;
            if (index < static_cast<int>(channels.size())) {
                m_selectedChannelId = channels[index].id;
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
        bool isVoice = false;
        int newHovered = (event == FL_LEAVE) ? -1 : getChannelAt(Fl::event_x(), Fl::event_y(), isVoice);

        if (newHovered != m_hoveredChannelIndex || isVoice != m_hoveredIsVoice) {
            m_hoveredChannelIndex = newHovered;
            m_hoveredIsVoice = isVoice;
            redraw();
        }
        return 1;
    }

    case FL_MOUSEWHEEL: {
        int dy = Fl::event_dy();
        m_scrollOffset += dy * 20;

        if (m_scrollOffset < 0)
            m_scrollOffset = 0;

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
    m_textChannels.push_back(item);
    redraw();
}

void GuildSidebar::addVoiceChannel(const std::string &channelId, const std::string &channelName) {
    ChannelItem item;
    item.id = channelId;
    item.name = channelName;
    item.isVoice = true;
    m_voiceChannels.push_back(item);
    redraw();
}

void GuildSidebar::clearChannels() {
    m_textChannels.clear();
    m_voiceChannels.clear();
    m_selectedChannelId.clear();
    redraw();
}

void GuildSidebar::setSelectedChannel(const std::string &channelId) {
    if (m_selectedChannelId != channelId) {
        m_selectedChannelId = channelId;
        redraw();
    }
}

int GuildSidebar::getChannelAt(int mx, int my, bool &isVoice) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    for (size_t i = 0; i < m_textChannels.size(); ++i) {
        int yPos = m_textChannels[i].yPos;
        if (my >= yPos && my < yPos + CHANNEL_HEIGHT) {
            isVoice = false;
            return static_cast<int>(i);
        }
    }

    for (size_t i = 0; i < m_voiceChannels.size(); ++i) {
        int yPos = m_voiceChannels[i].yPos;
        if (my >= yPos && my < yPos + CHANNEL_HEIGHT) {
            isVoice = true;
            return static_cast<int>(i);
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
    addTextChannel("1", "general");
    addTextChannel("2", "random");
    addTextChannel("3", "announcements");
    addVoiceChannel("4", "General Voice");
    addVoiceChannel("5", "Gaming");

    redraw();
}

void GuildSidebar::loadChannelsFromStore() {}
