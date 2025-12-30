#pragma once

#include <FL/Fl_Widget.H>
#include <string>

class ChannelPlaceholder : public Fl_Widget {
  public:
    ChannelPlaceholder(int x, int y, int w, int h, const char *label = nullptr);

    void draw() override;

    void setText(const std::string &text) {
        m_text = text;
        redraw();
    }

  private:
    std::string m_text = "No channel selected";
};
