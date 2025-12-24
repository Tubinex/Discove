#include "ui/components/ProfileBubble.h"

#include "ui/Theme.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

ProfileBubble::ProfileBubble(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {
    box(FL_NO_BOX);
}

void ProfileBubble::draw() {
    // Floating effect: 8px margin on all sides
    const int MARGIN = 8;
    const int BORDER_RADIUS = 8;

    int bubbleX = x() + MARGIN;
    int bubbleY = y() + MARGIN;
    int bubbleW = w() - (MARGIN * 2);
    int bubbleH = h() - (MARGIN * 2);

    // Draw rounded rectangle background
    fl_color(ThemeColors::BG_SECONDARY_ALT);

    // Draw four corner arcs
    fl_pie(bubbleX, bubbleY, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 90, 180);
    fl_pie(bubbleX + bubbleW - BORDER_RADIUS * 2, bubbleY, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 0, 90);
    fl_pie(bubbleX, bubbleY + bubbleH - BORDER_RADIUS * 2, BORDER_RADIUS * 2, BORDER_RADIUS * 2, 180, 270);
    fl_pie(bubbleX + bubbleW - BORDER_RADIUS * 2, bubbleY + bubbleH - BORDER_RADIUS * 2,
           BORDER_RADIUS * 2, BORDER_RADIUS * 2, 270, 360);

    // Fill center areas
    fl_rectf(bubbleX + BORDER_RADIUS, bubbleY, bubbleW - BORDER_RADIUS * 2, bubbleH);
    fl_rectf(bubbleX, bubbleY + BORDER_RADIUS, bubbleW, bubbleH - BORDER_RADIUS * 2);

    // Avatar
    int avatarX = bubbleX + 8;
    int avatarY = bubbleY + (bubbleH - AVATAR_SIZE) / 2;
    drawAvatar(avatarX, avatarY);

    // Status dot on avatar
    int dotX = avatarX + AVATAR_SIZE - STATUS_DOT_SIZE;
    int dotY = avatarY + AVATAR_SIZE - STATUS_DOT_SIZE;
    drawStatusDot(dotX, dotY);

    // Username
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FL_HELVETICA_BOLD, 13);

    int textX = avatarX + AVATAR_SIZE + 8;
    int textY = bubbleY + bubbleH / 2 - 8;

    fl_draw(m_username.c_str(), textX, textY);

    // Discriminator (if not new username system)
    if (m_discriminator != "0") {
        fl_color(ThemeColors::TEXT_MUTED);
        fl_font(FL_HELVETICA, 12);
        fl_draw(("#" + m_discriminator).c_str(), textX, textY + 16);
    }

    // Buttons on the right (microphone, headphones, settings)
    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - 4;

    // Settings button
    drawButton(buttonX, buttonY, "⚙", m_hoveredButton == 2, false);
    buttonX -= BUTTON_SIZE + 4;

    // Headphones button
    drawButton(buttonX, buttonY, "🎧", m_hoveredButton == 1, m_headphonesDeafened);
    buttonX -= BUTTON_SIZE + 4;

    // Microphone button
    drawButton(buttonX, buttonY, "🎤", m_hoveredButton == 0, m_microphoneMuted);
}

void ProfileBubble::drawAvatar(int avatarX, int avatarY) {
    // Draw circular avatar background
    fl_color(ThemeColors::BG_PRIMARY);
    fl_pie(avatarX, avatarY, AVATAR_SIZE, AVATAR_SIZE, 0, 360);

    // TODO: Load and draw actual avatar image
    // For now, draw first letter of username
    if (!m_username.empty()) {
        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FL_HELVETICA_BOLD, 16);

        std::string initial(1, m_username[0]);
        int textW = static_cast<int>(fl_width(initial.c_str()));
        int textH = fl_height();

        fl_draw(initial.c_str(),
                avatarX + (AVATAR_SIZE - textW) / 2,
                avatarY + (AVATAR_SIZE + textH) / 2 - fl_descent());
    }
}

void ProfileBubble::drawStatusDot(int dotX, int dotY) {
    // Status color
    Fl_Color statusColor;
    if (m_status == "online") {
        statusColor = fl_rgb_color(67, 181, 129); // Green
    } else if (m_status == "dnd") {
        statusColor = fl_rgb_color(240, 71, 71); // Red
    } else if (m_status == "idle") {
        statusColor = fl_rgb_color(250, 166, 26); // Yellow
    } else {
        statusColor = fl_rgb_color(116, 127, 141); // Gray (invisible/offline)
    }

    // Draw status dot
    fl_color(ThemeColors::BG_SECONDARY_ALT);
    fl_pie(dotX - 1, dotY - 1, STATUS_DOT_SIZE + 2, STATUS_DOT_SIZE + 2, 0, 360);

    fl_color(statusColor);
    fl_pie(dotX, dotY, STATUS_DOT_SIZE, STATUS_DOT_SIZE, 0, 360);
}

void ProfileBubble::drawButton(int btnX, int btnY, const char *icon, bool hovered, bool active) {
    // Button background
    if (hovered) {
        fl_color(ThemeColors::BG_MODIFIER_HOVER);
        fl_pie(btnX, btnY, BUTTON_SIZE, BUTTON_SIZE, 0, 360);
    }

    // Icon
    if (active) {
        fl_color(fl_rgb_color(240, 71, 71)); // Red when active (muted/deafened)
    } else {
        fl_color(ThemeColors::TEXT_MUTED);
    }

    fl_font(FL_HELVETICA, 18);

    int textW = static_cast<int>(fl_width(icon));
    int textH = fl_height();

    fl_draw(icon,
            btnX + (BUTTON_SIZE - textW) / 2,
            btnY + (BUTTON_SIZE + textH) / 2 - fl_descent());
}

int ProfileBubble::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        int button = getButtonAt(Fl::event_x(), Fl::event_y());

        if (button == 0 && m_onMicrophoneClicked) {
            m_microphoneMuted = !m_microphoneMuted;
            m_onMicrophoneClicked();
            redraw();
            return 1;
        } else if (button == 1 && m_onHeadphonesClicked) {
            m_headphonesDeafened = !m_headphonesDeafened;
            m_onHeadphonesClicked();
            redraw();
            return 1;
        } else if (button == 2 && m_onSettingsClicked) {
            m_onSettingsClicked();
            return 1;
        }
        break;
    }

    case FL_MOVE:
    case FL_ENTER:
    case FL_LEAVE: {
        int newHovered = (event == FL_LEAVE) ? -1 : getButtonAt(Fl::event_x(), Fl::event_y());

        if (newHovered != m_hoveredButton) {
            m_hoveredButton = newHovered;
            redraw();
        }
        return 1;
    }
    }

    return Fl_Widget::handle(event);
}

void ProfileBubble::setUser(const std::string &userId, const std::string &username, const std::string &avatarUrl,
                             const std::string &discriminator) {
    m_userId = userId;
    m_username = username;
    m_avatarUrl = avatarUrl;
    m_discriminator = discriminator;
    redraw();
}

void ProfileBubble::setStatus(const std::string &status) {
    m_status = status;
    redraw();
}

int ProfileBubble::getButtonAt(int mx, int my) const {
    const int MARGIN = 8;
    int bubbleX = x() + MARGIN;
    int bubbleY = y() + MARGIN;
    int bubbleW = w() - (MARGIN * 2);
    int bubbleH = h() - (MARGIN * 2);

    int buttonY = bubbleY + (bubbleH - BUTTON_SIZE) / 2;

    // Check from right to left: settings, headphones, microphone
    int buttonX = bubbleX + bubbleW - BUTTON_SIZE - 4;

    // Settings
    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 2;
    }

    buttonX -= BUTTON_SIZE + 4;

    // Headphones
    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 1;
    }

    buttonX -= BUTTON_SIZE + 4;

    // Microphone
    if (mx >= buttonX && mx < buttonX + BUTTON_SIZE && my >= buttonY && my < buttonY + BUTTON_SIZE) {
        return 0;
    }

    return -1;
}
