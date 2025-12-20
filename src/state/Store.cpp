#include "state/Store.h"

Store &Store::get() {
    static Store instance;
    return instance;
}

AppState Store::snapshot() const {
    std::scoped_lock lock(m_mutex);
    return m_state;
}

void Store::update(const std::function<void(AppState &)> &mutator) {
    {
        std::scoped_lock lock(m_mutex);
        mutator(m_state);
    }
    notifyAsync();
}

Store::ListenerId Store::subscribe(Listener cb) {
    std::scoped_lock lock(m_mutex);
    const auto id = ++m_nextId;
    m_listeners.emplace(id, std::move(cb));
    return id;
}

void Store::unsubscribe(ListenerId id) {
    std::scoped_lock lock(m_mutex);
    m_listeners.erase(id);
}

void Store::notifyAsync() {
    Fl::awake([](void *self) { static_cast<Store *>(self)->notifyNow(); }, this);
}

void Store::notifyNow() {
    AppState stateCopy;
    std::unordered_map<ListenerId, Listener> listenersCopy;
    {
        std::scoped_lock lock(m_mutex);
        stateCopy = m_state;
        listenersCopy = m_listeners;
    }
    for (auto &[id, cb] : listenersCopy) {
        cb(stateCopy);
    }
}
