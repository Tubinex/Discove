#pragma once

#include <FL/Fl.H>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <libwebsockets.h>
#include <nlohmann/json.hpp>

class AppState;
class DataStore;

void heartbeatTimerCallback(void *userData);
int lwsCallback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len);

class Gateway {
    friend void heartbeatTimerCallback(void *userData);
    friend int lwsCallback(lws *wsi, lws_callback_reasons reason, void *user, void *in, size_t len);

  public:
    using Json = nlohmann::json;
    using SubId = uint64_t;

    enum class ConnectionState { Disconnected, Connected };

    struct ReadyState {
        std::string sessionId;
        std::string userId;
        std::string username;
        std::string discriminator;
        std::string globalName;
        std::string avatar;
    };

    struct GuildSummary {
        std::string id;
        std::string name;
        std::string icon;
        std::string banner;
    };

    using AnyHandler = std::function<void(const Json &)>;
    using EventHandler = std::function<void(const Json &)>;
    using ConnectionStateHandler = std::function<void(ConnectionState)>;
    using IdentityProvider = std::function<Json()>;

    struct Subscription {
        Subscription() = default;
        Subscription(Gateway *gw, SubId id) : m_gw(gw), m_id(id) {}

        Subscription(const Subscription &) = delete;
        Subscription &operator=(const Subscription &) = delete;

        Subscription(Subscription &&other) noexcept { *this = std::move(other); }
        Subscription &operator=(Subscription &&other) noexcept {
            if (this != &other) {
                reset();
                m_gw = other.m_gw;
                m_id = other.m_id;
                other.m_gw = nullptr;
                other.m_id = 0;
            }
            return *this;
        }

        ~Subscription() { reset(); }

        void reset() {
            if (m_gw && m_id)
                m_gw->unsubscribe(m_id);
            m_gw = nullptr;
            m_id = 0;
        }

        explicit operator bool() const { return m_gw != nullptr && m_id != 0; }

      private:
        Gateway *m_gw = nullptr;
        SubId m_id = 0;
    };

    struct Options {
        std::string url = "wss://gateway.discord.gg/?v=10&encoding=json";
    };

    static Gateway &get();

    Gateway();
    ~Gateway();

    Gateway(const Gateway &) = delete;
    Gateway &operator=(const Gateway &) = delete;

    bool connect(const Options &opt = Options{});
    void disconnect();
    void reconnect(bool resume = true);

    void send(const std::string &text);
    void send(const Json &j);

    void setIdentityProvider(IdentityProvider factory);

    Subscription subscribe(AnyHandler callback, bool ui = true);
    Subscription subscribe(const std::string &t, EventHandler callback, bool ui = true);
    Subscription subscribeConnectionState(ConnectionStateHandler callback, bool ui = true);
    void unsubscribe(SubId id);

    bool isConnected() const { return m_connected.load(); }
    ConnectionState getConnectionState() const {
        return m_connected.load() ? ConnectionState::Connected : ConnectionState::Disconnected;
    }

    void setAppState(std::shared_ptr<AppState> state);
    void setDataStore(std::shared_ptr<DataStore> store);

    const std::optional<ReadyState> &getReadyState() const { return m_readyState; }
    const std::vector<GuildSummary> &getGuilds() const { return m_guilds; }

    void setAuthenticated(bool authenticated) { m_authenticated.store(authenticated); }
    bool isAuthenticated() const { return m_authenticated.load(); }

  private:
    void receive(const std::string &text);
    void dispatch(std::function<void()> fn);
    void notifyConnectionState(ConnectionState state);

    void handleReady(const Json &data);
    void handleReadySupplemental(const Json &data);
    void handleMessageCreate(const Json &data);

    void handleHello(const Json &data);
    void handleReconnect(const Json &data);
    void handleInvalidSession(const Json &data);
    void startHeartbeat(int intervalMs);
    void sendHeartbeat();
    void stopHeartbeat();

  private:
    struct AnySub {
        AnyHandler callback;
        bool ui = true;
    };

    struct EventSub {
        EventHandler callback;
        bool ui = true;
    };

    struct ConnectionStateSub {
        ConnectionStateHandler callback;
        bool ui = true;
    };

    static Gateway *s_instance;

    Options m_options{};

    lws_context *m_lwsContext = nullptr;
    lws *m_wsi = nullptr;
    std::thread m_serviceThread;
    std::mutex m_sendMutex;
    std::queue<std::string> m_sendQueue;
    std::string m_receiveBuffer;

    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_shuttingDown{false};
    std::atomic<bool> m_authenticated{false};

    std::mutex m_identityMutex;
    IdentityProvider m_identityProvider;

    std::mutex m_subscriptionMutex;
    std::atomic<SubId> m_nextId{0};

    std::unordered_map<SubId, AnySub> m_subscriptions;
    std::unordered_map<std::string, std::unordered_map<SubId, EventSub>> m_eventSubscriptions;
    std::unordered_map<SubId, ConnectionStateSub> m_connectionStateSubscriptions;

    std::shared_ptr<AppState> m_appState;
    std::shared_ptr<DataStore> m_dataStore;
    std::optional<ReadyState> m_readyState;
    std::vector<GuildSummary> m_guilds;

    std::atomic<int> m_heartbeatInterval{0};
    std::atomic<bool> m_heartbeatRunning{false};
    std::atomic<int> m_lastSequence{-1};
    std::atomic<bool> m_heartbeatAcked{true};

    std::string m_sessionId;
    std::string m_resumeGatewayUrl;
    std::mutex m_sessionMutex;
};
