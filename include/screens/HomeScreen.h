#pragma once

#include "router/Screen.h"
#include "state/Store.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

class HomeScreen : public Screen {
  public:
    HomeScreen(int x, int y, int w, int h);
    ~HomeScreen();

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();
    void onCounterChange(int newCounter);

    Fl_Box *m_counterLabel = nullptr;
    Fl_Button *m_incrementBtn = nullptr;
    Fl_Button *m_settingsBtn = nullptr;
    Fl_Button *m_loginBtn = nullptr;
    Fl_Button *m_profileBtn = nullptr;

    Store::ListenerId m_counterSub;
};
