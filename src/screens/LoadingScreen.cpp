#include "screens/LoadingScreen.h"

#include "ui/GifAnimation.h"
#include "ui/Theme.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Widget.H>
#include <FL/fl_draw.H>
#include <algorithm>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace {
constexpr const char *kLoadingGifPath = "assets/imgs/logo-loading.gif";
constexpr int kLogoSize = 100;
constexpr int kLabelHeight = 32;
constexpr int kLabelGap = 32;
} // namespace

class GifBox : public Fl_Widget {
  public:
    GifBox(int x, int y, int w, int h, Fl_Color bgColor, const char *gifPath)
        : Fl_Widget(x, y, w, h), m_bgColor(bgColor), m_animation(gifPath, GifAnimation::ScalingStrategy::Eager) {}

    ~GifBox() override {
        stop();
        if (m_offscreen) {
            fl_delete_offscreen(m_offscreen);
            m_offscreen = 0;
        }
    }

    bool isValid() const { return m_animation.isValid(); }

    void start() {
        if (m_running || !m_animation.isValid()) {
            return;
        }

        int baseW = m_animation.baseWidth();
        int baseH = m_animation.baseHeight();
        if (baseW > 0 && baseH > 0) {
            float scaleW = static_cast<float>(w()) / static_cast<float>(baseW);
            float scaleH = static_cast<float>(h()) / static_cast<float>(baseH);
            float scale = std::min(scaleW, scaleH);

            int drawW = std::max(1, static_cast<int>(baseW * scale));
            int drawH = std::max(1, static_cast<int>(baseH * scale));

            m_animation.getScaledFrame(drawW, drawH);
        }

        m_running = true;
        scheduleNext();
    }

    void stop() {
        if (!m_running) {
            return;
        }
        Fl::remove_timeout(onTick, this);
        m_running = false;
    }

    void draw() override {
        ensureOffscreen();
        if (m_offscreen) {
            fl_begin_offscreen(m_offscreen);
            fl_push_no_clip();
            drawFrame(0, 0);
            fl_pop_clip();
            fl_end_offscreen();
            fl_copy_offscreen(x(), y(), w(), h(), m_offscreen, 0, 0);
            return;
        }

        drawFrame(x(), y());
    }

  private:
    void ensureOffscreen() {
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

    void drawFrame(int originX, int originY) {
        fl_color(m_bgColor);
        fl_rectf(originX, originY, w(), h());

        int baseW = m_animation.baseWidth();
        int baseH = m_animation.baseHeight();
        if (baseW > 0 && baseH > 0) {
            float scaleW = static_cast<float>(w()) / static_cast<float>(baseW);
            float scaleH = static_cast<float>(h()) / static_cast<float>(baseH);
            float scale = std::min(scaleW, scaleH);

            int drawW = std::max(1, static_cast<int>(baseW * scale));
            int drawH = std::max(1, static_cast<int>(baseH * scale));
            int drawX = originX + (w() - drawW) / 2;
            int drawY = originY + (h() - drawH) / 2;

            if (auto *frame = m_animation.getScaledFrame(drawW, drawH)) {
                frame->draw(drawX, drawY);
            }
        }

        const char *text = label();
        if (text && *text) {
            fl_color(labelcolor());
            fl_font(labelfont(), labelsize());
            fl_draw(text, originX, originY, w(), h(), FL_ALIGN_CENTER);
        }
    }

    static void onTick(void *data) {
        auto *self = static_cast<GifBox *>(data);
        if (self && self->m_running) {
            self->advance();
        }
    }

    void advance() {
        m_animation.nextFrame();
        redraw();
        scheduleNext();
    }

    void scheduleNext() {
        int delayMs = m_animation.currentDelay();
        Fl::add_timeout(static_cast<double>(delayMs) / 1000.0, onTick, this);
    }

    Fl_Color m_bgColor;
    GifAnimation m_animation;
    bool m_running = false;
    Fl_Offscreen m_offscreen = 0;
    int m_offscreenW = 0;
    int m_offscreenH = 0;
};

LoadingScreen::LoadingScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Loading") {
    box(FL_FLAT_BOX);
    color(ThemeColors::BG_PRIMARY);
    setupUI();
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::setupUI() {
    begin();

    m_logoBox = new GifBox(0, 0, kLogoSize, kLogoSize, ThemeColors::BG_PRIMARY, kLoadingGifPath);

    m_loadingLabel = new Fl_Box(0, 0, w(), kLabelHeight, "Connecting to Gateway");
    m_loadingLabel->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    m_loadingLabel->labelcolor(ThemeColors::TEXT_NORMAL);
    m_loadingLabel->labelsize(24);

    end();
    layout();
}

void LoadingScreen::onEnter(const Context &ctx) {
    if (m_logoBox) {
        m_logoBox->start();
    }
}

void LoadingScreen::onExit() {
    if (m_logoBox) {
        m_logoBox->stop();
    }
}

void LoadingScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void LoadingScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}

void LoadingScreen::resize(int x, int y, int w, int h) {
    Screen::resize(x, y, w, h);
    layout();
}

void LoadingScreen::layout() {
    int contentHeight = kLogoSize + kLabelGap + kLabelHeight;
    int startY = (h() - contentHeight) / 2;
    if (startY < 0) {
        startY = 0;
    }

    int gifX = (w() - kLogoSize) / 2;
    if (gifX < 0) {
        gifX = 0;
    }

    if (m_logoBox) {
        m_logoBox->resize(gifX, startY, kLogoSize, kLogoSize);
    }

    if (m_loadingLabel) {
        m_loadingLabel->resize(0, startY + kLogoSize + kLabelGap, w(), kLabelHeight);
    }
}
