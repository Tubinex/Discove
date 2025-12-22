#include "screens/LoginScreen.h"
#include "net/Gateway.h"
#include "router/Router.h"
#include "ui/Theme.h"
#include "utils/Logger.h"
#include "utils/Secrets.h"
#include "utils/Uuid.h"
#include <FL/fl_ask.H>

LoginScreen::LoginScreen(int x, int y, int w, int h) : Screen(x, y, w, h, "Login") {
    box(FL_FLAT_BOX);
    color(ThemeColors::BG_PRIMARY);
    setupUI();
}

void LoginScreen::setupUI() {
    begin();

    m_titleLabel = new Fl_Box(w() / 2 - 150, 100, 300, 50, "Discord Login");
    m_titleLabel->labelsize(28);
    m_titleLabel->labelfont(FL_BOLD);
    m_titleLabel->labelcolor(ThemeColors::TEXT_NORMAL);

    m_tokenLabel = new Fl_Box(w() / 2 - 200, 240, 400, 20, "Discord Token:");
    m_tokenLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_tokenLabel->labelcolor(ThemeColors::TEXT_NORMAL);

    m_tokenInput = new Fl_Input(w() / 2 - 200, 260, 400, 35);
    m_tokenInput->type(FL_SECRET_INPUT);
    m_tokenInput->color(ThemeColors::BG_SECONDARY);
    m_tokenInput->textcolor(ThemeColors::TEXT_NORMAL);
    m_tokenInput->cursor_color(ThemeColors::TEXT_NORMAL);
    m_tokenInput->selection_color(ThemeColors::BTN_PRIMARY);

    m_loginBtn = new Fl_Button(w() / 2 - 100, 320, 200, 40, "Login");
    m_loginBtn->color(ThemeColors::BTN_PRIMARY);
    m_loginBtn->labelcolor(FL_WHITE);
    m_loginBtn->callback(
        [](Fl_Widget *, void *data) {
            auto *screen = static_cast<LoginScreen *>(data);
            screen->onLoginClicked();
        },
        this);

    m_errorLabel = new Fl_Box(w() / 2 - 200, 370, 400, 30, "");
    m_errorLabel->align(FL_ALIGN_CENTER | FL_ALIGN_INSIDE);
    m_errorLabel->labelsize(12);
    m_errorLabel->labelcolor(FL_RED);
    m_errorLabel->hide();

    end();
}

void LoginScreen::onLoginClicked() {
    const char *token = m_tokenInput->value();

    if (!token || strlen(token) == 0) {
        m_errorLabel->copy_label("Please enter your Discord token");
        m_errorLabel->show();
        redraw();
        return;
    }

    if (Secrets::set("token", token)) {
        Logger::info("Token saved successfully, authenticating...");

        Gateway &gateway = Gateway::get();
        gateway.setAuthenticated(false);
        gateway.setIdentityProvider([token = std::string(token)]() -> Gateway::Json {
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

        Router::get().navigate("/loading");
        if (gateway.isConnected()) {
            gateway.disconnect();
        }

        if (!gateway.connect()) {
            Logger::error("Gateway connection failed");
            m_errorLabel->copy_label("Failed to connect to Gateway. Please try again.");
            m_errorLabel->show();
            redraw();
        }
    } else {
        Logger::error("Failed to save token");
        m_errorLabel->copy_label("Failed to save token. Please try again.");
        m_errorLabel->show();
        redraw();
    }
}

void LoginScreen::onCreate(const Context &ctx) {}

void LoginScreen::onEnter(const Context &ctx) {}

void LoginScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void LoginScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
