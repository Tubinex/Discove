#pragma once

#include <FL/Fl.H>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

class Gateway {
  public:
    using Json = nlohmann::json;
    using SubId = uint64_t;

    using AnyHandler = std::function<void(const Json &)>;
    using EventHandler = std::function<void(const Json &)>;
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

    Gateway();
    ~Gateway();

    Gateway(const Gateway &) = delete;
    Gateway &operator=(const Gateway &) = delete;

    bool connect(const Options &opt = Options{});
    void disconnect();

    void send(const std::string &text);
    void send(const Json &j);

    void setIdentityProvider(IdentityProvider factory);

    Subscription subscribe(AnyHandler callback, bool ui = true);
    Subscription subscribe(const std::string &t, EventHandler callback, bool ui = true);
    void unsubscribe(SubId id);

    bool isConnected() const { return m_connected.load(); }

  private:
    void receive(const std::string &text);
    void dispatch(std::function<void()> fn);

  private:
    struct AnySub {
        AnyHandler callback;
        bool ui = true;
    };

    struct EventSub {
        EventHandler callback;
        bool ui = true;
    };

    Options m_options{};
    ix::WebSocket m_ws;

    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_shuttingDown{false};

    std::mutex m_identityMutex;
    IdentityProvider m_identityProvider;

    std::mutex m_subscriptionMutex;
    std::atomic<SubId> m_nextId{0};

    std::unordered_map<SubId, AnySub> m_subscriptions;
    std::unordered_map<std::string, std::unordered_map<SubId, EventSub>> m_eventSubscriptions;
};
