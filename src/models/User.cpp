#include "models/User.h"

#include "utils/CDN.h"

User User::fromJson(const nlohmann::json &j) {
    User user;

    user.id = j.at("id").get<std::string>();
    user.username = j.at("username").get<std::string>();
    user.discriminator = j.at("discriminator").get<std::string>();

    if (j.contains("global_name") && !j["global_name"].is_null()) {
        user.globalName = j["global_name"].get<std::string>();
    }

    if (j.contains("avatar") && !j["avatar"].is_null()) {
        user.avatar = j["avatar"].get<std::string>();
    }

    if (j.contains("avatar_decoration_data") && !j["avatar_decoration_data"].is_null()) {
        const auto &decorationData = j["avatar_decoration_data"];
        if (decorationData.contains("asset")) {
            user.avatarDecoration = decorationData["asset"].get<std::string>();
        }
    }

    if (j.contains("banner") && !j["banner"].is_null()) {
        user.banner = j["banner"].get<std::string>();
    }

    if (j.contains("accent_color") && !j["accent_color"].is_null()) {
        user.accentColor = j["accent_color"].get<uint32_t>();
    }

    if (j.contains("bot") && !j["bot"].is_null()) {
        user.bot = j["bot"].get<bool>();
    }

    if (j.contains("system") && !j["system"].is_null()) {
        user.system = j["system"].get<bool>();
    }

    if (j.contains("premium_type") && !j["premium_type"].is_null()) {
        user.premiumType = j["premium_type"].get<int>();
    }

    if (j.contains("public_flags") && !j["public_flags"].is_null()) {
        user.publicFlags = j["public_flags"].get<int>();
    }

    return user;
}

std::string User::getDisplayName() const {
    if (globalName.has_value() && !globalName->empty()) {
        return *globalName;
    }
    return username;
}

std::string User::getAvatarUrl(int size) const {
    if (avatar.has_value()) {
        return CDNUtils::getUserAvatarUrl(id, *avatar, size);
    }
    return getDefaultAvatarUrl();
}

std::string User::getBannerUrl(int size) const {
    if (banner.has_value()) {
        return CDNUtils::getUserBannerUrl(id, *banner, size);
    }
    return "";
}

std::string User::getDefaultAvatarUrl() const {
    if (discriminator == "0") {
        return CDNUtils::getDefaultAvatarUrl(id);
    }
    return CDNUtils::getDefaultAvatarUrlLegacy(discriminator);
}
