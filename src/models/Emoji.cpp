#include "models/Emoji.h"

#include "utils/CDN.h"

Emoji Emoji::fromJson(const nlohmann::json &j) {
    Emoji emoji;

    emoji.id = j.at("id").get<std::string>();
    emoji.name = j.at("name").get<std::string>();

    if (j.contains("roles") && j["roles"].is_array()) {
        for (const auto &roleId : j["roles"]) {
            emoji.roleIds.push_back(roleId.get<std::string>());
        }
    }

    if (j.contains("user") && j["user"].is_object()) {
        const auto &user = j["user"];
        if (user.contains("id")) {
            emoji.userId = user["id"].get<std::string>();
        }
    }

    if (j.contains("require_colons") && !j["require_colons"].is_null()) {
        emoji.requireColons = j["require_colons"].get<bool>();
    }

    if (j.contains("managed") && !j["managed"].is_null()) {
        emoji.managed = j["managed"].get<bool>();
    }

    if (j.contains("animated") && !j["animated"].is_null()) {
        emoji.animated = j["animated"].get<bool>();
    }

    if (j.contains("available") && !j["available"].is_null()) {
        emoji.available = j["available"].get<bool>();
    }

    return emoji;
}

std::string Emoji::getEmojiUrl(int size) const { return CDNUtils::getEmojiUrl(id, animated, size); }

std::string Emoji::getMention() const {
    if (animated) {
        return "<a:" + name + ":" + id + ">";
    } else {
        return "<:" + name + ":" + id + ">";
    }
}
