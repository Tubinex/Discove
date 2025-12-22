#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>

#include <string>

#include "net/Gateway.h"
#include "router/Router.h"
#include "screens/HomeScreen.h"
#include "screens/LoadingScreen.h"
#include "screens/LoginScreen.h"
#include "screens/NotFoundScreen.h"
#include "screens/ProfileScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/UILibraryScreen.h"
#include "state/Store.h"
#include "ui/Theme.h"
#include "utils/Logger.h"
#include "utils/Secrets.h"

const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 720;

int main(int argc, char **argv) {

    Fl::lock();

    Logger::setLevel(Logger::Level::DEBUG);
    Logger::info("Application started");
    Logger::debug("Debugging information");
    Logger::warn("This is a warning");
    Logger::error("This is an error message");

    init_theme();

    auto token = Secrets::get("token");
    if (token.has_value()) {
        Logger::info("Found existing session token: " + token.value());
    } else {
        Logger::info("No existing token found, creating a new one");
        std::string newToken = "example-token-12345";
        if (Secrets::set("token", newToken)) {
            Logger::info("Session token stored successfully");
        }
    }

    Gateway gateway;
    [[maybe_unused]] auto gatewayLogSub = gateway.subscribe(
        [](const Gateway::Json &message) { Logger::debug("Gateway message: " + message.dump(2)); }, false);

    [[maybe_unused]] auto gatewayStateSub = gateway.subscribeConnectionState([](Gateway::ConnectionState state) {
        if (state == Gateway::ConnectionState::Connected) {
            Logger::info("Gateway connected!");
        } else {
            Logger::warn("Gateway disconnected!");
        }
    });

    if (!gateway.connect()) {
        Logger::error("Gateway connection failed");
    }

    Fl_Window *window = new Fl_Window(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Discove");

    Router &router = Router::get();
    router.resize(0, 0, window->w(), window->h());

    window->begin();
    window->add(&router);

    router.addRoute("/", [](int x, int y, int w, int h) { return std::make_unique<HomeScreen>(x, y, w, h); });
    router.addRoute("/loading",
                    [](int x, int y, int w, int h) { return std::make_unique<LoadingScreen>(x, y, w, h); });
    router.addRoute("/login", [](int x, int y, int w, int h) { return std::make_unique<LoginScreen>(x, y, w, h); });
    router.addRoute("/settings",
                    [](int x, int y, int w, int h) { return std::make_unique<SettingsScreen>(x, y, w, h); });
    router.addRoute("/ui-library",
                    [](int x, int y, int w, int h) { return std::make_unique<UILibraryScreen>(x, y, w, h); });
    router.addRoute("/user/:id",
                    [](int x, int y, int w, int h) { return std::make_unique<ProfileScreen>(x, y, w, h); });
    router.setNotFoundFactory([](int x, int y, int w, int h) { return std::make_unique<NotFoundScreen>(x, y, w, h); });
    router.start("/");

    window->end();
    window->resizable(&router);
    window->size_range(400, 300);
    window->show(argc, argv);

    return Fl::run();
}
