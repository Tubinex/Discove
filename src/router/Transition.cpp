#include "router/Transition.h"
#include "router/Screen.h"
#include "utils/Logger.h"
#include <FL/Fl.H>
#include <algorithm>

Transition::Transition(Screen *from, Screen *to, double duration, Type type)
    : m_from(from), m_to(to), m_duration(duration), m_type(type) {}

Transition::~Transition() { cancel(); }

void Transition::start(OnComplete onComplete) {
    if (m_running) {
        return;
    }

    m_onComplete = std::move(onComplete);
    m_running = true;
    m_progress = 0.0f;
    m_startTime = std::chrono::steady_clock::now();

    Fl::add_timeout(FRAME_TIME, tick, this);
}

void Transition::cancel() {
    if (m_running) {
        Fl::remove_timeout(tick, this);
        m_running = false;
    }
}

void Transition::tick(void *data) {
    auto *transition = static_cast<Transition *>(data);
    if (transition && transition->m_running) {
        transition->update();
    }
}

void Transition::update() {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_startTime).count();
    m_progress = std::min(1.0f, static_cast<float>(elapsed / m_duration));

    Fl_Widget *parent = m_to ? m_to->parent() : (m_from ? m_from->parent() : nullptr);
    if (parent) {
        parent->damage(FL_DAMAGE_ALL);
    }

    if (m_from) {
        m_from->onTransitionOut(m_progress);
    }
    if (m_to) {
        m_to->onTransitionIn(m_progress);
    }

    if (parent) {
        parent->redraw();
    }

    if (m_progress >= 1.0f) {
        m_running = false;

        if (m_onComplete) {
            m_onComplete();
        }
    } else {
        Fl::add_timeout(FRAME_TIME, tick, this);
    }
}
