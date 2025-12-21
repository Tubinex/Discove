#include "router/Router.h"
#include "router/Screen.h"
#include "state/AppState.h"
#include "utils/Logger.h"

Router::Router(int x, int y, int w, int h) : Fl_Group(x, y, w, h), m_routeSubscription(0), m_initialized(false) {
    Logger::debug("Router created");
}

Router::~Router() {
    if (m_routeSubscription) {
        Store::get().unsubscribe(m_routeSubscription);
    }

    if (m_currentScreen) {
        m_currentScreen->onExit();
        m_currentScreen->onDestroy();
        remove(m_currentScreen);
        delete m_currentScreen;
        m_currentScreen = nullptr;
    }
}

Router &Router::get() {
    static Router instance(0, 0, 800, 600);
    return instance;
}

void Router::addRoute(const std::string &path, Route::ScreenFactory factory) {
    m_routes.emplace_back(path, std::move(factory));
    Logger::debug("Route added: " + path);
}

void Router::setNotFoundFactory(Route::ScreenFactory factory) { m_notFoundFactory = std::move(factory); }

void Router::setTransitionDuration(double seconds) { m_transitionDuration = seconds; }

void Router::start(const std::string &initialPath) {
    if (m_initialized) {
        Logger::warn("Router already started");
        return;
    }

    begin();
    end();

    m_routeSubscription = Store::get().subscribe<RouteState>(
        [](const AppState &state) { return state.route; },
        [this](const RouteState &newRoute) { handleRouteChange(newRoute); },
        std::equal_to<RouteState>{}, false);

    m_initialized = true;
    navigate(initialPath);
}

void Router::navigate(const std::string &path) {
    updateRouteState([&](RouteState &r) {
        std::string cleanPath;
        parseQuery(path, cleanPath, r.query);

        if (r.historyIndex < static_cast<int>(r.history.size()) - 1) {
            r.history.erase(r.history.begin() + r.historyIndex + 1, r.history.end());
        }

        r.history.push_back(cleanPath);
        r.historyIndex = static_cast<int>(r.history.size()) - 1;
        r.currentPath = cleanPath;
    });
}

void Router::push(const std::string &path) {
    navigate(path);
}

void Router::replace(const std::string &path) {
    updateRouteState([&](RouteState &r) {
        std::string cleanPath;
        parseQuery(path, cleanPath, r.query);

        if (!r.history.empty()) {
            r.history[r.historyIndex] = cleanPath;
        } else {
            r.history.push_back(cleanPath);
            r.historyIndex = 0;
        }
        r.currentPath = cleanPath;
    });
}

void Router::back() {
    updateRouteState([](RouteState &r) {
        if (r.historyIndex > 0) {
            r.historyIndex--;
            r.currentPath = r.history[r.historyIndex];
        }
    });
}

void Router::forward() {
    updateRouteState([](RouteState &r) {
        if (r.historyIndex < static_cast<int>(r.history.size()) - 1) {
            r.historyIndex++;
            r.currentPath = r.history[r.historyIndex];
        }
    });
}

void Router::go(int delta) {
    updateRouteState([delta](RouteState &r) {
        int newIndex = r.historyIndex + delta;
        if (newIndex >= 0 && newIndex < static_cast<int>(r.history.size())) {
            r.historyIndex = newIndex;
            r.currentPath = r.history[r.historyIndex];
        }
    });
}

void Router::handleRouteChange(const RouteState &newRoute) {
    if (m_transition && m_transition->isRunning()) {
        return;
    }

    performNavigation(newRoute.currentPath);
}

void Router::performNavigation(const std::string &toPath) {
    RouteMatch match;
    Screen *newScreen = createScreenForPath(toPath, match);

    if (!newScreen) {
        Logger::error("Failed to create screen for path: " + toPath);
        return;
    }

    Logger::info("Navigating to: " + toPath);

    Screen::Context ctx;
    ctx.path = toPath;
    ctx.params = std::move(match.params);

    AppState state = Store::get().snapshot();
    ctx.query = state.route.query;

    m_nextScreen = newScreen;
    m_nextScreen->setContext(ctx);
    m_nextScreen->onCreate(ctx);
    m_nextScreen->onEnter(ctx);

    if (m_currentScreen) {
        if (m_transitionDuration > 0.0) {
            add(m_nextScreen);
            m_transition = std::make_unique<Transition>(m_currentScreen, m_nextScreen, m_transitionDuration);
            m_transition->start([this]() { finishTransition(); });
        } else {
            m_currentScreen->onExit();
            m_currentScreen->onDestroy();
            remove(m_currentScreen);
            delete m_currentScreen;
            m_currentScreen = nullptr;

            add(m_nextScreen);
            m_currentScreen = m_nextScreen;
            m_nextScreen = nullptr;

            damage(FL_DAMAGE_ALL);
            if (parent()) {
                parent()->damage(FL_DAMAGE_ALL);
                parent()->redraw();
            }
            redraw();
        }
    } else {
        add(m_nextScreen);
        m_currentScreen = m_nextScreen;
        m_nextScreen = nullptr;

        damage(FL_DAMAGE_ALL);
        if (parent()) {
            parent()->damage(FL_DAMAGE_ALL);
            parent()->redraw();
        }
        redraw();
    }
}

void Router::finishTransition() {
    if (m_currentScreen) {
        m_currentScreen->onExit();
        m_currentScreen->onDestroy();
        remove(m_currentScreen);
        delete m_currentScreen;
    }

    m_currentScreen = m_nextScreen;
    m_nextScreen = nullptr;
    m_transition.reset();

    redraw();
}

const Route *Router::findRoute(const std::string &path) const {
    for (const auto &route : m_routes) {
        if (route.match(path)) {
            return &route;
        }
    }
    return nullptr;
}

Screen *Router::createScreenForPath(const std::string &path, RouteMatch &outMatch) {
    const Route *route = findRoute(path);

    if (route) {
        outMatch = route->match(path);
        return route->createScreen(x(), y(), w(), h()).release();
    }

    if (m_notFoundFactory) {
        Logger::warn("No route found for: " + path + ", using 404 handler");
        outMatch.matched = false;
        return m_notFoundFactory(x(), y(), w(), h()).release();
    }

    Logger::error("No route found for: " + path + " and no 404 handler configured");
    return nullptr;
}

void Router::updateRouteState(const std::function<void(RouteState &)> &mutator) {
    Store::get().update([&mutator](AppState &state) { mutator(state.route); });
}

void Router::parseQuery(const std::string &path, std::string &outPath,
                        std::unordered_map<std::string, std::string> &outQuery) {
    outQuery.clear();

    size_t queryPos = path.find('?');
    if (queryPos == std::string::npos) {
        outPath = path;
        return;
    }

    outPath = path.substr(0, queryPos);
    std::string queryString = path.substr(queryPos + 1);

    size_t start = 0;
    while (start < queryString.length()) {
        size_t ampPos = queryString.find('&', start);
        if (ampPos == std::string::npos) {
            ampPos = queryString.length();
        }

        std::string param = queryString.substr(start, ampPos - start);
        size_t eqPos = param.find('=');

        if (eqPos != std::string::npos) {
            std::string key = param.substr(0, eqPos);
            std::string value = param.substr(eqPos + 1);
            outQuery[key] = value;
        }

        start = ampPos + 1;
    }
}
