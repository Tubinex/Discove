#include "screens/HomeScreen.h"
#include "router/Router.h"
#include "state/AppState.h"
#include "ui/Theme.h"
#include "utils/Logger.h"

#include <FL/fl_draw.H>

HomeScreen::HomeScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Home") { setupUI(); }

HomeScreen::~HomeScreen() {}

void HomeScreen::setupUI() {
    begin();

    m_guildBar = new GuildBar(0, 0, GUILD_BAR_WIDTH, h() - PROFILE_HEIGHT);
    m_sidebar = new Sidebar(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, h() - PROFILE_HEIGHT);

    m_profileBubble = new ProfileBubble(0, h() - PROFILE_HEIGHT, GUILD_BAR_WIDTH + SIDEBAR_WIDTH, PROFILE_HEIGHT);

    m_guildBar->addGuild("1", "", "Test Server 1");
    m_guildBar->addGuild("2", "", "Another Server");
    m_guildBar->addGuild("3", "", "Cool Guild");
    m_guildBar->setSelectedGuild("1");

    m_sidebar->setGuild("1", "Test Server 1");
    m_sidebar->addTextChannel("100", "general");
    m_sidebar->addTextChannel("101", "random");
    m_sidebar->addTextChannel("102", "memes");
    m_sidebar->addVoiceChannel("200", "General Voice");
    m_sidebar->addVoiceChannel("201", "Gaming");
    m_sidebar->setSelectedChannel("100");

    m_profileBubble->setUser("123", "TestUser", "", "1234");
    m_profileBubble->setStatus("online");

    end();
}

void HomeScreen::onCreate(const Context &ctx) {}

void HomeScreen::onEnter(const Context &ctx) {}

void HomeScreen::resize(int x, int y, int w, int h) {
    Screen::resize(x, y, w, h);

    if (m_guildBar) {
        m_guildBar->resize(0, 0, GUILD_BAR_WIDTH, h - PROFILE_HEIGHT);
    }
    if (m_sidebar) {
        m_sidebar->resize(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, h - PROFILE_HEIGHT);
    }
    if (m_profileBubble) {
        m_profileBubble->resize(0, h - PROFILE_HEIGHT, GUILD_BAR_WIDTH + SIDEBAR_WIDTH, PROFILE_HEIGHT);
    }
}

void HomeScreen::draw() {
    int gapY = h() - PROFILE_HEIGHT;

    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(0, gapY, GUILD_BAR_WIDTH, PROFILE_HEIGHT);

    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(GUILD_BAR_WIDTH, gapY, SIDEBAR_WIDTH, PROFILE_HEIGHT);

    Screen::draw();
}

void HomeScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void HomeScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
