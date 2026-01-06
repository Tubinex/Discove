#include "ui/RoundedWidget.h"

#include <FL/fl_draw.H>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {
constexpr double kPi = 3.14159265358979323846;

struct PathCacheKey {
    int w, h, rtl, rtr, rbl, rbr;

    bool operator==(const PathCacheKey &other) const {
        return w == other.w && h == other.h && rtl == other.rtl && rtr == other.rtr && rbl == other.rbl &&
               rbr == other.rbr;
    }
};

struct PathCacheKeyHash {
    std::size_t operator()(const PathCacheKey &k) const {
        std::size_t h = 0;
        h ^= std::hash<int>{}(k.w) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.h) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.rtl) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.rtr) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.rbl) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<int>{}(k.rbr) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

std::unordered_map<PathCacheKey, std::vector<std::pair<double, double>>, PathCacheKeyHash> path_cache;
std::vector<std::pair<double, double>> buildRelativeRoundedPath(int W, int H, int rtl, int rtr, int rbl, int rbr) {
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
        appendArc(rtl, rtl, rtl, kPi, 1.5 * kPi);
    else
        pts.emplace_back(0, 0);

    if (rtr > 0) {
        pts.emplace_back(W - rtr, 0);
        appendArc(W - rtr, rtr, rtr, 1.5 * kPi, 2.0 * kPi);
    } else {
        pts.emplace_back(W, 0);
    }

    if (rbr > 0) {
        pts.emplace_back(W, H - rbr);
        appendArc(W - rbr, H - rbr, rbr, 0.0, 0.5 * kPi);
    } else {
        pts.emplace_back(W, H);
    }

    if (rbl > 0) {
        pts.emplace_back(rbl, H);
        appendArc(rbl, H - rbl, rbl, 0.5 * kPi, kPi);
    } else {
        pts.emplace_back(0, H);
    }

    if (rtl == 0)
        pts.emplace_back(0, 0);
    return pts;
}

std::vector<std::pair<double, double>> buildRoundedPoints(int X, int Y, int W, int H, int rtl, int rtr, int rbl,
                                                          int rbr) {
    if (W <= 0 || H <= 0 || W > 10000 || H > 10000) {
        return {};
    }

    rtl = std::max(0, std::min({rtl, W / 2, H / 2}));
    rtr = std::max(0, std::min({rtr, W / 2, H / 2}));
    rbl = std::max(0, std::min({rbl, W / 2, H / 2}));
    rbr = std::max(0, std::min({rbr, W / 2, H / 2}));

    if (path_cache.size() > 1000) {
        path_cache.clear();
    }

    PathCacheKey key{W, H, rtl, rtr, rbl, rbr};
    std::vector<std::pair<double, double>> relativePts;
    auto it = path_cache.find(key);
    if (it != path_cache.end()) {
        relativePts = it->second;
    } else {
        relativePts = buildRelativeRoundedPath(W, H, rtl, rtr, rbl, rbr);
        if (!relativePts.empty()) {
            path_cache[key] = relativePts;
        }
    }

    std::vector<std::pair<double, double>> pts;
    pts.reserve(relativePts.size());
    for (const auto &p : relativePts) {
        pts.emplace_back(X + p.first, Y + p.second);
    }
    return pts;
}
} // namespace

void RoundedStyle::setBorderRadius(int radius) { m_borderRadius = std::max(0, radius); }

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
    if (W <= 0 || H <= 0)
        return;

    fl_color(fill);
    auto pts = buildRoundedPoints(X, Y, W, H, rtl, rtr, rbl, rbr);
    if (pts.empty())
        return;

    fl_begin_polygon();
    for (auto &p : pts) {
        fl_vertex(p.first, p.second);
    }
    fl_end_polygon();
}

void RoundedStyle::drawRoundedOutline(int X, int Y, int W, int H, int rtl, int rtr, int rbl, int rbr, Fl_Color stroke) {
    if (W <= 0 || H <= 0)
        return;

    fl_color(stroke);
    auto pts = buildRoundedPoints(X, Y, W, H, rtl, rtr, rbl, rbr);
    if (pts.empty())
        return;

    fl_begin_loop();
    for (auto &p : pts) {
        fl_vertex(p.first, p.second);
    }
    fl_end_loop();
}
