#pragma once

#include "router/Screen.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>

class NotFoundScreen : public Screen {
  public:
    NotFoundScreen(int x, int y, int w, int h);

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();

    Fl_Box *m_titleLabel = nullptr;
    Fl_Box *m_messageLabel = nullptr;
    Fl_Button *m_homeBtn = nullptr;
};
