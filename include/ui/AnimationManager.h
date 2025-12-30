#pragma once

#include <FL/Fl.H>

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>

/**
 * Global animation manager that provides unified timing for all animations.
 * Instead of each component running its own timer, components register with
 * this manager to receive animation callbacks from a single shared timer.
 *
 * Benefits:
 * - Reduced CPU usage (one timer instead of many)
 * - Synchronized animations across the app
 * - Centralized control (pause/resume all animations)
 * - Automatic timer management (starts when first animation registers, stops when last unregisters)
 */
class AnimationManager {
  public:
    using AnimationId = uint64_t;
    using AnimationCallback = std::function<bool()>; // Returns true to continue, false to unregister

    static AnimationManager &get();

    /**
     * Register an animation callback.
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
     * Set the target frame rate (FPS) for all animations.
     * Default is 30 FPS. Common values: 30, 60.
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
    std::atomic<int> m_fps{60}; // Default 60 FPS
    std::atomic<bool> m_timerRunning{false};
    std::atomic<bool> m_paused{false};
};
