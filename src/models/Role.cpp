#include "models/Role.h"

#include "utils/CDN.h"
#include "utils/Permissions.h"

Role Role::fromJson(const nlohmann::json &j) {
    Role role;

    role.id = j.at("id").get<std::string>();
    role.name = j.at("name").get<std::string>();
    role.color = j.at("color").get<uint32_t>();
    role.hoist = j.at("hoist").get<bool>();
    role.position = j.at("position").get<int>();
    role.permissions = j.at("permissions").get<std::string>();
    role.managed = j.at("managed").get<bool>();
    role.mentionable = j.at("mentionable").get<bool>();

    if (j.contains("icon") && !j["icon"].is_null()) {
        role.icon = j["icon"].get<std::string>();
    }

    if (j.contains("unicode_emoji") && !j["unicode_emoji"].is_null()) {
        role.unicodeEmoji = j["unicode_emoji"].get<std::string>();
    }

    if (j.contains("flags") && !j["flags"].is_null()) {
        role.flags = j["flags"].get<uint32_t>();
    }

    return role;
}

std::string Role::getIconUrl(int size) const {
    if (icon.has_value()) {
        return CDNUtils::getRoleIconUrl(id, *icon, size);
    }
    return "";
}

bool Role::hasPermission(uint64_t permission) const { return PermissionUtils::hasPermission(permissions, permission); }

uint64_t Role::getPermissionsInt() const { return PermissionUtils::parsePermissions(permissions); }
