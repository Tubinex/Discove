#pragma once

#include <chrono>
#include <functional>

#include "ui/AnimationManager.h"

class Screen;

class Transition {
  public:
    using OnComplete = std::function<void()>;

    enum class Type { Fade, SlideLeft, SlideRight, CrossFade };

    Transition(Screen *from, Screen *to, double duration, Type type = Type::Fade);
    ~Transition();

    void start(OnComplete onComplete);
    void cancel();

    bool isRunning() const { return m_running; }
    float progress() const { return m_progress; }

  private:
    bool update();

    Screen *m_from;
    Screen *m_to;
    double m_duration;
    Type m_type;

    std::chrono::steady_clock::time_point m_startTime;
    float m_progress = 0.0f;
    bool m_running = false;
    OnComplete m_onComplete;
    AnimationManager::AnimationId m_animationId = 0;
};
