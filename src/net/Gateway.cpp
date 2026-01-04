#include "net/Gateway.h"

#include "models/Channel.h"
#include "models/GuildFolder.h"
#include "models/GuildMember.h"
#include "models/Role.h"
#include "models/User.h"
#include "state/Store.h"
#include "utils/CDN.h"
#include "utils/Logger.h"
#include "utils/Protobuf.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

static int lwsCallback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len) {
    Gateway *gateway = static_cast<Gateway *>(lws_context_user(lws_get_context(wsi)));
    if (!gateway)
        return 0;

    switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        Logger::info("WebSocket connected");
        gateway->m_connected.store(true);
        gateway->m_wsi = wsi;
        gateway->notifyConnectionState(Gateway::ConnectionState::Connected);
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE: {
        if (in && len > 0) {
            gateway->m_receiveBuffer.append(static_cast<char *>(in), len);

            if (lws_is_final_fragment(wsi)) {
                gateway->receive(gateway->m_receiveBuffer);
                gateway->m_receiveBuffer.clear();
            }
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
        std::scoped_lock lock(gateway->m_sendMutex);
        if (!gateway->m_sendQueue.empty()) {
            std::string &msg = gateway->m_sendQueue.front();

            std::vector<unsigned char> buf(LWS_PRE + msg.size());
            memcpy(&buf[LWS_PRE], msg.data(), msg.size());

            int written = lws_write(wsi, &buf[LWS_PRE], msg.size(), LWS_WRITE_TEXT);
            if (written == static_cast<int>(msg.size())) {
                gateway->m_sendQueue.pop();

                if (!gateway->m_sendQueue.empty()) {
                    lws_callback_on_writable(wsi);
                }
            }
        }
        break;
    }

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        Logger::error(std::string("WebSocket connection error: ") + (in ? static_cast<char *>(in) : "unknown"));
        gateway->m_connected.store(false);
        gateway->m_wsi = nullptr;
        gateway->notifyConnectionState(Gateway::ConnectionState::Disconnected);
        break;

    case LWS_CALLBACK_CLIENT_CLOSED:
    case LWS_CALLBACK_WSI_DESTROY:
        Logger::info("WebSocket closed");
        gateway->m_connected.store(false);
        gateway->m_wsi = nullptr;
        gateway->notifyConnectionState(Gateway::ConnectionState::Disconnected);
        break;

    default:
        break;
    }

    return 0;
}

Gateway *Gateway::s_instance = nullptr;

Gateway &Gateway::get() {
    if (!s_instance) {
        s_instance = new Gateway();
    }
    return *s_instance;
}

Gateway::Gateway() {}

Gateway::~Gateway() {
    disconnect();
    if (m_serviceThread.joinable()) {
        m_serviceThread.join();
    }
    if (m_lwsContext) {
        lws_context_destroy(m_lwsContext);
        m_lwsContext = nullptr;
    }
}

bool Gateway::connect(const Options &opt) {
    if (m_connected.load())
        return true;

    m_options = opt;
    m_shuttingDown.store(false);

    std::string url = m_options.url;
    bool useTLS = url.find("wss://") == 0;
    size_t hostStart = useTLS ? 6 : 5;
    size_t pathStart = url.find('/', hostStart);
    std::string host = url.substr(hostStart, pathStart - hostStart);
    std::string path = pathStart != std::string::npos ? url.substr(pathStart) : "/";
    int port = useTLS ? 443 : 80;

    static const lws_protocols protocols[] = {{"discord-gateway", lwsCallback, 0, 65536, 0, nullptr, 0},
                                              {nullptr, nullptr, 0, 0, 0, nullptr, 0}};

    lws_context_creation_info info{};
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.user = this;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.fd_limit_per_thread = 1 + 1 + 1;

    m_lwsContext = lws_create_context(&info);
    if (!m_lwsContext) {
        Logger::error("Failed to create libwebsockets context");
        return false;
    }

    lws_client_connect_info ccinfo{};
    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = m_lwsContext;
    ccinfo.address = host.c_str();
    ccinfo.port = port;
    ccinfo.path = path.c_str();
    ccinfo.host = host.c_str();
    ccinfo.origin = host.c_str();
    ccinfo.protocol = protocols[0].name;
    ccinfo.ssl_connection = useTLS ? LCCSCF_USE_SSL : 0;

    m_wsi = lws_client_connect_via_info(&ccinfo);
    if (!m_wsi) {
        Logger::error("Failed to create WebSocket connection");
        lws_context_destroy(m_lwsContext);
        m_lwsContext = nullptr;
        return false;
    }

    m_serviceThread = std::thread([this]() {
        Logger::info("libwebsockets service thread started");
        while (!m_shuttingDown.load()) {
            lws_service(m_lwsContext, 50);
        }
        Logger::info("libwebsockets service thread stopped");
    });

    return true;
}

void Gateway::disconnect() {
    if (m_shuttingDown.exchange(true))
        return;

    stopHeartbeat();
    m_connected.store(false);
    notifyConnectionState(ConnectionState::Disconnected);

    if (m_serviceThread.joinable()) {
        m_serviceThread.join();
    }

    if (m_wsi) {
        m_wsi = nullptr;
    }

    if (m_lwsContext) {
        lws_context_destroy(m_lwsContext);
        m_lwsContext = nullptr;
    }

    {
        std::scoped_lock lock(m_sendMutex);
        while (!m_sendQueue.empty()) {
            m_sendQueue.pop();
        }
    }
}

void Gateway::send(const std::string &text) {
    if (!m_connected.load() || !m_wsi)
        return;

    {
        std::scoped_lock lock(m_sendMutex);
        m_sendQueue.push(text);
    }

    lws_callback_on_writable(m_wsi);
}

void Gateway::send(const Json &j) { send(j.dump()); }

void Gateway::setIdentityProvider(IdentityProvider factory) {
    std::scoped_lock lock(m_identityMutex);
    m_identityProvider = std::move(factory);
}

Gateway::Subscription Gateway::subscribe(AnyHandler callback, bool ui) {
    const auto id = ++m_nextId;
    {
        std::scoped_lock lock(m_subscriptionMutex);
        m_subscriptions.emplace(id, AnySub{std::move(callback), ui});
    }
    return Subscription(this, id);
}

Gateway::Subscription Gateway::subscribe(const std::string &t, EventHandler callback, bool ui) {
    const auto id = ++m_nextId;
    {
        std::scoped_lock lock(m_subscriptionMutex);
        m_eventSubscriptions[t].emplace(id, EventSub{std::move(callback), ui});
    }
    return Subscription(this, id);
}

Gateway::Subscription Gateway::subscribeConnectionState(ConnectionStateHandler callback, bool ui) {
    const auto id = ++m_nextId;
    {
        std::scoped_lock lock(m_subscriptionMutex);
        m_connectionStateSubscriptions.emplace(id, ConnectionStateSub{std::move(callback), ui});
    }
    return Subscription(this, id);
}

void Gateway::unsubscribe(SubId id) {
    std::scoped_lock lock(m_subscriptionMutex);

    m_subscriptions.erase(id);
    m_connectionStateSubscriptions.erase(id);
    for (auto it = m_eventSubscriptions.begin(); it != m_eventSubscriptions.end();) {
        it->second.erase(id);
        if (it->second.empty())
            it = m_eventSubscriptions.erase(it);
        else
            ++it;
    }
}

void Gateway::receive(const std::string &text) {
    Json msg;
    try {
        msg = Json::parse(text);
    } catch (...) {
        return;
    }

    logMessage(msg);
    auto seqIt = msg.find("s");
    if (seqIt != msg.end() && seqIt->is_number()) {
        m_lastSequence.store(seqIt->get<int>());
    }

    auto opIt = msg.find("op");
    if (opIt != msg.end() && opIt->is_number()) {
        int opcode = opIt->get<int>();
        auto dataIt = msg.find("d");

        switch (opcode) {
        case 10: // HELLO
            if (dataIt != msg.end()) {
                handleHello(*dataIt);
            }
            break;

        case 11: // HEARTBEAT_ACK
            m_heartbeatAcked.store(true);
            Logger::debug("Received heartbeat ACK");
            break;

        case 7: // RECONNECT
            Logger::warn("Received RECONNECT opcode from Discord");
            handleReconnect(dataIt != msg.end() ? *dataIt : Json::object());
            break;

        case 9: // INVALID_SESSION
            if (dataIt != msg.end()) {
                Logger::warn("Received INVALID_SESSION opcode from Discord");
                handleInvalidSession(*dataIt);
            }
            break;

        default:
            break;
        }
    }

    auto it = msg.find("t");
    if (it != msg.end() && it->is_string()) {
        const std::string eventType = it->get<std::string>();
        auto dataIt = msg.find("d");
        if (dataIt != msg.end() && dataIt->is_object()) {
            static const std::unordered_map<std::string, std::function<void(Gateway *, const Json &)>> eventHandlers = {
                {"READY", [](Gateway *gw, const Json &data) { gw->handleReady(data); }},
                {"READY_SUPPLEMENTAL", [](Gateway *gw, const Json &data) { gw->handleReadySupplemental(data); }},
                {"MESSAGE_CREATE", [](Gateway *gw, const Json &data) { gw->handleMessageCreate(data); }},
                {"USER_SETTINGS_PROTO_UPDATE",
                 [](Gateway *gw, const Json &data) { gw->handleUserSettingsProtoUpdate(data); }},
                {"RESUMED", [](Gateway *, const Json &) { Logger::info("Session successfully resumed"); }},
            };

            auto handlerIt = eventHandlers.find(eventType);
            if (handlerIt != eventHandlers.end()) {
                handlerIt->second(this, *dataIt);
            } else {
                logUnhandledEvent(eventType);
            }
        }
    }

    std::vector<AnyHandler> anyUi;
    std::vector<AnyHandler> anyBg;
    std::vector<EventHandler> eventsUi;
    std::vector<EventHandler> eventsBg;

    {
        std::scoped_lock lock(m_subscriptionMutex);
        anyUi.reserve(m_subscriptions.size());
        anyBg.reserve(m_subscriptions.size());

        for (auto &kv : m_subscriptions) {
            auto &sub = kv.second;
            (sub.ui ? anyUi : anyBg).push_back(sub.callback);
        }

        auto it = msg.find("t");
        if (it != msg.end() && it->is_string()) {
            const std::string t = it->get<std::string>();
            auto jt = m_eventSubscriptions.find(t);
            if (jt != m_eventSubscriptions.end()) {
                eventsUi.reserve(jt->second.size());
                eventsBg.reserve(jt->second.size());

                for (auto &kv : jt->second) {
                    auto &sub = kv.second;
                    (sub.ui ? eventsUi : eventsBg).push_back(sub.callback);
                }
            }
        }
    }

    for (auto &callback : anyBg)
        callback(msg);
    for (auto &callback : eventsBg)
        callback(msg);

    if (!anyUi.empty() || !eventsUi.empty()) {
        dispatch([msg = std::move(msg), anyUi = std::move(anyUi), eventsUi = std::move(eventsUi)]() mutable {
            for (auto &callback : anyUi)
                callback(msg);
            for (auto &callback : eventsUi)
                callback(msg);
        });
    }
}

void Gateway::dispatch(std::function<void()> fn) {
    auto *heapFn = new std::function<void()>(std::move(fn));
    Fl::awake(
        [](void *p) {
            std::unique_ptr<std::function<void()>> fnPtr(static_cast<std::function<void()> *>(p));
            (*fnPtr)();
        },
        heapFn);
}

void Gateway::notifyConnectionState(ConnectionState state) {
    std::vector<ConnectionStateHandler> handlersUi;
    std::vector<ConnectionStateHandler> handlersBg;

    {
        std::scoped_lock lock(m_subscriptionMutex);
        handlersUi.reserve(m_connectionStateSubscriptions.size());
        handlersBg.reserve(m_connectionStateSubscriptions.size());

        for (auto &kv : m_connectionStateSubscriptions) {
            auto &sub = kv.second;
            (sub.ui ? handlersUi : handlersBg).push_back(sub.callback);
        }
    }

    for (auto &handler : handlersBg) {
        handler(state);
    }

    if (!handlersUi.empty()) {
        dispatch([state, handlersUi = std::move(handlersUi)]() {
            for (auto &handler : handlersUi) {
                handler(state);
            }
        });
    }
}

void Gateway::setAppState(std::shared_ptr<AppState> state) { m_appState = std::move(state); }

void Gateway::setDataStore(std::shared_ptr<DataStore> store) { m_dataStore = std::move(store); }

void Gateway::handleReady(const Json &data) {
    try {
        std::filesystem::create_directories("discove");
        std::ofstream file("discove/ready.json");
        if (file.is_open()) {
            file << data.dump(2);
            file.close();
            Logger::info("READY message written to discove/ready.json");
        }
    } catch (const std::exception &e) {
        Logger::error(std::string("Failed to write READY message: ") + e.what());
    }

    if (data.contains("user_settings_proto") && data["user_settings_proto"].is_string()) {
        const std::string base64Proto = data["user_settings_proto"].get<std::string>();

        std::vector<ProtobufUtils::ParsedFolder> folders;
        std::vector<uint64_t> positions;

        if (ProtobufUtils::parseGuildFoldersProto(base64Proto, folders, positions)) {

            Store::get().update([&folders, &positions](AppState &appState) {
                appState.guildFolders.clear();
                appState.guildFolders.reserve(folders.size());

                for (const auto &parsedFolder : folders) {
                    appState.guildFolders.push_back(GuildFolder::fromProtobuf(parsedFolder));
                }

                appState.guildPositions = positions;
            });

            Logger::debug("Stored " + std::to_string(folders.size()) + " guild folders and " +
                          std::to_string(positions.size()) + " guild positions in AppState");
        } else {
            Logger::warn("Failed to parse user_settings_proto guild folders");
        }
    }

    ReadyState state;
    if (data.contains("session_id") && data["session_id"].is_string()) {
        state.sessionId = data["session_id"].get<std::string>();
        {
            std::scoped_lock lock(m_sessionMutex);
            m_sessionId = state.sessionId;
        }
    }

    if (data.contains("resume_gateway_url") && data["resume_gateway_url"].is_string()) {
        std::scoped_lock lock(m_sessionMutex);
        m_resumeGatewayUrl = data["resume_gateway_url"].get<std::string>();
        Logger::info("Stored resume gateway URL: " + m_resumeGatewayUrl);
    }

    if (data.contains("user") && data["user"].is_object()) {
        const auto &user = data["user"];

        if (user.contains("id") && user["id"].is_string()) {
            state.userId = user["id"].get<std::string>();
        }

        if (user.contains("username") && user["username"].is_string()) {
            state.username = user["username"].get<std::string>();
        }

        if (user.contains("discriminator") && user["discriminator"].is_string()) {
            state.discriminator = user["discriminator"].get<std::string>();
        }

        if (user.contains("global_name") && user["global_name"].is_string()) {
            state.globalName = user["global_name"].get<std::string>();
        }

        if (user.contains("avatar") && user["avatar"].is_string()) {
            state.avatar = user["avatar"].get<std::string>();
        }
    }

    std::string userStatus = "online";
    std::optional<CustomStatus> customStatus;

    if (data.contains("sessions") && data["sessions"].is_array() && !data["sessions"].empty()) {
        Logger::debug("Found sessions array with " + std::to_string(data["sessions"].size()) + " sessions");
        const auto &session = data["sessions"][0];

        if (session.contains("status") && session["status"].is_string()) {
            userStatus = session["status"].get<std::string>();
            Logger::debug("User status: " + userStatus);
        }

        if (session.contains("activities") && session["activities"].is_array()) {
            Logger::debug("Found " + std::to_string(session["activities"].size()) + " activities");

            for (const auto &activity : session["activities"]) {
                if (activity.contains("type") && activity["type"].is_number() && activity["type"].get<int>() == 4) {
                    Logger::debug("Found custom status activity (type 4)");

                    CustomStatus cs;

                    if (activity.contains("state") && activity["state"].is_string()) {
                        cs.text = activity["state"].get<std::string>();
                        Logger::debug("Custom status text: " + cs.text);
                    }

                    if (activity.contains("emoji") && activity["emoji"].is_object()) {
                        const auto &emoji = activity["emoji"];
                        if (emoji.contains("name") && emoji["name"].is_string()) {
                            cs.emojiName = emoji["name"].get<std::string>();
                            Logger::debug("Custom status emoji name: " + cs.emojiName);
                        }
                        if (emoji.contains("id") && emoji["id"].is_string()) {
                            cs.emojiId = emoji["id"].get<std::string>();
                            Logger::debug("Custom status emoji ID: " + cs.emojiId);
                        }
                        if (emoji.contains("animated") && emoji["animated"].is_boolean()) {
                            cs.emojiAnimated = emoji["animated"].get<bool>();
                            Logger::debug("Custom status emoji animated: " +
                                          std::string(cs.emojiAnimated ? "true" : "false"));
                        }

                        if (!cs.emojiId.empty()) {
                            std::string ext = cs.emojiAnimated ? "gif" : "png";
                            cs.emojiUrl = "https://cdn.discordapp.com/emojis/" + cs.emojiId + "." + ext;
                            Logger::info("Custom status emoji URL: " + cs.emojiUrl);
                        }
                    }

                    if (!cs.isEmpty()) {
                        customStatus = cs;
                        Logger::info("Parsed custom status: text='" + cs.text + "', emojiUrl='" + cs.emojiUrl + "'");
                    }
                    break;
                }
            }
        }
    } else {
        Logger::warn("No sessions found in READY message");
    }

    Store::get().update([&state, &userStatus, &customStatus](AppState &appState) {
        UserProfile profile;
        profile.id = state.userId;
        profile.username = state.username;
        profile.discriminator = state.discriminator;
        profile.globalName = state.globalName;
        profile.avatarHash = state.avatar;
        profile.status = userStatus;
        profile.customStatus = customStatus;

        if (!state.avatar.empty()) {
            profile.avatarUrl = CDNUtils::getUserAvatarUrl(state.userId, state.avatar);
        } else {
            profile.avatarUrl = CDNUtils::getDefaultAvatarUrl(state.userId);
        }

        appState.currentUser = std::move(profile);
    });

    std::vector<GuildInfo> guildsForState;
    std::unordered_map<std::string, std::vector<std::shared_ptr<GuildChannel>>> allGuildChannels;
    std::unordered_map<std::string, std::vector<Role>> allGuildRoles;

    if (data.contains("guilds") && data["guilds"].is_array()) {
        Logger::debug("Found guilds array with " + std::to_string(data["guilds"].size()) + " guilds");
        for (const auto &guildJson : data["guilds"]) {
            GuildSummary guild;
            GuildInfo guildInfo;

            if (guildJson.contains("id") && guildJson["id"].is_string()) {
                guild.id = guildJson["id"].get<std::string>();
                guildInfo.id = guild.id;
            }

            if (guildJson.contains("properties") && guildJson["properties"].is_object()) {
                const auto &props = guildJson["properties"];

                if (props.contains("name") && props["name"].is_string()) {
                    guild.name = props["name"].get<std::string>();
                    guildInfo.name = guild.name;
                }

                if (props.contains("icon") && props["icon"].is_string()) {
                    guild.icon = props["icon"].get<std::string>();
                    guildInfo.icon = guild.icon;
                }

                if (props.contains("banner") && props["banner"].is_string()) {
                    guild.banner = props["banner"].get<std::string>();
                    guildInfo.banner = guild.banner;
                }

                if (props.contains("rules_channel_id") && props["rules_channel_id"].is_string()) {
                    guild.rulesChannelId = props["rules_channel_id"].get<std::string>();
                    guildInfo.rulesChannelId = guild.rulesChannelId.value_or("");
                }

                if (props.contains("premium_tier") && props["premium_tier"].is_number()) {
                    guildInfo.premiumTier = props["premium_tier"].get<int>();
                }

                if (props.contains("premium_subscription_count") && props["premium_subscription_count"].is_number()) {
                    guildInfo.premiumSubscriptionCount = props["premium_subscription_count"].get<int>();
                }
            } else {
                if (guildJson.contains("name") && guildJson["name"].is_string()) {
                    guild.name = guildJson["name"].get<std::string>();
                    guildInfo.name = guild.name;
                }

                if (guildJson.contains("icon") && guildJson["icon"].is_string()) {
                    guild.icon = guildJson["icon"].get<std::string>();
                    guildInfo.icon = guild.icon;
                }

                if (guildJson.contains("banner") && guildJson["banner"].is_string()) {
                    guild.banner = guildJson["banner"].get<std::string>();
                    guildInfo.banner = guild.banner;
                }

                if (guildJson.contains("rules_channel_id") && guildJson["rules_channel_id"].is_string()) {
                    guild.rulesChannelId = guildJson["rules_channel_id"].get<std::string>();
                    guildInfo.rulesChannelId = guild.rulesChannelId.value_or("");
                }

                if (guildJson.contains("premium_tier") && guildJson["premium_tier"].is_number()) {
                    guildInfo.premiumTier = guildJson["premium_tier"].get<int>();
                }

                if (guildJson.contains("premium_subscription_count") &&
                    guildJson["premium_subscription_count"].is_number()) {
                    guildInfo.premiumSubscriptionCount = guildJson["premium_subscription_count"].get<int>();
                }
            }

            if (guildJson.contains("channels") && guildJson["channels"].is_array() && !guild.id.empty()) {
                std::vector<std::shared_ptr<GuildChannel>> guildChannelsList;
                for (const auto &channelJson : guildJson["channels"]) {
                    auto channel = Channel::fromJson(channelJson);
                    if (auto *guildChannel = dynamic_cast<GuildChannel *>(channel.get())) {
                        guildChannelsList.push_back(
                            std::shared_ptr<GuildChannel>(static_cast<GuildChannel *>(channel.release())));
                    }
                }
                if (!guildChannelsList.empty()) {
                    allGuildChannels[guild.id] = std::move(guildChannelsList);
                }
            }

            if (guildJson.contains("roles") && guildJson["roles"].is_array() && !guild.id.empty()) {
                std::vector<Role> rolesList;
                for (const auto &roleJson : guildJson["roles"]) {
                    rolesList.push_back(Role::fromJson(roleJson));
                }
                if (!rolesList.empty()) {
                    allGuildRoles[guild.id] = std::move(rolesList);
                }
            }

            if (!guild.id.empty()) {
                guildsForState.push_back(std::move(guildInfo));
            }
        }
    }

    std::unordered_map<std::string, GuildMember> guildMembersMap;
    if (data.contains("merged_members") && data["merged_members"].is_array()) {
        const auto &mergedMembers = data["merged_members"];
        Logger::debug("Found merged_members array with " + std::to_string(mergedMembers.size()) + " entries");

        size_t guildIndex = 0;
        if (data.contains("guilds") && data["guilds"].is_array()) {
            const auto &guildsArray = data["guilds"];

            for (size_t i = 0; i < mergedMembers.size() && i < guildsArray.size(); ++i) {
                if (!mergedMembers[i].is_array() || mergedMembers[i].empty()) {
                    continue;
                }

                std::string guildId;
                if (guildsArray[i].contains("id") && guildsArray[i]["id"].is_string()) {
                    guildId = guildsArray[i]["id"].get<std::string>();
                } else {
                    continue;
                }

                const auto &memberJson = mergedMembers[i][0];
                GuildMember member = GuildMember::fromJson(memberJson);

                if (!member.userId.empty()) {
                    guildMembersMap[guildId] = std::move(member);
                }
            }
        }
    }

    size_t guildCount = guildsForState.size();
    size_t totalChannelCount = 0;
    for (const auto &[guildId, channels] : allGuildChannels) {
        totalChannelCount += channels.size();
    }

    Store::get().update([guildsForState = std::move(guildsForState), allGuildChannels = std::move(allGuildChannels),
                         allGuildRoles = std::move(allGuildRoles),
                         guildMembersMap = std::move(guildMembersMap)](AppState &appState) mutable {
        appState.guilds = std::move(guildsForState);
        appState.guildChannels = std::move(allGuildChannels);
        appState.guildRoles = std::move(allGuildRoles);
        appState.guildMembers = std::move(guildMembersMap);
    });

    Logger::info("Stored " + std::to_string(guildCount) + " guilds and " + std::to_string(totalChannelCount) +
                 " total channels in AppState");

    if (data.contains("private_channels") && data["private_channels"].is_array()) {
        Logger::info("Processing " + std::to_string(data["private_channels"].size()) + " private channels");

        std::vector<std::shared_ptr<DMChannel>> privateChannels;
        for (const auto &privateChannelJson : data["private_channels"]) {
            auto channel = Channel::fromJson(privateChannelJson);
            if (auto *dmChannel = dynamic_cast<DMChannel *>(channel.get())) {
                privateChannels.push_back(std::shared_ptr<DMChannel>(static_cast<DMChannel *>(channel.release())));
            }
        }

        size_t dmCount = privateChannels.size();
        Store::get().update([privateChannels = std::move(privateChannels)](AppState &appState) mutable {
            appState.privateChannels = std::move(privateChannels);
        });

        Logger::info("Stored " + std::to_string(dmCount) + " DM channels in AppState");
    }
    if (data.contains("users") && data["users"].is_array()) {
        Logger::info("Processing " + std::to_string(data["users"].size()) + " users for collectibles");

        size_t userCount = 0;
        size_t avatarDecorationCount = 0;
        size_t nameplateCount = 0;

        for (const auto &userJson : data["users"]) {
            try {
                User user = User::fromJson(userJson);
                userCount++;

                std::string username = user.getDisplayName();
                if (username.empty()) {
                    username = user.username;
                }

                std::string decorationUrl = user.getAvatarDecorationUrl();
                if (!decorationUrl.empty()) {
                    avatarDecorationCount++;
                }

                std::string nameplateUrl = user.getNameplateUrl();
                if (!nameplateUrl.empty()) {
                    nameplateCount++;
                }
            } catch (const std::exception &e) {
                Logger::warn("Failed to parse user for collectibles: " + std::string(e.what()));
            }
        }

        Logger::info("Collectibles summary: " + std::to_string(avatarDecorationCount) + " avatar decorations, " +
                     std::to_string(nameplateCount) + " nameplates from " + std::to_string(userCount) + " users");
    }
}

void Gateway::handleReadySupplemental(const Json &data) {
    try {
        std::filesystem::create_directories("discove");
        std::ofstream file("discove/ready_supplemental.json");
        if (file.is_open()) {
            file << data.dump(2);
            file.close();
            Logger::info("READY_SUPPLEMENTAL message written to discove/ready_supplemental.json");
        }
    } catch (const std::exception &e) {
        Logger::error(std::string("Failed to write READY_SUPPLEMENTAL message: ") + e.what());
    }

    if (data.contains("guilds") && data["guilds"].is_array()) {
        const size_t guildCount = data["guilds"].size();
        (void)guildCount;
    }
}

void Gateway::handleMessageCreate(const Json &data) {
    std::string messageId;
    std::string channelId;
    std::string content;
    long long timestampMs = 0;

    if (data.contains("id") && data["id"].is_string()) {
        messageId = data["id"].get<std::string>();
    }

    if (data.contains("channel_id") && data["channel_id"].is_string()) {
        channelId = data["channel_id"].get<std::string>();
    }

    if (data.contains("content") && data["content"].is_string()) {
        content = data["content"].get<std::string>();
    }

    auto now = std::chrono::system_clock::now();
    timestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::string authorId;
    std::string authorUsername;
    std::string authorDiscriminator;
    std::string authorGlobalName;
    std::string authorAvatar;
    bool isBot = false;

    if (data.contains("author") && data["author"].is_object()) {
        const auto &author = data["author"];

        if (author.contains("id") && author["id"].is_string()) {
            authorId = author["id"].get<std::string>();
        }

        if (author.contains("username") && author["username"].is_string()) {
            authorUsername = author["username"].get<std::string>();
        }

        if (author.contains("discriminator") && author["discriminator"].is_string()) {
            authorDiscriminator = author["discriminator"].get<std::string>();
        }

        if (author.contains("global_name") && author["global_name"].is_string()) {
            authorGlobalName = author["global_name"].get<std::string>();
        }

        if (author.contains("avatar") && author["avatar"].is_string()) {
            authorAvatar = author["avatar"].get<std::string>();
        }

        if (author.contains("bot") && author["bot"].is_boolean()) {
            isBot = author["bot"].get<bool>();
        }
    }

    if (messageId.empty() || channelId.empty() || authorId.empty()) {
        return;
    }
}

void Gateway::handleUserSettingsProtoUpdate(const Json &data) {
    std::string base64Proto;
    if (data.contains("settings") && data["settings"].is_object()) {
        const auto &settings = data["settings"];

        if (settings.contains("proto") && settings["proto"].is_string()) {
            base64Proto = settings["proto"].get<std::string>();

            int settingsType = -1;
            if (settings.contains("type") && settings["type"].is_number()) {
                settingsType = settings["type"].get<int>();
            }

            ProtobufUtils::ParsedStatus status;
            bool statusParsed = ProtobufUtils::parseStatusProto(base64Proto, status);

            Logger::debug("Status parsing result: " + std::string(statusParsed ? "success" : "failed") +
                          (statusParsed ? ", status=" + status.status : ""));

            if (statusParsed) {
                Store::get().update([&status](AppState &appState) {
                    if (appState.currentUser.has_value()) {
                        appState.currentUser->status = status.status;
                    }
                });

                Logger::info("Updated user status to: " + status.status);
            }

            std::vector<ProtobufUtils::ParsedFolder> folders;
            std::vector<uint64_t> positions;

            if (ProtobufUtils::parseGuildFoldersProto(base64Proto, folders, positions)) {
                Store::get().update([&folders, &positions](AppState &appState) {
                    appState.guildFolders.clear();
                    appState.guildFolders.reserve(folders.size());

                    for (const auto &parsedFolder : folders) {
                        appState.guildFolders.push_back(GuildFolder::fromProtobuf(parsedFolder));
                    }

                    appState.guildPositions = positions;
                });

                Logger::info("Updated AppState with new guild folders from proto update");
            }

            if (!statusParsed && folders.empty()) {
                Logger::debug(
                    "Proto update contained neither status nor guild folders (may contain other settings data)");
            }
        }
    }
}

void Gateway::handleHello(const Json &data) {
    if (data.contains("heartbeat_interval") && data["heartbeat_interval"].is_number()) {
        int interval = data["heartbeat_interval"].get<int>();
        Logger::info("Received HELLO with heartbeat interval: " + std::to_string(interval) + "ms");
        startHeartbeat(interval);
    }

    std::string sessionId;
    int lastSeq = m_lastSequence.load();
    {
        std::scoped_lock lock(m_sessionMutex);
        sessionId = m_sessionId;
    }

    if (!sessionId.empty() && lastSeq > 0) {
        Json resume;
        resume["op"] = 6;
        resume["d"] = {{"token", ""}, {"session_id", sessionId}, {"seq", lastSeq}};

        IdentityProvider identityProvider;
        {
            std::scoped_lock lock(m_identityMutex);
            identityProvider = m_identityProvider;
        }

        if (identityProvider) {
            Json identifyPayload = identityProvider();
            if (identifyPayload.contains("d") && identifyPayload["d"].contains("token")) {
                resume["d"]["token"] = identifyPayload["d"]["token"];
            }
        }

        send(resume);
        Logger::info("Sent RESUME (session_id: " + sessionId + ", seq: " + std::to_string(lastSeq) + ")");
    } else {
        IdentityProvider identityProvider;
        {
            std::scoped_lock lock(m_identityMutex);
            identityProvider = m_identityProvider;
        }

        if (identityProvider) {
            send(identityProvider());
            Logger::info("Sent IDENTIFY");
        }
    }
}

static void heartbeatTimerCallback(void *userData) {
    Gateway *gw = static_cast<Gateway *>(userData);
    if (gw && gw->isConnected()) {
        gw->sendHeartbeat();
    }
}

void Gateway::startHeartbeat(int intervalMs) {
    stopHeartbeat();
    m_heartbeatInterval.store(intervalMs);
    m_heartbeatRunning.store(true);
    m_heartbeatAcked.store(true);

    Fl::add_timeout(static_cast<double>(intervalMs) / 1000.0, heartbeatTimerCallback, this);
    Logger::info("Started heartbeat with interval: " + std::to_string(intervalMs) + "ms");
}

void Gateway::sendHeartbeat() {
    if (!m_heartbeatRunning.load() || !m_connected.load()) {
        return;
    }

    if (!m_heartbeatAcked.load()) {
        Logger::warn("Heartbeat ACK not received - connection may be zombied, reconnecting");
        dispatch([this]() { reconnect(true); });
        return;
    }

    m_heartbeatAcked.store(false);

    Json heartbeat;
    heartbeat["op"] = 1;
    int seq = m_lastSequence.load();
    heartbeat["d"] = (seq == -1) ? Json(nullptr) : Json(seq);

    send(heartbeat);
    Logger::debug("Sent heartbeat (seq: " + (seq == -1 ? std::string("null") : std::to_string(seq)) + ")");

    int interval = m_heartbeatInterval.load();
    if (interval > 0) {
        Fl::add_timeout(static_cast<double>(interval) / 1000.0, heartbeatTimerCallback, this);
    }
}

void Gateway::stopHeartbeat() {
    m_heartbeatRunning.store(false);
    Fl::remove_timeout(heartbeatTimerCallback, this);
    Logger::debug("Stopped heartbeat");
}

void Gateway::handleReconnect(const Json &data) {
    Logger::warn("Discord requested reconnect - attempting to resume session");
    dispatch([this]() { reconnect(true); });
}

void Gateway::handleInvalidSession(const Json &data) {
    bool canResume = false;
    if (data.is_boolean()) {
        canResume = data.get<bool>();
    }

    if (canResume) {
        Logger::warn("Session invalid but resumable - reconnecting with resume");
        dispatch([this]() {
            Fl::add_timeout(
                1.0 + (std::rand() % 5),
                [](void *userData) {
                    Gateway *gw = static_cast<Gateway *>(userData);
                    if (gw) {
                        gw->reconnect(true);
                    }
                },
                this);
        });
    } else {
        Logger::warn("Session invalid and not resumable - clearing session and reconnecting");
        {
            std::scoped_lock lock(m_sessionMutex);
            m_sessionId.clear();
            m_resumeGatewayUrl.clear();
        }
        m_lastSequence.store(-1);
        dispatch([this]() {
            Fl::add_timeout(
                1.0 + (std::rand() % 5),
                [](void *userData) {
                    Gateway *gw = static_cast<Gateway *>(userData);
                    if (gw) {
                        gw->reconnect(false);
                    }
                },
                this);
        });
    }
}

void Gateway::reconnect(bool resume) {
    if (m_shuttingDown.load()) {
        return;
    }

    Logger::info("Reconnecting to gateway" + std::string(resume ? " (resume)" : " (fresh)"));

    stopHeartbeat();
    m_connected.store(false);

    if (!resume) {
        std::scoped_lock lock(m_sessionMutex);
        m_sessionId.clear();
        m_resumeGatewayUrl.clear();
        m_lastSequence.store(-1);
    }

    std::string url;
    {
        std::scoped_lock lock(m_sessionMutex);
        url = resume ? m_resumeGatewayUrl : "";
    }

    if (url.empty()) {
        url = m_options.url;
    }

    disconnect();

    dispatch([this, url]() {
        Fl::add_timeout(
            0.5,
            [](void *userData) {
                auto *data = static_cast<std::pair<Gateway *, std::string> *>(userData);
                Gateway *gw = data->first;
                std::string reconnectUrl = data->second;
                delete data;

                if (gw && !gw->m_shuttingDown.load()) {
                    Options opts = gw->m_options;
                    opts.url = reconnectUrl;
                    gw->connect(opts);
                }
            },
            new std::pair<Gateway *, std::string>(this, url));
    });
}

void Gateway::logMessage(const Json &msg) {
    try {
        std::filesystem::create_directories("discove");
        std::ofstream file("discove/gateway_messages.jsonl", std::ios::app);
        if (file.is_open()) {
            file << msg.dump() << "\n";
            file.close();
        }
    } catch (const std::exception &e) {
        Logger::error(std::string("Failed to log gateway message: ") + e.what());
    }
}

void Gateway::logUnhandledEvent(const std::string &eventType) {
    Logger::debug("Unhandled event type: " + eventType);

    try {
        std::filesystem::create_directories("discove");
        std::ofstream file("discove/unhandled_events.log", std::ios::app);
        if (file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::string timeStr = std::ctime(&time);
            timeStr.pop_back();

            file << "[" << timeStr << "] Unhandled event: " << eventType << "\n";
            file.close();
        }
    } catch (const std::exception &e) {
        Logger::error(std::string("Failed to log unhandled event: ") + e.what());
    }
}
