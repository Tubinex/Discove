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
#include "utils/Uuid.h"

const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 720;

int main(int argc, char **argv) {

    Fl::lock();

    Logger::setLevel(Logger::Level::DEBUG);
    Logger::info("Application started");

    init_theme();

    Fl_Window *window = new Fl_Window(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Discove");

    Router &router = Router::get();
    router.resize(0, 0, window->w(), window->h());

    window->begin();
    window->add(&router);

    router.addRoute("/", [](int x, int y, int w, int h) { return std::make_unique<HomeScreen>(x, y, w, h); });
    router.addRoute("/loading", [](int x, int y, int w, int h) { return std::make_unique<LoadingScreen>(x, y, w, h); });
    router.addRoute("/login", [](int x, int y, int w, int h) { return std::make_unique<LoginScreen>(x, y, w, h); });
    router.addRoute("/settings",
                    [](int x, int y, int w, int h) { return std::make_unique<SettingsScreen>(x, y, w, h); });
    router.addRoute("/ui-library",
                    [](int x, int y, int w, int h) { return std::make_unique<UILibraryScreen>(x, y, w, h); });
    router.addRoute("/user/:id",
                    [](int x, int y, int w, int h) { return std::make_unique<ProfileScreen>(x, y, w, h); });
    router.setNotFoundFactory([](int x, int y, int w, int h) { return std::make_unique<NotFoundScreen>(x, y, w, h); });

    Gateway &gateway = Gateway::get();
    [[maybe_unused]] static auto gatewayLogSub = gateway.subscribe(
        [](const Gateway::Json &message) { Logger::debug("Gateway message: " + message.dump(2)); }, false);

    [[maybe_unused]] static auto gatewayStateSub =
        gateway.subscribeConnectionState([&router, &gateway](Gateway::ConnectionState state) {
            if (state == Gateway::ConnectionState::Connected) {
                Logger::info("Gateway connected!");
            } else {
                Logger::warn("Gateway disconnected!");
                if (!gateway.isAuthenticated()) {
                    Logger::error("Disconnected before authentication - token is invalid");
                    Secrets::set("token", "");
                    router.navigate("/login");
                }
            }
        });

    [[maybe_unused]] static auto readySub = gateway.subscribe(
        "READY",
        [&router, &gateway](const Gateway::Json &message) {
            Logger::info("Authentication successful! Received READY event");
            gateway.setAuthenticated(true);
            router.navigate("/");
        },
        true);

    [[maybe_unused]] static auto readySupplementalSub = gateway.subscribe(
        "READY_SUPPLEMENTAL", [](const Gateway::Json &message) { Logger::info("Received READY_SUPPLEMENTAL event"); },
        true);

    [[maybe_unused]] static auto anySub = gateway.subscribe(
        [&router](const Gateway::Json &message) {
            if (message.contains("op") && message["op"].is_number() && message["op"].get<int>() == 9) {
                Logger::error("Authentication failed: Invalid session");
                Secrets::set("token", "");
                router.navigate("/login");
            }
        },
        true);

    auto token = Secrets::get("token");
    if (token.has_value() && !token.value().empty()) {
        Logger::info("Found existing token, attempting to authenticate...");

        gateway.setAuthenticated(false);
        gateway.setIdentityProvider([token = token.value()]() -> Gateway::Json {
            const std::string launchId = UuidHelper::generate();
            const std::string signature = UuidHelper::generate();

            return {{"op", 2},
                    {"d",
                     {{"token", token},
                      {"capabilities", 1734653},
                      {"properties",
                       {{"os", "Windows"},
                        {"browser", "Discord Client"},
                        {"device", ""},
                        {"system_locale", "en-US"},
                        {"has_client_mods", false},
                        {"browser_user_agent", "Discord/1.0"},
                        {"browser_version", "1.0.0"},
                        {"os_version", "10"},
                        {"referrer", ""},
                        {"referring_domain", ""},
                        {"referrer_current", ""},
                        {"referring_domain_current", ""},
                        {"release_channel", "stable"},
                        {"client_build_number", 476179},
                        {"client_event_source", nullptr},
                        {"client_launch_id", launchId},
                        {"launch_signature", signature},
                        {"client_app_state", "focused"},
                        {"is_fast_connect", false},
                        {"gateway_connect_reasons", "AppSkeleton"}}},
                      {"presence",
                       {{"status", "online"}, {"since", 0}, {"activities", nlohmann::json::array()}, {"afk", false}}},
                      {"compress", false},
                      {"client_state", {{"guild_versions", nlohmann::json::object()}}}}}};
        });

        router.start("/loading");
        if (!gateway.connect()) {
            Logger::error("Gateway connection failed");
            router.navigate("/login");
        }
    } else {
        Logger::info("No token found, showing login screen");
        router.start("/login");
    }

    window->end();
    window->resizable(&router);
    window->size_range(400, 300);
    window->show(argc, argv);

    return Fl::run();
}
