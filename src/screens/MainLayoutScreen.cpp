#include "screens/MainLayoutScreen.h"

#include "router/Router.h"
#include "state/AppState.h"
#include "state/Store.h"
#include "ui/Theme.h"
#include "utils/Logger.h"

#include <FL/fl_draw.H>

MainLayoutScreen::MainLayoutScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "MainLayout") {
    Logger::debug("MainLayoutScreen constructor called");
}

MainLayoutScreen::~MainLayoutScreen() {
    Logger::debug("MainLayoutScreen destructor called");

    if (m_userProfileListenerId) {
        Store::get().unsubscribe(m_userProfileListenerId);
    }
}

void MainLayoutScreen::onCreate(const Context &ctx) {
    Logger::debug("MainLayoutScreen::onCreate called for path: " + ctx.path);
    begin();

    int contentHeight = h() - PROFILE_HEIGHT - GUILDBAR_BOTTOM_PADDING;

    Logger::debug("Creating GuildBar...");
    m_guildBar = new GuildBar(0, 0, GUILD_BAR_WIDTH, contentHeight);
    Logger::debug("GuildBar created");

    Logger::debug("Creating ProfileBubble...");
    m_profileBubble = new ProfileBubble(0, h() - PROFILE_HEIGHT, GUILD_BAR_WIDTH + SIDEBAR_WIDTH, PROFILE_HEIGHT);
    Logger::debug("ProfileBubble created");

    add(m_guildBar);
    add(m_profileBubble);

    end();

    Logger::debug("Setting up navigation callbacks...");
    setupNavigationCallbacks();
    Logger::debug("Navigation callbacks set up");

    Logger::debug("Subscribing to Store...");
    subscribeToStore();
    Logger::debug("Store subscription set up");

    updateContentForRoute(ctx);
}

void MainLayoutScreen::onEnter(const Context &ctx) { updateContentForRoute(ctx); }

void MainLayoutScreen::onExit() {}

void MainLayoutScreen::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);

    int contentHeight = h - PROFILE_HEIGHT - GUILDBAR_BOTTOM_PADDING;

    if (m_guildBar) {
        m_guildBar->resize(0, 0, GUILD_BAR_WIDTH, contentHeight);
    }

    if (m_profileBubble) {
        m_profileBubble->resize(0, h - PROFILE_HEIGHT, GUILD_BAR_WIDTH + SIDEBAR_WIDTH, PROFILE_HEIGHT);
    }

    if (m_sidebar) {
        m_sidebar->resize(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, contentHeight);
    }

    if (m_mainContent) {
        m_mainContent->resize(GUILD_BAR_WIDTH + SIDEBAR_WIDTH, 0, w - GUILD_BAR_WIDTH - SIDEBAR_WIDTH, h);
    }
}

void MainLayoutScreen::draw() {
    fl_color(ThemeColors::BG_PRIMARY);
    fl_rectf(x(), y(), w(), h());

    int profileY = h() - PROFILE_HEIGHT;
    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(0, profileY, GUILD_BAR_WIDTH + SIDEBAR_WIDTH, PROFILE_HEIGHT);

    for (int i = 0; i < children(); i++) {
        Fl_Widget *child = this->child(i);
        if (child != m_profileBubble && child->visible()) {
            draw_child(*child);
        }
    }

    if (m_profileBubble && m_profileBubble->visible()) {
        draw_child(*m_profileBubble);
    }
}

void MainLayoutScreen::updateContentForRoute(const Context &ctx) {
    Logger::info("MainLayoutScreen::updateContentForRoute - path: " + ctx.path);

    clearContentArea();
    if (ctx.path == "/channels/me" || ctx.path == "/") {
        createDMsView();
    } else if (ctx.params.count("dmId") > 0) {
        createDMChannelView(ctx.params.at("dmId"));
    } else if (ctx.params.count("guildId") > 0 && ctx.params.count("channelId") > 0) {
        createGuildChannelView(ctx.params.at("guildId"), ctx.params.at("channelId"));
    } else {
        createDMsView();
    }

    redraw();
}

void MainLayoutScreen::clearContentArea() {
    if (m_sidebar) {
        if (auto *dmSidebar = dynamic_cast<DMSidebar *>(m_sidebar)) {
            m_dmSidebarScrollOffset = dmSidebar->getScrollOffset();
        } else if (auto *guildSidebar = dynamic_cast<GuildSidebar *>(m_sidebar)) {
            std::string guildId = guildSidebar->getGuildId();
            if (!guildId.empty()) {
                m_guildSidebarScrollOffsets[guildId] = guildSidebar->getScrollOffset();
            }
        }

        remove(m_sidebar);
        delete m_sidebar;
        m_sidebar = nullptr;
    }

    if (m_mainContent) {
        remove(m_mainContent);
        delete m_mainContent;
        m_mainContent = nullptr;
    }
}

void MainLayoutScreen::createDMsView() {
    Logger::info("MainLayoutScreen::createDMsView");

    int contentHeight = h() - PROFILE_HEIGHT - GUILDBAR_BOTTOM_PADDING;

    begin();

    auto *sidebar = new DMSidebar(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, contentHeight);
    sidebar->setScrollOffset(m_dmSidebarScrollOffset);
    sidebar->setOnDMSelected([](const std::string &dmId) { Router::navigate("/channels/" + dmId); });

    auto *channelView =
        new TextChannelView(GUILD_BAR_WIDTH + SIDEBAR_WIDTH, 0, w() - GUILD_BAR_WIDTH - SIDEBAR_WIDTH, h());
    channelView->setChannel("", "Select a DM to start messaging", "", false);

    m_sidebar = sidebar;
    m_mainContent = channelView;

    add(m_sidebar);
    add(m_mainContent);

    end();
}

void MainLayoutScreen::createDMChannelView(const std::string &dmId) {
    Logger::info("MainLayoutScreen::createDMChannelView - dmId: " + dmId);

    int contentHeight = h() - PROFILE_HEIGHT - GUILDBAR_BOTTOM_PADDING;

    begin();

    auto *sidebar = new DMSidebar(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, contentHeight);
    sidebar->setScrollOffset(m_dmSidebarScrollOffset);
    sidebar->setSelectedDM(dmId);
    sidebar->setOnDMSelected([](const std::string &newDmId) { Router::navigate("/channels/" + newDmId); });

    auto state = Store::get().snapshot();
    std::string channelName = "Direct Message";
    for (const auto &dm : state.privateChannels) {
        if (dm->id == dmId && dm->name.has_value()) {
            channelName = *dm->name;
            break;
        }
    }

    auto *channelView =
        new TextChannelView(GUILD_BAR_WIDTH + SIDEBAR_WIDTH, 0, w() - GUILD_BAR_WIDTH - SIDEBAR_WIDTH, h());
    channelView->setChannel(dmId, channelName, "", true);

    m_sidebar = sidebar;
    m_mainContent = channelView;

    add(m_sidebar);
    add(m_mainContent);

    end();
}

void MainLayoutScreen::createGuildChannelView(const std::string &guildId, const std::string &channelId) {
    Logger::info("MainLayoutScreen::createGuildChannelView - guildId: " + guildId + ", channelId: " + channelId);

    int contentHeight = h() - PROFILE_HEIGHT - GUILDBAR_BOTTOM_PADDING;

    begin();

    auto *sidebar = new GuildSidebar(GUILD_BAR_WIDTH, 0, SIDEBAR_WIDTH, contentHeight);
    sidebar->setGuildId(guildId);
    sidebar->setSelectedChannel(channelId);

    auto scrollIt = m_guildSidebarScrollOffsets.find(guildId);
    if (scrollIt != m_guildSidebarScrollOffsets.end()) {
        sidebar->setScrollOffset(scrollIt->second);
    }

    sidebar->setOnChannelSelected(
        [guildId](const std::string &newChannelId) { Router::navigate("/channels/" + guildId + "/" + newChannelId); });

    auto state = Store::get().snapshot();
    std::string channelName = "channel";
    bool isWelcomeVisible = false;
    auto channelIt = state.guildChannels.find(guildId);
    if (channelIt != state.guildChannels.end()) {
        for (const auto &channel : channelIt->second) {
            if (channel->id == channelId && channel->name.has_value()) {
                channelName = *channel->name;
                isWelcomeVisible = (channel->type == ChannelType::GUILD_TEXT);
                break;
            }
        }
    }

    auto *channelView =
        new TextChannelView(GUILD_BAR_WIDTH + SIDEBAR_WIDTH, 0, w() - GUILD_BAR_WIDTH - SIDEBAR_WIDTH, h());
    channelView->setChannel(channelId, channelName, guildId, isWelcomeVisible);

    m_sidebar = sidebar;
    m_mainContent = channelView;

    add(m_sidebar);
    add(m_mainContent);

    end();
}

void MainLayoutScreen::setupNavigationCallbacks() {
    m_guildBar->setOnGuildSelected([](const std::string &guildId) {
        Logger::info("Guild selected: " + guildId);
        Router::navigate("/channels/" + guildId + "/1");
    });

    m_guildBar->setOnHomeClicked([]() { Router::navigate("/channels/me"); });
    m_profileBubble->setOnSettingsClicked([]() { Logger::info("Settings clicked"); });
    m_profileBubble->setOnMicrophoneClicked([]() { Logger::info("Microphone toggled"); });
    m_profileBubble->setOnHeadphonesClicked([]() { Logger::info("Headphones toggled"); });
}

void MainLayoutScreen::subscribeToStore() {
    m_profileBubble->setUser("123", "Loading...", "", "0");
    m_profileBubble->setStatus("online");

    m_userProfileListenerId = Store::get().subscribe<std::optional<UserProfile>>(
        [](const AppState &state) { return state.currentUser; },
        [this](const std::optional<UserProfile> &profile) {
            if (profile.has_value() && m_profileBubble) {
                const auto &user = profile.value();
                std::string displayName = !user.globalName.empty() ? user.globalName : user.username;
                m_profileBubble->setUser(user.id, displayName, user.avatarUrl, user.discriminator);
                m_profileBubble->setStatus(user.status);

                if (user.customStatus.has_value()) {
                    const auto &cs = user.customStatus.value();
                    m_profileBubble->setCustomStatus(cs.text, cs.emojiUrl);
                } else {
                    m_profileBubble->setCustomStatus("", "");
                }
            }
        },
        std::equal_to<std::optional<UserProfile>>{}, true);
}
