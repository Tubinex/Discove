#pragma once

#include "router/Screen.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

class ProfileScreen : public Screen {
  public:
    ProfileScreen(int x, int y, int w, int h);

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();
    void loadUserProfile(const std::string &userId);

    Fl_Box *m_titleLabel = nullptr;
    Fl_Box *m_userIdLabel = nullptr;
    Fl_Box *m_userInfoLabel = nullptr;
    Fl_Button *m_backBtn = nullptr;

    std::string m_userId;
};
