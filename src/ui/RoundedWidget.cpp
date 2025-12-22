#include "ui/RoundedWidget.h"

#include <FL/fl_draw.H>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

void RoundedStyle::setBorderRadius(int radius) {
    m_borderRadius = std::max(0, radius);
}

void RoundedStyle::drawRoundedRect(int x, int y, int w, int h, int radius, Fl_Color fillColor) const {
    int maxRadius = std::min(w, h) / 2;
    radius = std::min(radius, maxRadius);

    fl_color(fillColor);
    if (radius <= 0) {
        fl_rectf(x, y, w, h);
        return;
    }

    fl_pie(x, y, radius * 2, radius * 2, 90, 180);
    fl_pie(x + w - radius * 2, y, radius * 2, radius * 2, 0, 90);
    fl_pie(x, y + h - radius * 2, radius * 2, radius * 2, 180, 270);
    fl_pie(x + w - radius * 2, y + h - radius * 2, radius * 2, radius * 2, 270, 360);

    fl_rectf(x + radius, y, w - radius * 2, h);
    fl_rectf(x, y + radius, radius, h - radius * 2);
    fl_rectf(x + w - radius, y + radius, radius, h - radius * 2);
}
