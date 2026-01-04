#include "models/Guild.h"

#include "utils/CDN.h"

#include <algorithm>

Guild Guild::fromJson(const nlohmann::json &j) {
    Guild guild;

    guild.id = j.at("id").get<std::string>();
    guild.name = j.at("name").get<std::string>();

    if (j.contains("properties") && j["properties"].is_object()) {
        const auto &props = j["properties"];

        if (props.contains("name")) {
            guild.name = props["name"].get<std::string>();
        }
        if (props.contains("icon") && !props["icon"].is_null()) {
            guild.icon = props["icon"].get<std::string>();
        }
        if (props.contains("banner") && !props["banner"].is_null()) {
            guild.banner = props["banner"].get<std::string>();
        }
    }

    if (j.contains("icon") && !j["icon"].is_null()) {
        guild.icon = j["icon"].get<std::string>();
    }

    if (j.contains("splash") && !j["splash"].is_null()) {
        guild.splash = j["splash"].get<std::string>();
    }

    if (j.contains("discovery_splash") && !j["discovery_splash"].is_null()) {
        guild.discoverySplash = j["discovery_splash"].get<std::string>();
    }

    if (j.contains("banner") && !j["banner"].is_null()) {
        guild.banner = j["banner"].get<std::string>();
    }

    if (j.contains("owner_id")) {
        guild.ownerId = j["owner_id"].get<std::string>();
    }

    if (j.contains("description") && !j["description"].is_null()) {
        guild.description = j["description"].get<std::string>();
    }

    if (j.contains("rules_channel_id") && !j["rules_channel_id"].is_null()) {
        guild.rulesChannelId = j["rules_channel_id"].get<std::string>();
    }

    if (j.contains("features") && j["features"].is_array()) {
        for (const auto &feature : j["features"]) {
            guild.features.push_back(feature.get<std::string>());
        }
    }

    if (j.contains("verification_level")) {
        guild.verificationLevel = j["verification_level"].get<int>();
    }

    if (j.contains("default_message_notifications")) {
        guild.defaultMessageNotifications = j["default_message_notifications"].get<int>();
    }

    if (j.contains("explicit_content_filter")) {
        guild.explicitContentFilter = j["explicit_content_filter"].get<int>();
    }

    if (j.contains("mfa_level")) {
        guild.mfaLevel = j["mfa_level"].get<int>();
    }

    if (j.contains("premium_tier")) {
        guild.premiumTier = j["premium_tier"].get<int>();
    }

    if (j.contains("premium_subscription_count")) {
        guild.premiumSubscriptionCount = j["premium_subscription_count"].get<int>();
    }

    if (j.contains("preferred_locale") && !j["preferred_locale"].is_null()) {
        guild.preferredLocale = j["preferred_locale"].get<std::string>();
    }

    if (j.contains("unavailable") && !j["unavailable"].is_null()) {
        guild.unavailable = j["unavailable"].get<bool>();
    }

    if (j.contains("member_count")) {
        guild.memberCount = j["member_count"].get<int>();
    }

    if (j.contains("large") && !j["large"].is_null()) {
        guild.large = j["large"].get<bool>();
    }

    return guild;
}

std::string Guild::getIconUrl(int size) const {
    if (icon.has_value()) {
        return CDNUtils::getGuildIconUrl(id, *icon, size);
    }
    return "";
}

std::string Guild::getSplashUrl(int size) const {
    if (splash.has_value()) {
        return CDNUtils::getGuildSplashUrl(id, *splash, size);
    }
    return "";
}

std::string Guild::getBannerUrl(int size) const {
    if (banner.has_value()) {
        return CDNUtils::getGuildBannerUrl(id, *banner, size);
    }
    return "";
}

bool Guild::hasFeature(const std::string &feature) const {
    return std::find(features.begin(), features.end(), feature) != features.end();
}
