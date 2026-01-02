#pragma once

#include "router/Screen.h"
#include "state/Store.h"
#include "ui/components/ChannelPlaceholder.h"
#include "ui/components/DMSidebar.h"
#include "ui/components/GuildBar.h"
#include "ui/components/GuildSidebar.h"
#include "ui/components/ProfileBubble.h"

#include <unordered_map>

class MainLayoutScreen : public Screen {
  public:
    MainLayoutScreen(int x, int y, int w, int h);
    ~MainLayoutScreen();

    void onCreate(const Context &ctx) override;
    void onEnter(const Context &ctx) override;
    void onExit() override;
    void resize(int x, int y, int w, int h) override;
    void draw() override;

  private:
    void updateContentForRoute(const Context &ctx);
    void clearContentArea();
    void createDMsView();
    void createDMChannelView(const std::string &dmId);
    void createGuildChannelView(const std::string &guildId, const std::string &channelId);

    void setupNavigationCallbacks();
    void subscribeToStore();

    GuildBar *m_guildBar = nullptr;
    ProfileBubble *m_profileBubble = nullptr;

    Fl_Widget *m_sidebar = nullptr;
    Fl_Widget *m_mainContent = nullptr;

    Store::ListenerId m_userProfileListenerId = 0;
    int m_dmSidebarScrollOffset = 0;
    std::unordered_map<std::string, int> m_guildSidebarScrollOffsets;

    static constexpr int GUILD_BAR_WIDTH = 72;
    static constexpr int PROFILE_HEIGHT = 74;
    static constexpr int SIDEBAR_WIDTH = 302;
    static constexpr int GUILDBAR_BOTTOM_PADDING = -8;
};
