#pragma once

#include "ui/RoundedWidget.h"
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>

class RoundedButton : public RoundedWidget<Fl_Button> {
  public:
    RoundedButton(int x, int y, int w, int h, const char *label = nullptr);
    ~RoundedButton() override;

    void setPressedColor(Fl_Color color);
    int handle(int event) override;
    void draw() override;

  private:
    void ensureOffscreen();

    bool m_isHovered = false;
    bool m_hasPressedColor = false;
    Fl_Color m_pressedColor = 0;
    Fl_Offscreen m_offscreen = 0;
    int m_offscreenW = 0;
    int m_offscreenH = 0;
};
