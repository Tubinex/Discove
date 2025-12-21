#include "screens/SettingsScreen.h"
#include "router/Router.h"
#include "utils/Logger.h"

SettingsScreen::SettingsScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Settings") { setupUI(); }

void SettingsScreen::setupUI() {
    begin();

    m_titleLabel = new Fl_Box(w() / 2 - 100, 150, 200, 50, "Settings");
    m_titleLabel->labelsize(28);
    m_titleLabel->labelfont(FL_BOLD);

    m_notificationsCheck = new Fl_Check_Button(w() / 2 - 100, 230, 200, 30, "Enable Notifications");
    m_notificationsCheck->value(1);

    m_darkModeCheck = new Fl_Check_Button(w() / 2 - 100, 280, 200, 30, "Dark Mode");
    m_darkModeCheck->value(0);

    m_backBtn = new Fl_Button(w() / 2 - 75, 360, 150, 40, "Back to Home");
    m_backBtn->callback([](Fl_Widget *, void *) { Router::navigate("/"); });

    end();
}

void SettingsScreen::onCreate(const Context &ctx) {}

void SettingsScreen::onEnter(const Context &ctx) {}

void SettingsScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void SettingsScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
