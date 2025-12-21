#include "screens/LoginScreen.h"
#include "router/Router.h"
#include "utils/Logger.h"

LoginScreen::LoginScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Login") { setupUI(); }

void LoginScreen::setupUI() {
    begin();

    m_titleLabel = new Fl_Box(w() / 2 - 100, 150, 200, 50, "Login");
    m_titleLabel->labelsize(28);
    m_titleLabel->labelfont(FL_BOLD);

    m_usernameLabel = new Fl_Box(w() / 2 - 100, 210, 200, 20, "Username:");
    m_usernameLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_usernameInput = new Fl_Input(w() / 2 - 100, 230, 200, 30);

    m_passwordLabel = new Fl_Box(w() / 2 - 100, 270, 200, 20, "Password:");
    m_passwordLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_passwordInput = new Fl_Input(w() / 2 - 100, 290, 200, 30);
    m_passwordInput->type(FL_SECRET_INPUT);

    m_loginBtn = new Fl_Button(w() / 2 - 75, 350, 150, 40, "Login");
    m_loginBtn->callback([](Fl_Widget *, void *) {
        // TODO: Implement login logic
        Router::navigate("/");
    });

    m_backBtn = new Fl_Button(w() / 2 - 75, 410, 150, 40, "Back to Home");
    m_backBtn->callback([](Fl_Widget *, void *) { Router::back(); });

    end();
}

void LoginScreen::onCreate(const Context &ctx) {}

void LoginScreen::onEnter(const Context &ctx) {}

void LoginScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void LoginScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
