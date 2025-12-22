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
    void onLoginClicked();

    Fl_Box *m_titleLabel = nullptr;
    Fl_Box *m_tokenLabel = nullptr;
    Fl_Input *m_tokenInput = nullptr;
    Fl_Button *m_loginBtn = nullptr;
    Fl_Box *m_errorLabel = nullptr;
};
