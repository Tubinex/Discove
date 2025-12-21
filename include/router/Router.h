#pragma once

#include <FL/Fl_Group.H>
#include <memory>
#include <vector>

#include "router/Route.h"
#include "router/Transition.h"
#include "state/Store.h"

class Screen;

class Router : public Fl_Group {
  public:
    static Router &get();

    void addRoute(const std::string &path, Route::ScreenFactory factory);
    void setNotFoundFactory(Route::ScreenFactory factory);
    void setTransitionDuration(double seconds);

    static void navigate(const std::string &path);
    static void push(const std::string &path);
    static void replace(const std::string &path);
    static void back();
    static void forward();
    static void go(int delta);

    void start(const std::string &initialPath = "/");

    ~Router();

  private:
    Router(int x, int y, int w, int h);
    Router(const Router &) = delete;
    Router &operator=(const Router &) = delete;

    void handleRouteChange(const RouteState &newRoute);
    void performNavigation(const std::string &toPath);
    void finishTransition();

    const Route *findRoute(const std::string &path) const;
    Screen *createScreenForPath(const std::string &path, RouteMatch &outMatch);

    static void updateRouteState(const std::function<void(RouteState &)> &mutator);
    static void parseQuery(const std::string &path, std::string &outPath,
                           std::unordered_map<std::string, std::string> &outQuery);

  private:
    std::vector<Route> m_routes;
    Route::ScreenFactory m_notFoundFactory;

    Screen *m_currentScreen = nullptr;
    Screen *m_nextScreen = nullptr;

    std::unique_ptr<Transition> m_transition;
    double m_transitionDuration = 0.0; // seconds (0 = instant, >0 = animated)

    Store::ListenerId m_routeSubscription;
    bool m_initialized;
};
