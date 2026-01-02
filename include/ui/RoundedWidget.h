#pragma once

#include <FL/Fl.H>
#include <utility>

class RoundedStyle {
  public:
    void setBorderRadius(int radius);
    int getBorderRadius() const { return m_borderRadius; }

    static void drawRoundedRect(int x, int y, int w, int h, int radiusTopLeft, int radiusTopRight,
                                int radiusBottomLeft, int radiusBottomRight, Fl_Color fill);

    static void drawRoundedOutline(int x, int y, int w, int h, int radiusTopLeft, int radiusTopRight,
                                    int radiusBottomLeft, int radiusBottomRight, Fl_Color stroke);

  protected:
    void drawRoundedRect(int x, int y, int w, int h, int radius, Fl_Color color) const;

  private:
    int m_borderRadius = 8;
};

template <typename WidgetT>
class RoundedWidget : public WidgetT, public RoundedStyle {
  public:
    template <typename... Args>
    explicit RoundedWidget(Args &&...args) : WidgetT(std::forward<Args>(args)...) {}

    void setBorderRadius(int radius) {
        RoundedStyle::setBorderRadius(radius);
        this->redraw();
    }
};
