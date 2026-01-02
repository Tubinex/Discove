#include "models/PermissionOverwrite.h"
#include "utils/Permissions.h"

PermissionOverwrite PermissionOverwrite::fromJson(const nlohmann::json &j) {
    PermissionOverwrite overwrite;

    if (j.contains("id") && !j["id"].is_null()) {
        overwrite.id = j["id"].get<std::string>();
    }

    if (j.contains("type") && !j["type"].is_null()) {
        overwrite.type = j["type"].get<int>();
    }

    if (j.contains("allow") && !j["allow"].is_null()) {
        overwrite.allow = PermissionUtils::parsePermissions(j["allow"].get<std::string>());
    } else {
        overwrite.allow = 0;
    }

    if (j.contains("deny") && !j["deny"].is_null()) {
        overwrite.deny = PermissionUtils::parsePermissions(j["deny"].get<std::string>());
    } else {
        overwrite.deny = 0;
    }

    return overwrite;
}

std::vector<PermissionOverwrite> PermissionOverwrite::fromJsonArray(const nlohmann::json &j) {
    std::vector<PermissionOverwrite> overwrites;

    if (!j.is_array()) {
        return overwrites;
    }

    for (const auto &item : j) {
        overwrites.push_back(fromJson(item));
    }

    return overwrites;
}
