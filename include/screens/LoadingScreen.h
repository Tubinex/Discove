#pragma once

#include "router/Screen.h"

class Fl_Box;
class GifBox;

class LoadingScreen : public Screen {
  public:
    LoadingScreen(int x, int y, int w, int h);
    ~LoadingScreen() override;

    void onEnter(const Context &ctx) override;
    void onExit() override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;
    void resize(int x, int y, int w, int h) override;

  private:
    void setupUI();
    void layout();

    GifBox *m_logoBox = nullptr;
    Fl_Box *m_loadingLabel = nullptr;
};
