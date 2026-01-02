#include "ui/RoundedWidget.h"

#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<std::pair<double, double>> buildRoundedPoints(int X, int Y, int W, int H, int rtl, int rtr, int rbl,
                                                           int rbr) {
    rtl = std::max(0, std::min({rtl, W / 2, H / 2}));
    rtr = std::max(0, std::min({rtr, W / 2, H / 2}));
    rbl = std::max(0, std::min({rbl, W / 2, H / 2}));
    rbr = std::max(0, std::min({rbr, W / 2, H / 2}));

    std::vector<std::pair<double, double>> pts;
    pts.reserve(64);

    auto appendArc = [&](double cx, double cy, double r, double a0, double a1) {
        if (r <= 0)
            return;
        int steps = std::max(12, static_cast<int>(r * 1.5));
        double step = (a1 - a0) / steps;
        for (int i = 0; i <= steps; ++i) {
            double a = a0 + step * i;
            pts.emplace_back(std::round(cx + std::cos(a) * r), std::round(cy + std::sin(a) * r));
        }
    };

    if (rtl > 0)
        appendArc(X + rtl, Y + rtl, rtl, kPi, 1.5 * kPi);
    else
        pts.emplace_back(X, Y);

    if (rtr > 0) {
        pts.emplace_back(X + W - rtr, Y);
        appendArc(X + W - rtr, Y + rtr, rtr, 1.5 * kPi, 2.0 * kPi);
    } else {
        pts.emplace_back(X + W, Y);
    }

    if (rbr > 0) {
        pts.emplace_back(X + W, Y + H - rbr);
        appendArc(X + W - rbr, Y + H - rbr, rbr, 0.0, 0.5 * kPi);
    } else {
        pts.emplace_back(X + W, Y + H);
    }

    if (rbl > 0) {
        pts.emplace_back(X + rbl, Y + H);
        appendArc(X + rbl, Y + H - rbl, rbl, 0.5 * kPi, kPi);
    } else {
        pts.emplace_back(X, Y + H);
    }

    if (rtl == 0)
        pts.emplace_back(X, Y);
    return pts;
}
} // namespace

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

void RoundedStyle::drawRoundedRect(int X, int Y, int W, int H, int rtl, int rtr, int rbl, int rbr, Fl_Color fill) {
    fl_color(fill);
    auto pts = buildRoundedPoints(X, Y, W, H, rtl, rtr, rbl, rbr);
    fl_begin_polygon();
    for (auto &p : pts) {
        fl_vertex(p.first, p.second);
    }
    fl_end_polygon();
}

void RoundedStyle::drawRoundedOutline(int X, int Y, int W, int H, int rtl, int rtr, int rbl, int rbr, Fl_Color stroke) {
    fl_color(stroke);
    auto pts = buildRoundedPoints(X, Y, W, H, rtl, rtr, rbl, rbr);
    fl_begin_loop();
    for (auto &p : pts) {
        fl_vertex(p.first, p.second);
    }
    fl_end_loop();
}
