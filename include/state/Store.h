#pragma once

#include <FL/Fl.H>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>

#include "state/AppState.h"

class Store {
  public:
    using ListenerId = uint64_t;
    using Listener = std::function<void(const AppState &)>;

    static Store &get();

    AppState snapshot() const;
    void update(const std::function<void(AppState &)> &mutator);

    ListenerId subscribe(Listener cb);
    template <class T, class Selector, class Callback, class Equals = std::equal_to<T>>
    ListenerId subscribe(Selector selector, Callback onChange, Equals equals = Equals{}, bool fireImmediately = false) {
        static_assert(std::is_copy_constructible_v<T>, "T must be copy-constructible.");
        T initial;
        {
            std::scoped_lock lock(m_mutex);
            initial = static_cast<T>(selector(m_state));
        }

        if (fireImmediately) {
            onChange(initial);
        }

        auto last = std::make_shared<T>(std::move(initial));
        return subscribe([selector = std::move(selector), onChange = std::move(onChange), equals = std::move(equals),
                          last](const AppState &s) mutable {
            T next = static_cast<T>(selector(s));
            if (!equals(*last, next)) {
                *last = next;
                onChange(*last);
            }
        });
    }

    void unsubscribe(ListenerId id);

  private:
    Store() = default;
    Store(const Store &) = delete;
    Store &operator=(const Store &) = delete;

    void notifyAsync();
    void notifyNow();

  private:
    mutable std::mutex m_mutex;
    AppState m_state{};

    std::unordered_map<ListenerId, Listener> m_listeners;
    std::atomic<ListenerId> m_nextId{0};
};
