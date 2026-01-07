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
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Image.H>

#ifdef _WIN32
#include <FL/x.H>
#include <windows.h>
#endif

#include <string>

#include "data/Database.h"
#include "net/APIClient.h"
#include "net/Gateway.h"
#include "router/Router.h"
#include "screens/HomeScreen.h"
#include "screens/LoadingScreen.h"
#include "screens/LoginScreen.h"
#include "screens/MainLayoutScreen.h"
#include "screens/NotFoundScreen.h"
#include "screens/ProfileScreen.h"
#include "screens/SettingsScreen.h"
#include "screens/UILibraryScreen.h"
#include "state/Store.h"
#include "ui/EmojiManager.h"
#include "ui/GifAnimation.h"
#include "ui/Theme.h"
#include "utils/Fonts.h"
#include "utils/Logger.h"
#include "utils/Secrets.h"
#include "utils/Uuid.h"

const int INITIAL_WINDOW_WIDTH = 1280;
const int INITIAL_WINDOW_HEIGHT = 720;

namespace {
Fl_Window *g_mainWindow = nullptr;

bool shouldPauseAnimations() {
    if (!g_mainWindow) {
        return false;
    }

#ifdef _WIN32
    HWND hwnd = fl_xid(g_mainWindow);
    if (!hwnd) {
        return false;
    }

    if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) {
        return true;
    }

    HWND fg = GetForegroundWindow();
    if (!fg) {
        return true;
    }

    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    DWORD ourPid = GetCurrentProcessId();
    if (fgPid == ourPid) {
        return false;
    }

    return true;
#else
    return !g_mainWindow->shown();
#endif
}

void syncAnimationPauseState() { GifAnimation::setGlobalPaused(shouldPauseAnimations()); }

void animationPausePoll(void *) {
    syncAnimationPauseState();
    Fl::repeat_timeout(0.25, animationPausePoll);
}

void scheduleAnimationPausePoll() {
    static bool scheduled = false;
    if (scheduled) {
        return;
    }
    scheduled = true;

    Fl::add_timeout(0.25, animationPausePoll);
}
} // namespace

class AppWindow : public Fl_Double_Window {
  public:
    using Fl_Double_Window::Fl_Double_Window;

    int handle(int event) override {
        if (event == FL_ACTIVATE || event == FL_DEACTIVATE || event == FL_SHOW || event == FL_HIDE || event == FL_FOCUS ||
            event == FL_UNFOCUS) {
            syncAnimationPauseState();
        }
        return Fl_Double_Window::handle(event);
    }
};

int main(int argc, char **argv) {

    Fl::lock();

    Fl_Image::RGB_scaling(FL_RGB_SCALING_BILINEAR);
    Fl_Image::scaling_algorithm(FL_RGB_SCALING_BILINEAR);

    Fl::add_handler([](int event) -> int {
        if (event == FL_ACTIVATE || event == FL_DEACTIVATE || event == FL_SHOW || event == FL_HIDE || event == FL_FOCUS ||
            event == FL_UNFOCUS) {
            syncAnimationPauseState();
        }
        return 0;
    });

    Logger::setLevel(Logger::Level::INFO);
    Logger::info("Application started");

    if (!Data::Database::get().initialize()) {
        Logger::error("Failed to initialize database");
        return 1;
    }

    Data::Database::get().clearAllMessages();
    Logger::info("Database initialized and messages cleared for fresh session");

    init_theme();

    if (!FontLoader::loadFonts()) {
        Logger::warn("Some fonts failed to load, using fallback fonts");
    }

    EmojiManager::initializeFromDefaultLocations();

    AppWindow *window = new AppWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "Discove");
    g_mainWindow = window;
    scheduleAnimationPausePoll();

    Router &router = Router::get();
    router.resize(0, 0, window->w(), window->h());

    window->begin();
    window->add(&router);

    auto channelFactory = [](int x, int y, int w, int h) { return std::make_unique<MainLayoutScreen>(x, y, w, h); };

    router.addRoute("/channels/me", channelFactory, "channels");
    router.addRoute("/channels/:dmId", channelFactory, "channels");
    router.addRoute("/channels/:guildId/:channelId", channelFactory, "channels");
    router.addRoute("/", channelFactory, "channels");
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
            Logger::debug("READY event handler called");
            bool wasAuthenticated = gateway.isAuthenticated();
            Logger::debug("wasAuthenticated: " + std::string(wasAuthenticated ? "true" : "false"));
            gateway.setAuthenticated(true);

            if (wasAuthenticated) {
                Logger::info("Session reconnected - received fresh READY event");
            } else {
                Logger::info("Authentication successful! Received READY event");
                Logger::debug("About to navigate to /channels/me");
                router.navigate("/channels/me");
                Logger::debug("Navigation to /channels/me completed");
            }
        },
        true);

    [[maybe_unused]] static auto readySupplementalSub = gateway.subscribe(
        "READY_SUPPLEMENTAL", [](const Gateway::Json &message) { Logger::info("Received READY_SUPPLEMENTAL event"); },
        true);

    [[maybe_unused]] static auto anySub = gateway.subscribe(
        [&router, &gateway](const Gateway::Json &message) {
            if (message.contains("op") && message["op"].is_number() && message["op"].get<int>() == 9) {
                if (!gateway.isAuthenticated()) {
                    Logger::error("Authentication failed: Invalid session");
                    Secrets::set("token", "");
                    router.navigate("/login");
                }
            }
        },
        true);

    auto token = Secrets::get("token");
    if (token.has_value() && !token.value().empty()) {
        Logger::info("Found existing token, attempting to authenticate...");

        Discord::APIClient::get().setToken(token.value());
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
    syncAnimationPauseState();

    return Fl::run();
}
