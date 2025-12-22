#include "screens/HomeScreen.h"
#include "router/Router.h"
#include "state/AppState.h"
#include "utils/Logger.h"

HomeScreen::HomeScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Home"), m_counterSub(0) { setupUI(); }

HomeScreen::~HomeScreen() {
    if (m_counterSub) {
        Store::get().unsubscribe(m_counterSub);
    }
}

void HomeScreen::setupUI() {
    begin();

    m_counterLabel = new Fl_Box(w() / 2 - 100, 200, 200, 100, "Counter: 0");
    m_counterLabel->labelsize(32);
    m_counterLabel->labelfont(FL_BOLD);

    m_incrementBtn = new Fl_Button(w() / 2 - 75, 320, 150, 40, "Increment");
    m_incrementBtn->callback([](Fl_Widget *, void *) {
        Store::get().update([](AppState &state) { state.counter++; });
    });

    m_settingsBtn = new Fl_Button(w() / 2 - 75, 380, 150, 40, "Settings");
    m_settingsBtn->callback([](Fl_Widget *, void *) { Router::navigate("/settings"); });

    m_profileBtn = new Fl_Button(w() / 2 - 75, 500, 150, 40, "View Profile (User 42)");
    m_profileBtn->callback([](Fl_Widget *, void *) { Router::navigate("/user/42"); });

    m_uiLibraryBtn = new Fl_Button(w() / 2 - 75, 560, 150, 40, "UI Library");
    m_uiLibraryBtn->callback([](Fl_Widget *, void *) { Router::navigate("/ui-library"); });

    end();
}

void HomeScreen::onCreate(const Context &ctx) {
    m_counterSub = Store::get().subscribe<int>(
        [](const AppState &state) { return state.counter; },
        [this](int counter) { onCounterChange(counter); }, std::equal_to<int>{}, true);
}

void HomeScreen::onEnter(const Context &ctx) {}

void HomeScreen::onCounterChange(int newCounter) {
    std::string label = "Counter: " + std::to_string(newCounter);
    m_counterLabel->copy_label(label.c_str());
    m_counterLabel->redraw();
}

void HomeScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void HomeScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
