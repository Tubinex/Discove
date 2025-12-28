#pragma once

#include <FL/Fl_Group.H>
#include <functional>
#include <string>
#include <vector>

#include "state/Store.h"

class GuildBar : public Fl_Group {
  public:
    GuildBar(int x, int y, int w, int h, const char *label = nullptr);
    ~GuildBar() override;

    void refresh();
    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;
    void draw() override;
    void repositionChildren();

    void setOnGuildSelected(std::function<void(const std::string &)> cb) { m_onGuildSelected = std::move(cb); }
    void setOnHomeClicked(std::function<void()> cb) { m_onHomeClicked = std::move(cb); }

  private:
    void subscribeToStore();

    Store::ListenerId m_guildDataListenerId = 0;
    double m_scrollOffset = 0.0;
    std::function<void(const std::string &)> m_onGuildSelected;
    std::function<void()> m_onHomeClicked;
    std::vector<std::string> m_layoutSignature;
};
