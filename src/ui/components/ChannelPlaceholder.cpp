#include "ui/components/ChannelPlaceholder.h"

#include "ui/Theme.h"

#include <FL/fl_draw.H>

ChannelPlaceholder::ChannelPlaceholder(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {
    box(FL_NO_BOX);
}

void ChannelPlaceholder::draw() {
    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), y(), w(), h());

    fl_color(ThemeColors::TEXT_MUTED);
    fl_font(FL_HELVETICA, 14);

    int textWidth = static_cast<int>(fl_width(m_text.c_str()));
    int textX = x() + (w() - textWidth) / 2;
    int textY = y() + h() / 2;

    fl_draw(m_text.c_str(), textX, textY);
}
