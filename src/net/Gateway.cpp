#include "net/Gateway.h"

#include "models/GuildFolder.h"
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

Gateway *Gateway::s_instance = nullptr;

Gateway &Gateway::get() {
    if (!s_instance) {
        s_instance = new Gateway();
    }
    return *s_instance;
}

Gateway::Gateway() {
    ix::initNetSystem();
    m_ws.setPingInterval(0);
    m_ws.disableAutomaticReconnection();
}

Gateway::~Gateway() {
    disconnect();
    ix::uninitNetSystem();
}

bool Gateway::connect(const Options &opt) {
    if (m_connected.load())
        return true;

    m_options = opt;
    m_shuttingDown.store(false);

    m_ws.setUrl(m_options.url);
    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
        if (m_shuttingDown.load())
            return;

        switch (msg->type) {
        case ix::WebSocketMessageType::Open:
            m_connected.store(true);
            notifyConnectionState(ConnectionState::Connected);
            // Identity/Resume will be sent after receiving HELLO
            break;

        case ix::WebSocketMessageType::Message:
            receive(msg->str);
            break;

        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error:
            m_connected.store(false);
            notifyConnectionState(ConnectionState::Disconnected);
            break;

        default:
            break;
        }
    });

    m_ws.start();
    return true;
}

void Gateway::disconnect() {
    if (m_shuttingDown.exchange(true))
        return;

    stopHeartbeat();
    m_connected.store(false);
    notifyConnectionState(ConnectionState::Disconnected);
    m_ws.stop();
    m_ws.setOnMessageCallback(nullptr);
}

void Gateway::send(const std::string &text) {
    if (!m_connected.load())
        return;
    m_ws.send(text);
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

    auto seqIt = msg.find("s");
    if (seqIt != msg.end() && seqIt->is_number()) {
        m_lastSequence.store(seqIt->get<int>());
    }

    auto opIt = msg.find("op");
    if (opIt != msg.end() && opIt->is_number()) {
        int opcode = opIt->get<int>();
        auto dataIt = msg.find("d");

        if (opcode == 10 && dataIt != msg.end()) {
            // HELLO
            handleHello(*dataIt);
        } else if (opcode == 11) {
            // HEARTBEAT_ACK
            m_heartbeatAcked.store(true);
            Logger::debug("Received heartbeat ACK");
        } else if (opcode == 7) {
            // RECONNECT
            Logger::warn("Received RECONNECT opcode from Discord");
            handleReconnect(dataIt != msg.end() ? *dataIt : Json::object());
        } else if (opcode == 9 && dataIt != msg.end()) {
            // INVALID_SESSION
            Logger::warn("Received INVALID_SESSION opcode from Discord");
            handleInvalidSession(*dataIt);
        }
    }

    auto it = msg.find("t");
    if (it != msg.end() && it->is_string()) {
        const std::string eventType = it->get<std::string>();
        auto dataIt = msg.find("d");
        if (dataIt != msg.end() && dataIt->is_object()) {
            if (eventType == "READY") {
                handleReady(*dataIt);
            } else if (eventType == "READY_SUPPLEMENTAL") {
                handleReadySupplemental(*dataIt);
            } else if (eventType == "MESSAGE_CREATE") {
                handleMessageCreate(*dataIt);
            } else if (eventType == "RESUMED") {
                Logger::info("Session successfully resumed");
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
            Logger::info("Parsed " + std::to_string(folders.size()) + " guild folders and " +
                         std::to_string(positions.size()) + " guild positions");

            Store::get().update([&folders, &positions](AppState &appState) {
                appState.guildFolders.clear();
                appState.guildFolders.reserve(folders.size());

                for (const auto &parsedFolder : folders) {
                    appState.guildFolders.push_back(GuildFolder::fromProtobuf(parsedFolder));
                }

                appState.guildPositions = positions;
            });

            Logger::info("Stored " + std::to_string(folders.size()) + " guild folders and " +
                         std::to_string(positions.size()) + " guild positions in AppState");

            try {
                std::ofstream folderFile("discove/guild_folders.txt");
                if (folderFile.is_open()) {
                    folderFile << "Guild Folders (" << folders.size() << "):\n";
                    folderFile << "=====================================\n\n";

                    for (size_t i = 0; i < folders.size(); ++i) {
                        const auto &folder = folders[i];
                        folderFile << "Folder #" << (i + 1) << ":\n";
                        folderFile << "  ID: " << folder.id << "\n";
                        folderFile << "  Name: " << (folder.name.empty() ? "(unnamed)" : folder.name) << "\n";

                        if (folder.hasColor) {
                            std::ostringstream colorHex;
                            colorHex << "#" << std::hex << std::uppercase << folder.color;
                            folderFile << "  Color: " << colorHex.str() << "\n";
                        } else {
                            folderFile << "  Color: (default)\n";
                        }

                        folderFile << "  Guild IDs (" << folder.guildIds.size() << "):\n";
                        for (const auto &gid : folder.guildIds) {
                            folderFile << "    - " << gid << "\n";
                        }
                        folderFile << "\n";
                    }

                    folderFile << "\nGuild Positions (" << positions.size() << "):\n";
                    folderFile << "=====================================\n";
                    for (size_t i = 0; i < positions.size(); ++i) {
                        folderFile << i + 1 << ". " << positions[i] << "\n";
                    }

                    folderFile.close();
                    Logger::info("Guild folder data written to discove/guild_folders.txt");
                }
            } catch (const std::exception &e) {
                Logger::error(std::string("Failed to write guild folders: ") + e.what());
            }
        } else {
            Logger::warn("Failed to parse user_settings_proto guild folders");
        }
    }

    ReadyState state;
    if (data.contains("session_id") && data["session_id"].is_string()) {
        state.sessionId = data["session_id"].get<std::string>();

        // Store session_id for resuming
        {
            std::scoped_lock lock(m_sessionMutex);
            m_sessionId = state.sessionId;
        }
    }

    // Store resume_gateway_url if provided
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

    m_readyState = state;

    Store::get().update([&state](AppState &appState) {
        UserProfile profile;
        profile.id = state.userId;
        profile.username = state.username;
        profile.discriminator = state.discriminator;
        profile.globalName = state.globalName;
        profile.avatarHash = state.avatar;

        if (!state.avatar.empty()) {
            profile.avatarUrl = CDNUtils::getUserAvatarUrl(state.userId, state.avatar);
        } else {
            profile.avatarUrl = CDNUtils::getDefaultAvatarUrl(state.userId);
        }

        appState.currentUser = std::move(profile);
    });

    m_guilds.clear();
    std::vector<GuildInfo> guildsForState;

    if (data.contains("guilds") && data["guilds"].is_array()) {
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
            }

            if (!guild.id.empty()) {
                m_guilds.push_back(std::move(guild));
                guildsForState.push_back(std::move(guildInfo));
            }
        }
    }

    Store::get().update([guildsForState = std::move(guildsForState)](AppState &appState) mutable {
        appState.guilds = std::move(guildsForState);
    });

    Logger::info("Stored " + std::to_string(m_guilds.size()) + " guilds in AppState");

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

    // READY_SUPPLEMENTAL contains additional data:
    // - merged_presences: presence data for guild members
    // - merged_members: additional member data for guilds
    // - lazy_private_channels: private channel data
    // - guilds: supplemental guild information

    // TODO: When DataStore is implemented, process and store:
    // - Merged presences for online status tracking
    // - Merged members for member cache
    // - Lazy private channels for DM support
    // - Guild supplemental data

    // For now, just log that we received it
    if (data.contains("guilds") && data["guilds"].is_array()) {
        const size_t guildCount = data["guilds"].size();
        (void)guildCount;
    }
}

void Gateway::handleMessageCreate(const Json &data) {
    // TODO: Store messages in DataStore
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

    // TODO: When DataStore is implemented, store the message:
    // m_dataStore->upsertUser(authorId, authorUsername, authorDiscriminator, isBot, authorGlobalName, authorAvatar);
    // m_dataStore->upsertMessage(messageId, channelId, authorId, content, timestampMs);
}

void Gateway::handleHello(const Json &data) {
    if (data.contains("heartbeat_interval") && data["heartbeat_interval"].is_number()) {
        int interval = data["heartbeat_interval"].get<int>();
        Logger::info("Received HELLO with heartbeat interval: " + std::to_string(interval) + "ms");
        startHeartbeat(interval);
    }

    // Check if we can resume the session
    std::string sessionId;
    int lastSeq = m_lastSequence.load();
    {
        std::scoped_lock lock(m_sessionMutex);
        sessionId = m_sessionId;
    }

    if (!sessionId.empty() && lastSeq > 0) {
        // Send RESUME (opcode 6)
        Json resume;
        resume["op"] = 6;
        resume["d"] = {{"token", ""}, {"session_id", sessionId}, {"seq", lastSeq}};

        // Get token from identity provider
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
        // Send IDENTIFY (opcode 2)
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
    m_heartbeatAcked.store(true); // Reset ACK flag for new session

    Fl::add_timeout(static_cast<double>(intervalMs) / 1000.0, heartbeatTimerCallback, this);
    Logger::info("Started heartbeat with interval: " + std::to_string(intervalMs) + "ms");
}

void Gateway::sendHeartbeat() {
    if (!m_heartbeatRunning.load() || !m_connected.load()) {
        return;
    }

    // Check if previous heartbeat was acknowledged
    if (!m_heartbeatAcked.load()) {
        Logger::warn("Heartbeat ACK not received - connection may be zombied, reconnecting");
        dispatch([this]() { reconnect(true); });
        return;
    }

    // Mark as waiting for ACK
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
    // Discord wants us to reconnect and resume the session
    // We should close the connection and reconnect with resume
    dispatch([this]() { reconnect(true); });
}

void Gateway::handleInvalidSession(const Json &data) {
    bool canResume = false;
    if (data.is_boolean()) {
        canResume = data.get<bool>();
    }

    if (canResume) {
        Logger::warn("Session invalid but resumable - reconnecting with resume");
        // Wait a bit before reconnecting as per Discord recommendations
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
        // Clear session data and reconnect with fresh IDENTIFY
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

    // Stop current connection
    stopHeartbeat();
    m_connected.store(false);

    // If not resuming, clear session data
    if (!resume) {
        std::scoped_lock lock(m_sessionMutex);
        m_sessionId.clear();
        m_resumeGatewayUrl.clear();
        m_lastSequence.store(-1);
    }

    // Get the URL to connect to
    std::string url;
    {
        std::scoped_lock lock(m_sessionMutex);
        url = resume ? m_resumeGatewayUrl : "";
    }

    if (url.empty()) {
        url = m_options.url;
    }

    // Stop the websocket and wait a bit before reconnecting
    m_ws.stop();

    // Schedule reconnection after a brief delay to ensure clean disconnect
    dispatch([this, url]() {
        Fl::add_timeout(
            0.5, // Wait 500ms for clean disconnect
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
