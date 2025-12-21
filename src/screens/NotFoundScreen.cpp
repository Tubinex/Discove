#include "screens/NotFoundScreen.h"
#include "router/Router.h"
#include "utils/Logger.h"

NotFoundScreen::NotFoundScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Not Found") { setupUI(); }

void NotFoundScreen::setupUI() {
    begin();

    m_titleLabel = new Fl_Box(w() / 2 - 100, 200, 200, 50, "404");
    m_titleLabel->labelsize(48);
    m_titleLabel->labelfont(FL_BOLD);

    m_messageLabel = new Fl_Box(w() / 2 - 150, 270, 300, 60, "Page not found\nThe requested page does not exist.");
    m_messageLabel->labelsize(16);
    m_messageLabel->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);

    m_homeBtn = new Fl_Button(w() / 2 - 75, 360, 150, 40, "Go Home");
    m_homeBtn->callback([](Fl_Widget *, void *) { Router::navigate("/"); });

    end();
}

void NotFoundScreen::onCreate(const Context &ctx) {}

void NotFoundScreen::onEnter(const Context &ctx) {}

void NotFoundScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void NotFoundScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
