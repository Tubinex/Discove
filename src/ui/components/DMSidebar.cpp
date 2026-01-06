#include "ui/components/DMSidebar.h"

#include "state/AppState.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

DMSidebar::DMSidebar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    clip_children(1);
    end();

    m_storeListenerId = Store::get().subscribe<std::vector<std::shared_ptr<DMChannel>>>(
        [](const AppState &state) { return state.privateChannels; },
        [this](const std::vector<std::shared_ptr<DMChannel>> &channels) {
            Fl::lock();
            loadDMsFromState();
            redraw();
            Fl::unlock();
            Fl::awake();
        },
        [](const std::vector<std::shared_ptr<DMChannel>> &a, const std::vector<std::shared_ptr<DMChannel>> &b) {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i]->id != b[i]->id)
                    return false;
            }
            return true;
        },
        true);
}

DMSidebar::~DMSidebar() {
    if (m_storeListenerId) {
        Store::get().unsubscribe(m_storeListenerId);
    }
}

void DMSidebar::draw() {
    if (!damage() || damage() == FL_DAMAGE_CHILD) {
        return;
    }

    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), h());

    drawHeader();
    int currentY = y() + HEADER_HEIGHT - m_scrollOffset;
    for (size_t i = 0; i < m_dms.size(); ++i) {
        auto &dm = m_dms[i];
        dm.yPos = currentY;

        bool selected = (dm.id == m_selectedDMId);
        bool hovered = (m_hoveredDMIndex == static_cast<int>(i));

        drawDM(dm, currentY, selected, hovered);
        currentY += DM_HEIGHT;
    }

    fl_pop_clip();
}

void DMSidebar::drawHeader() {
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_BOLD, 15);

    int textY = y() + 18;
    fl_draw("Direct Messages", x() + 16, textY);

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y() + HEADER_HEIGHT - 1, x() + w(), y() + HEADER_HEIGHT - 1);
}

void DMSidebar::drawDM(const DMItem &dm, int yPos, bool selected, bool hovered) {
    if (selected) {
        fl_color(ThemeColors::BG_MODIFIER_SELECTED);
        fl_rectf(x() + 8, yPos, w() - 16, DM_HEIGHT);
    } else if (hovered) {
        fl_color(ThemeColors::BG_MODIFIER_HOVER);
        fl_rectf(x() + 8, yPos, w() - 16, DM_HEIGHT);
    }

    int avatarSize = 32;
    int avatarX = x() + 12;
    int avatarY = yPos + (DM_HEIGHT - avatarSize) / 2;

    fl_color(ThemeColors::BG_TERTIARY);
    fl_pie(avatarX, avatarY, avatarSize, avatarSize, 0, 360);

    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FL_HELVETICA, 14);
    fl_draw(dm.username.c_str(), avatarX + avatarSize + 12, yPos + DM_HEIGHT / 2 + 5);

    int statusSize = 10;
    int statusX = avatarX + avatarSize - statusSize + 2;
    int statusY = avatarY + avatarSize - statusSize + 2;

    if (dm.status == "online") {
        fl_color(ThemeColors::STATUS_ONLINE);
    } else if (dm.status == "idle") {
        fl_color(ThemeColors::STATUS_IDLE);
    } else if (dm.status == "dnd") {
        fl_color(ThemeColors::STATUS_DND);
    } else {
        fl_color(ThemeColors::STATUS_OFFLINE);
    }
    fl_pie(statusX, statusY, statusSize, statusSize, 0, 360);

    fl_color(ThemeColors::BG_SECONDARY);
    fl_arc(statusX, statusY, statusSize, statusSize, 0, 360);
}

int DMSidebar::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            int mx = Fl::event_x();
            int my = Fl::event_y();
            int dmIndex = getDMAt(mx, my);

            if (dmIndex >= 0 && dmIndex < static_cast<int>(m_dms.size())) {
                if (m_onDMSelected) {
                    m_onDMSelected(m_dms[dmIndex].id);
                }
                return 1;
            }
        }
        break;
    }

    case FL_MOVE: {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        int newHovered = getDMAt(mx, my);

        if (newHovered != m_hoveredDMIndex) {
            m_hoveredDMIndex = newHovered;
            redraw();
        }
        return 1;
    }

    case FL_LEAVE:
        if (m_hoveredDMIndex != -1) {
            m_hoveredDMIndex = -1;
            redraw();
        }
        return 1;

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

void DMSidebar::setSelectedDM(const std::string &dmId) {
    if (m_selectedDMId != dmId) {
        m_selectedDMId = dmId;
        redraw();
    }
}

int DMSidebar::getDMAt(int mx, int my) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    for (size_t i = 0; i < m_dms.size(); ++i) {
        int yPos = m_dms[i].yPos;
        if (my >= yPos && my < yPos + DM_HEIGHT) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void DMSidebar::loadDMsFromState() {
    m_dms.clear();

    AppState state = Store::get().snapshot();
    for (const auto &channel : state.privateChannels) {
        if (!channel)
            continue;

        DMItem item;
        item.id = channel->id;

        if (channel->name.has_value() && !channel->name->empty()) {
            item.username = *channel->name;
        } else if (!channel->recipientIds.empty()) {
            item.username = "DM " + channel->recipientIds[0].substr(0, 8);
        } else {
            item.username = "Unknown DM";
        }

        item.avatarUrl = "";
        item.status = "offline";

        m_dms.push_back(item);
    }
}
