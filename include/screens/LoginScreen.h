#pragma once

#include "router/Screen.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>

class LoginScreen : public Screen {
  public:
    LoginScreen(int x, int y, int w, int h);

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();

    Fl_Box *m_titleLabel = nullptr;
    Fl_Box *m_usernameLabel = nullptr;
    Fl_Input *m_usernameInput = nullptr;
    Fl_Box *m_passwordLabel = nullptr;
    Fl_Input *m_passwordInput = nullptr;
    Fl_Button *m_loginBtn = nullptr;
    Fl_Button *m_backBtn = nullptr;
};
