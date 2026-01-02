#pragma once

#include <FL/Fl.H>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

class AnimationManager {
  public:
    using AnimationId = uint64_t;
    using AnimationCallback = std::function<bool()>; 

    static AnimationManager &get();

    /**
     * @param callback Function called on each animation frame. Return true to continue, false to stop.
     * @return Animation ID for later unregistration
     */
    AnimationId registerAnimation(AnimationCallback callback);

    /**
     * Unregister an animation callback.
     * @param id The animation ID returned from registerAnimation
     */
    void unregisterAnimation(AnimationId id);

    /**
     * Set the target frame rate for all animations.
     * @param fps Frames per second (must be > 0)
     */
    void setFrameRate(int fps);

    /**
     * Get the current frame rate.
     * @return Current FPS
     */
    int getFrameRate() const { return m_fps.load(); }

    /**
     * Get the frame time in seconds.
     * @return Time between frames (1.0 / fps)
     */
    double getFrameTime() const { return 1.0 / static_cast<double>(m_fps.load()); }

    /**
     * Pause all animations globally.
     */
    void pauseAll();

    /**
     * Resume all animations globally.
     */
    void resumeAll();

    /**
     * Check if animations are paused.
     * @return true if paused
     */
    bool isPaused() const { return m_paused.load(); }

  private:
    AnimationManager();
    AnimationManager(const AnimationManager &) = delete;
    AnimationManager &operator=(const AnimationManager &) = delete;

    void startTimer();
    void stopTimer();
    void tick();
    static void timerCallback(void *data);

  private:
    std::mutex m_mutex;
    std::unordered_map<AnimationId, AnimationCallback> m_animations;
    std::atomic<AnimationId> m_nextId{0};
    std::atomic<int> m_fps{60};
    std::atomic<bool> m_timerRunning{false};
    std::atomic<bool> m_paused{false};
};
