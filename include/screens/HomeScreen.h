#pragma once

#include "router/Screen.h"
#include "state/Store.h"
#include "ui/components/GuildBar.h"
#include "ui/components/GuildSidebar.h"
#include "ui/components/ProfileBubble.h"

class HomeScreen : public Screen {
  public:
    HomeScreen(int x, int y, int w, int h);
    ~HomeScreen();

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onTransitionIn(float progress) override;
    void onTransitionOut(float progress) override;
    void resize(int x, int y, int w, int h) override;
    void draw() override;

  private:
    void setupUI();
    void subscribeToStore();

    static constexpr int GUILD_BAR_WIDTH = 72;
    static constexpr int SIDEBAR_WIDTH = 302;
    static constexpr int PROFILE_HEIGHT = 74;
    static constexpr int GUILDBAR_BOTTOM_PADDING = -8;

    GuildBar *m_guildBar = nullptr;
    GuildSidebar *m_sidebar = nullptr;
    ProfileBubble *m_profileBubble = nullptr;

    Store::ListenerId m_userProfileListenerId = 0;
};
