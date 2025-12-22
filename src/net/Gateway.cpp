#include "net/Gateway.h"

#include "state/Store.h"

#include <chrono>
#include <memory>
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
            {
                IdentityProvider IdentityProvider;
                {
                    std::scoped_lock lock(m_identityMutex);
                    IdentityProvider = m_identityProvider;
                }
                if (IdentityProvider) {
                    send(IdentityProvider());
                }
            }
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

    auto it = msg.find("t");
    if (it != msg.end() && it->is_string()) {
        const std::string eventType = it->get<std::string>();
        auto dataIt = msg.find("d");
        if (dataIt != msg.end() && dataIt->is_object()) {
            if (eventType == "READY") {
                handleReady(*dataIt);
            } else if (eventType == "MESSAGE_CREATE") {
                handleMessageCreate(*dataIt);
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
    ReadyState state;
    if (data.contains("session_id") && data["session_id"].is_string()) {
        state.sessionId = data["session_id"].get<std::string>();
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

    // TODO: Update AppState when UserProfile support is added
    // if (m_appState) {
    //     AppState::UserProfile profile;
    //     profile.id = state.userId;
    //     profile.username = state.username;
    //     profile.discriminator = state.discriminator;
    //     profile.globalName = state.globalName;
    //     profile.avatar = state.avatar;
    //     m_appState->setSessionId(state.sessionId);
    //     m_appState->setUserProfile(std::move(profile));
    // }

    m_guilds.clear();
    if (data.contains("guilds") && data["guilds"].is_array()) {
        for (const auto &guildJson : data["guilds"]) {
            GuildSummary guild;

            if (guildJson.contains("id") && guildJson["id"].is_string()) {
                guild.id = guildJson["id"].get<std::string>();
            }

            if (guildJson.contains("properties") && guildJson["properties"].is_object()) {
                const auto &props = guildJson["properties"];

                if (props.contains("name") && props["name"].is_string()) {
                    guild.name = props["name"].get<std::string>();
                }

                if (props.contains("icon") && props["icon"].is_string()) {
                    guild.icon = props["icon"].get<std::string>();
                }

                if (props.contains("banner") && props["banner"].is_string()) {
                    guild.banner = props["banner"].get<std::string>();
                }
            } else {
                if (guildJson.contains("name") && guildJson["name"].is_string()) {
                    guild.name = guildJson["name"].get<std::string>();
                }

                if (guildJson.contains("icon") && guildJson["icon"].is_string()) {
                    guild.icon = guildJson["icon"].get<std::string>();
                }

                if (guildJson.contains("banner") && guildJson["banner"].is_string()) {
                    guild.banner = guildJson["banner"].get<std::string>();
                }
            }

            if (!guild.id.empty()) {
                m_guilds.push_back(std::move(guild));
            }
        }
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
