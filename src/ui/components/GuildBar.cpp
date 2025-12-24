#include "ui/components/GuildBar.h"

#include "ui/Theme.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

GuildBar::GuildBar(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {
    box(FL_NO_BOX);
}

void GuildBar::draw() {
    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), y(), w(), h());

    int currentY = y() + 8 - m_scrollOffset;
    for (size_t i = 0; i < m_guilds.size(); ++i) {
        const auto &guild = m_guilds[i];
        bool selected = (guild.id == m_selectedGuildId);
        bool hovered = (static_cast<int>(i) == m_hoveredIndex);

        drawGuildIcon(guild, currentY, selected, hovered);

        currentY += GUILD_ICON_SIZE + GUILD_SPACING;
    }
}

void GuildBar::drawGuildIcon(const GuildItem &guild, int yPos, bool selected, bool hovered) {
    int centerX = x() + w() / 2;
    int iconX = centerX - GUILD_ICON_SIZE / 2;

    if (selected || hovered) {
        fl_color(FL_WHITE);
        int barHeight = selected ? GUILD_ICON_SIZE : (hovered ? GUILD_ICON_SIZE / 2 : 0);
        int barY = yPos + (GUILD_ICON_SIZE - barHeight) / 2;
        fl_rectf(x(), barY, 4, barHeight);
    }

    if (guild.hasUnread && !selected) {
        fl_color(FL_WHITE);
        fl_pie(x() + 2, yPos + GUILD_ICON_SIZE / 2 - 4, 8, 8, 0, 360);
    }

    int borderRadius = (selected || hovered) ? GUILD_ICON_SIZE / 3 : GUILD_ICON_SIZE / 2;
    fl_color(ThemeColors::BG_SECONDARY);

    fl_pie(iconX, yPos, borderRadius * 2, borderRadius * 2, 90, 180);
    fl_pie(iconX + GUILD_ICON_SIZE - borderRadius * 2, yPos, borderRadius * 2, borderRadius * 2, 0, 90);
    fl_pie(iconX, yPos + GUILD_ICON_SIZE - borderRadius * 2, borderRadius * 2, borderRadius * 2, 180, 270);
    fl_pie(iconX + GUILD_ICON_SIZE - borderRadius * 2, yPos + GUILD_ICON_SIZE - borderRadius * 2,
           borderRadius * 2, borderRadius * 2, 270, 360);

    fl_rectf(iconX + borderRadius, yPos, GUILD_ICON_SIZE - borderRadius * 2, GUILD_ICON_SIZE);
    fl_rectf(iconX, yPos + borderRadius, GUILD_ICON_SIZE, GUILD_ICON_SIZE - borderRadius * 2);

    // TODO: Load and draw actual guild icon image
    if (!guild.name.empty()) {
        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FL_HELVETICA_BOLD, 20);

        std::string initial(1, guild.name[0]);
        int textW = static_cast<int>(fl_width(initial.c_str()));
        int textH = fl_height();

        fl_draw(initial.c_str(),
                iconX + (GUILD_ICON_SIZE - textW) / 2,
                yPos + (GUILD_ICON_SIZE + textH) / 2 - fl_descent());
    }
}

int GuildBar::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        int index = getGuildIndexAt(Fl::event_x(), Fl::event_y());
        if (index >= 0 && index < static_cast<int>(m_guilds.size())) {
            m_selectedGuildId = m_guilds[index].id;
            if (m_onGuildSelected) {
                m_onGuildSelected(m_selectedGuildId);
            }
            redraw();
            return 1;
        }
        break;
    }

    case FL_MOVE:
    case FL_ENTER:
    case FL_LEAVE: {
        int newHovered = (event == FL_LEAVE) ? -1 : getGuildIndexAt(Fl::event_x(), Fl::event_y());
        if (newHovered != m_hoveredIndex) {
            m_hoveredIndex = newHovered;
            redraw();
        }
        return 1;
    }

    case FL_MOUSEWHEEL: {
        int dy = Fl::event_dy();
        m_scrollOffset += dy * 20;

        int maxScroll = static_cast<int>(m_guilds.size()) * (GUILD_ICON_SIZE + GUILD_SPACING);
        if (m_scrollOffset < 0) m_scrollOffset = 0;
        if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;

        redraw();
        return 1;
    }
    }

    return Fl_Widget::handle(event);
}

void GuildBar::addGuild(const std::string &guildId, const std::string &iconUrl, const std::string &name) {
    GuildItem item;
    item.id = guildId;
    item.iconUrl = iconUrl;
    item.name = name;
    item.y = static_cast<int>(m_guilds.size()) * (GUILD_ICON_SIZE + GUILD_SPACING);
    m_guilds.push_back(item);
    redraw();
}

void GuildBar::setSelectedGuild(const std::string &guildId) {
    if (m_selectedGuildId != guildId) {
        m_selectedGuildId = guildId;
        redraw();
    }
}

int GuildBar::getGuildIndexAt(int mx, int my) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    int currentY = y() + 8 - m_scrollOffset;

    for (size_t i = 0; i < m_guilds.size(); ++i) {
        if (my >= currentY && my < currentY + GUILD_ICON_SIZE) {
            return static_cast<int>(i);
        }
        currentY += GUILD_ICON_SIZE + GUILD_SPACING;
    }

    return -1;
}
 