#pragma once

#include "router/Screen.h"
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Scroll.H>
#include <vector>

class UILibraryScreen : public Screen {
  public:
    UILibraryScreen(int x, int y, int w, int h);
    ~UILibraryScreen();

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;

  private:
    void setupUI();
    void addSectionTitle(const char *title);
    void addButtons();
    void addColors();
    void addTypography();
    void addSpacing();

    Fl_Scroll *m_scroll = nullptr;
    int m_currentY = 20;
    const int m_padding = 20;
    const int m_sectionSpacing = 40;
};
