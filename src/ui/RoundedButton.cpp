#include "ui/RoundedButton.h"

#include <FL/Fl.H>
#include <FL/Enumerations.H>
#include <FL/Fl_Group.H>
#include <FL/fl_draw.H>

RoundedButton::RoundedButton(int x, int y, int w, int h, const char *label)
    : RoundedWidget<Fl_Button>(x, y, w, h, label) {
    box(FL_NO_BOX);
    clear_visible_focus();
}

RoundedButton::~RoundedButton() {
    if (m_offscreen) {
        fl_delete_offscreen(m_offscreen);
        m_offscreen = 0;
    }
}

void RoundedButton::setPressedColor(Fl_Color color) {
    m_pressedColor = color;
    m_hasPressedColor = true;
    redraw();
}

void RoundedButton::ensureOffscreen() {
    int width = w();
    int height = h();

    if (width <= 0 || height <= 0) {
        return;
    }

    if (m_offscreen && width == m_offscreenW && height == m_offscreenH) {
        return;
    }

    if (m_offscreen) {
        fl_delete_offscreen(m_offscreen);
        m_offscreen = 0;
    }

    m_offscreen = fl_create_offscreen(width, height);
    m_offscreenW = width;
    m_offscreenH = height;
}

int RoundedButton::handle(int event) {
    switch (event) {
    case FL_ENTER:
    case FL_MOVE:
        if (!m_isHovered) {
            m_isHovered = true;
            redraw();
        }
        break;
    case FL_LEAVE:
    case FL_HIDE:
        if (m_isHovered) {
            m_isHovered = false;
            redraw();
        }
        break;
    default:
        break;
    }

    return Fl_Button::handle(event);
}

void RoundedButton::draw() {
    Fl_Color bgColor;
    Fl_Color pressedColor = m_hasPressedColor ? m_pressedColor : fl_darker(selection_color());
    if (value()) {
        bgColor = pressedColor;
    } else if (m_isHovered && active()) {
        bgColor = selection_color();
    } else {
        bgColor = color();
    }

    Fl_Color clearColor = parent() ? parent()->color() : FL_BACKGROUND_COLOR;
    ensureOffscreen();

    if (m_offscreen) {
        fl_begin_offscreen(m_offscreen);
        fl_push_no_clip();
        fl_color(clearColor);
        fl_rectf(0, 0, w(), h());
        drawRoundedRect(0, 0, w(), h(), getBorderRadius(), bgColor);
        fl_color(labelcolor());
        fl_font(labelfont(), labelsize());
        fl_draw(label(), 0, 0, w(), h(), align());
        fl_pop_clip();
        fl_end_offscreen();

        fl_copy_offscreen(x(), y(), w(), h(), m_offscreen, 0, 0);
        return;
    }

    fl_color(clearColor);
    fl_rectf(x(), y(), w(), h());
    drawRoundedRect(x(), y(), w(), h(), getBorderRadius(), bgColor);
    fl_color(labelcolor());
    fl_font(labelfont(), labelsize());
    fl_draw(label(), x(), y(), w(), h(), align());
}
