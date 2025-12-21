#pragma once

#include "router/Screen.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Check_Button.H>

class SettingsScreen : public Screen {
  public:
    SettingsScreen(int x, int y, int w, int h);

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();

    Fl_Box *m_titleLabel = nullptr;
    Fl_Check_Button *m_notificationsCheck = nullptr;
    Fl_Check_Button *m_darkModeCheck = nullptr;
    Fl_Button *m_backBtn = nullptr;
};
