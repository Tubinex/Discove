#include "ui/AnimationManager.h"

#include "utils/Logger.h"

#include <algorithm>

AnimationManager &AnimationManager::get() {
    static AnimationManager instance;
    return instance;
}

AnimationManager::AnimationManager() { m_fps.store(60); }

AnimationManager::AnimationId AnimationManager::registerAnimation(AnimationCallback callback) {
    std::scoped_lock lock(m_mutex);

    const auto id = ++m_nextId;
    m_animations.emplace(id, std::move(callback));

    Logger::debug("AnimationManager: Registered animation #" + std::to_string(id) + " (total: " +
                  std::to_string(m_animations.size()) + ")");

    if (m_animations.size() == 1 && !m_timerRunning.load()) {
        Logger::info("AnimationManager: Starting timer at " + std::to_string(m_fps.load()) + " FPS");
        startTimer();
    }

    return id;
}

void AnimationManager::unregisterAnimation(AnimationId id) {
    std::scoped_lock lock(m_mutex);

    auto it = m_animations.find(id);
    if (it != m_animations.end()) {
        m_animations.erase(it);
        Logger::debug("AnimationManager: Unregistered animation #" + std::to_string(id) + " (remaining: " +
                      std::to_string(m_animations.size()) + ")");
    }

    if (m_animations.empty() && m_timerRunning.load()) {
        Logger::info("AnimationManager: Stopping timer (no animations remaining)");
        stopTimer();
    }
}

void AnimationManager::setFrameRate(int fps) {
    if (fps <= 0)
        return;

    m_fps.store(fps);
    if (m_timerRunning.load()) {
    }
}

void AnimationManager::pauseAll() { m_paused.store(true); }

void AnimationManager::resumeAll() { m_paused.store(false); }

void AnimationManager::startTimer() {
    if (m_timerRunning.exchange(true)) {
        return;
    }

    Fl::add_timeout(getFrameTime(), timerCallback, this);
}

void AnimationManager::stopTimer() {
    if (!m_timerRunning.exchange(false)) {
        return;
    }

    Fl::remove_timeout(timerCallback, this);
}

void AnimationManager::tick() {
    if (m_paused.load()) {
        Fl::repeat_timeout(getFrameTime(), timerCallback, this);
        return;
    }

    std::unordered_map<AnimationId, AnimationCallback> animationsCopy;
    {
        std::scoped_lock lock(m_mutex);
        animationsCopy = m_animations;
    }

    std::vector<AnimationId> toRemove;
    for (auto &[id, callback] : animationsCopy) {
        try {
            bool shouldContinue = callback();
            if (!shouldContinue) {
                toRemove.push_back(id);
            }
        } catch (...) {
            toRemove.push_back(id);
        }
    }

    if (!toRemove.empty()) {
        std::scoped_lock lock(m_mutex);
        for (auto id : toRemove) {
            m_animations.erase(id);
            Logger::debug("AnimationManager: Auto-removed animation #" + std::to_string(id) +
                          " (returned false)");
        }

        Logger::debug("AnimationManager: Remaining animations: " + std::to_string(m_animations.size()));

        if (m_animations.empty() && m_timerRunning.load()) {
            Logger::info("AnimationManager: Stopping timer (no animations remaining after cleanup)");
            stopTimer();
            return;
        }
    }

    Fl::repeat_timeout(getFrameTime(), timerCallback, this);
}

void AnimationManager::timerCallback(void *data) {
    auto *manager = static_cast<AnimationManager *>(data);
    manager->tick();
}
