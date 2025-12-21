#include "screens/ProfileScreen.h"
#include "router/Router.h"
#include "utils/Logger.h"

ProfileScreen::ProfileScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Profile") { setupUI(); }

void ProfileScreen::setupUI() {
    begin();

    m_titleLabel = new Fl_Box(w() / 2 - 100, 150, 200, 50, "User Profile");
    m_titleLabel->labelsize(28);
    m_titleLabel->labelfont(FL_BOLD);

    m_userIdLabel = new Fl_Box(w() / 2 - 100, 220, 200, 30, "User ID: ");
    m_userIdLabel->labelsize(18);
    m_userIdLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

    m_userInfoLabel = new Fl_Box(w() / 2 - 100, 270, 200, 60, "Loading user information...");
    m_userInfoLabel->labelsize(14);
    m_userInfoLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);

    m_backBtn = new Fl_Button(w() / 2 - 75, 360, 150, 40, "Back to Home");
    m_backBtn->callback([](Fl_Widget *, void *) { Router::navigate("/"); });

    end();
}

void ProfileScreen::onCreate(const Context &ctx) {
    auto it = ctx.params.find("id");
    if (it != ctx.params.end()) {
        m_userId = it->second;
        loadUserProfile(m_userId);
    }
}

void ProfileScreen::onEnter(const Context &ctx) {}

void ProfileScreen::loadUserProfile(const std::string &userId) {
    std::string userIdText = "User ID: " + userId;
    m_userIdLabel->copy_label(userIdText.c_str());

    std::string userInfo = "Name: User " + userId + "\nEmail: user" + userId + "@example.com\nStatus: Active";
    m_userInfoLabel->copy_label(userInfo.c_str());

    redraw();
}

void ProfileScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void ProfileScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
