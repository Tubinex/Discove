#include "net/Gateway.h"

#include <memory>
#include <utility>

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
