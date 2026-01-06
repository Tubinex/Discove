#include "screens/MainLayoutScreen.h"

#include "net/APIClient.h"
#include "data/Database.h"
#include "models/Message.h"
#include "router/Router.h"
#include "state/AppState.h"
#include "state/Store.h"
#include "ui/Theme.h"
#include "utils/Logger.h"
#include "utils/Permissions.h"

#include <FL/fl_draw.H>

#include <algorithm>
#include <map>

namespace {
void removePendingMessage(AppState &state, const std::string &channelId, const std::string &nonce) {
    if (channelId.empty() || nonce.empty()) {
        return;
    }

    auto pendingIt = state.pendingChannelMessages.find(channelId);
    if (pendingIt == state.pendingChannelMessages.end()) {
        return;
    }

    auto &pending = pendingIt->second;
    pending.erase(
        std::remove_if(pending.begin(), pending.end(),
                       [&](const Message &msg) { return msg.nonce.has_value() && msg.nonce.value() == nonce; }),
        pending.end());
    if (pending.empty()) {
        state.pendingChannelMessages.erase(pendingIt);
    }
}

bool isSelectableTextChannel(ChannelType type) {
    switch (type) {
    case ChannelType::GUILD_TEXT:
    case ChannelType::GUILD_ANNOUNCEMENT:
    case ChannelType::ANNOUNCEMENT_THREAD:
    case ChannelType::PUBLIC_THREAD:
    case ChannelType::PRIVATE_THREAD:
    case ChannelType::GUILD_FORUM:
    case ChannelType::GUILD_MEDIA:
        return true;
    default:
        return false;
    }
}

std::string findFirstVisibleTextChannel(const AppState &state, const std::string &guildId) {
    auto channelIt = state.guildChannels.find(guildId);
    if (channelIt == state.guildChannels.end()) {
        return "";
    }

    if (!state.currentUser.has_value()) {
        return "";
    }

    std::vector<std::string> userRoleIds;
    auto memberIt = state.guildMembers.find(guildId);
    if (memberIt != state.guildMembers.end()) {
        userRoleIds = memberIt->second.roleIds;
    }

    std::vector<Role> guildRoles;
    auto rolesIt = state.guildRoles.find(guildId);
    if (rolesIt != state.guildRoles.end()) {
        guildRoles = rolesIt->second;
    }

    uint64_t basePermissions = PermissionUtils::computeBasePermissions(guildId, userRoleIds, guildRoles);

    struct CategoryInfo {
        int position = 0;
        std::vector<std::shared_ptr<GuildChannel>> channels;
    };

    std::map<std::string, CategoryInfo> categories;
    std::vector<std::shared_ptr<GuildChannel>> uncategorized;
    std::vector<std::shared_ptr<GuildChannel>> regularChannels;

    for (const auto &channel : channelIt->second) {
        if (!channel || !channel->name.has_value()) {
            continue;
        }

        if (channel->type == ChannelType::GUILD_CATEGORY) {
            categories[channel->id].position = channel->position;
        } else {
            regularChannels.push_back(channel);
        }
    }

    for (const auto &channel : regularChannels) {
        bool canView =
            PermissionUtils::canViewChannel(guildId, userRoleIds, channel->permissionOverwrites, basePermissions);
        if (!canView) {
            continue;
        }

        if (channel->parentId.has_value() && !channel->parentId->empty()) {
            auto catIt = categories.find(*channel->parentId);
            if (catIt != categories.end()) {
                catIt->second.channels.push_back(channel);
            } else {
                uncategorized.push_back(channel);
            }
        } else {
            uncategorized.push_back(channel);
        }
    }

    std::stable_sort(uncategorized.begin(), uncategorized.end(),
                     [](const std::shared_ptr<GuildChannel> &a, const std::shared_ptr<GuildChannel> &b) {
                         return a->position < b->position;
                     });

    std::vector<std::pair<std::string, CategoryInfo>> orderedCategories;
    orderedCategories.reserve(categories.size());
    for (auto &entry : categories) {
        if (!entry.second.channels.empty()) {
            std::stable_sort(entry.second.channels.begin(), entry.second.channels.end(),
                             [](const std::shared_ptr<GuildChannel> &a, const std::shared_ptr<GuildChannel> &b) {
                                 return a->position < b->position;
                             });
            orderedCategories.push_back(entry);
        }
    }

    std::stable_sort(orderedCategories.begin(), orderedCategories.end(),
                     [](const auto &a, const auto &b) { return a.second.position < b.second.position; });

    for (const auto &channel : uncategorized) {
        if (isSelectableTextChannel(channel->type)) {
            return channel->id;
        }
    }

    for (const auto &category : orderedCategories) {
        for (const auto &channel : category.second.channels) {
            if (isSelectableTextChannel(channel->type)) {
                return channel->id;
            }
        }
    }

    return "";
}
} // namespace

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
    channelView->setOnSendMessage([channelId = dmId](const std::string &message, const std::string &nonce) {
        if (channelId.empty()) {
            return;
        }
        Discord::APIClient::get().sendChannelMessage(
            channelId, message, nonce,
            [channelId, nonce](const Discord::APIClient::Json &json) {
                try {
                    Message sent = Message::fromJson(json);
                    Data::Database::get().insertMessage(sent);
                    Store::get().update([&](AppState &state) {
                        removePendingMessage(state, channelId, nonce);
                        auto &messages = state.channelMessages[channelId];
                        auto it =
                            std::find_if(messages.begin(), messages.end(), [&](const Message &m) { return m.id == sent.id; });
                        if (it == messages.end()) {
                            messages.push_back(sent);
                            std::sort(messages.begin(), messages.end(),
                                      [](const Message &a, const Message &b) { return a.timestamp < b.timestamp; });
                        }
                    });
                    Logger::debug("Message sent.");
                } catch (const std::exception &e) {
                    Logger::error("Failed to parse send response: " + std::string(e.what()));
                    Store::get().update(
                        [&](AppState &state) { removePendingMessage(state, channelId, nonce); });
                }
            },
            [channelId, nonce](int code, const std::string &error) {
                Logger::error("Failed to send message (" + std::to_string(code) + "): " + error);
                Store::get().update([&](AppState &state) { removePendingMessage(state, channelId, nonce); });
            });
    });

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
    channelView->setOnSendMessage([channelId](const std::string &message, const std::string &nonce) {
        if (channelId.empty()) {
            return;
        }
        Discord::APIClient::get().sendChannelMessage(
            channelId, message, nonce,
            [channelId, nonce](const Discord::APIClient::Json &json) {
                try {
                    Message sent = Message::fromJson(json);
                    Data::Database::get().insertMessage(sent);
                    Store::get().update([&](AppState &state) {
                        removePendingMessage(state, channelId, nonce);
                        auto &messages = state.channelMessages[channelId];
                        auto it =
                            std::find_if(messages.begin(), messages.end(), [&](const Message &m) { return m.id == sent.id; });
                        if (it == messages.end()) {
                            messages.push_back(sent);
                            std::sort(messages.begin(), messages.end(),
                                      [](const Message &a, const Message &b) { return a.timestamp < b.timestamp; });
                        }
                    });
                    Logger::debug("Message sent.");
                } catch (const std::exception &e) {
                    Logger::error("Failed to parse send response: " + std::string(e.what()));
                    Store::get().update(
                        [&](AppState &state) { removePendingMessage(state, channelId, nonce); });
                }
            },
            [channelId, nonce](int code, const std::string &error) {
                Logger::error("Failed to send message (" + std::to_string(code) + "): " + error);
                Store::get().update([&](AppState &state) { removePendingMessage(state, channelId, nonce); });
            });
    });

    m_sidebar = sidebar;
    m_mainContent = channelView;

    add(m_sidebar);
    add(m_mainContent);

    end();
}

void MainLayoutScreen::setupNavigationCallbacks() {
    m_guildBar->setOnGuildSelected([](const std::string &guildId) {
        Logger::info("Guild selected: " + guildId);

        auto state = Store::get().snapshot();
        std::string channelId = findFirstVisibleTextChannel(state, guildId);
        if (!channelId.empty()) {
            Router::navigate("/channels/" + guildId + "/" + channelId);
            return;
        }

        Logger::warn("No visible text channels found for guild: " + guildId);
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
