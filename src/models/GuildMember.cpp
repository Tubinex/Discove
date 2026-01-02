#include "models/GuildMember.h"

GuildMember GuildMember::fromJson(const nlohmann::json &j) {
    GuildMember member;

    if (j.contains("user_id") && !j["user_id"].is_null()) {
        member.userId = j["user_id"].get<std::string>();
    }

    if (j.contains("roles") && j["roles"].is_array()) {
        for (const auto &role : j["roles"]) {
            member.roleIds.push_back(role.get<std::string>());
        }
    }

    if (j.contains("joined_at") && !j["joined_at"].is_null()) {
        member.joinedAt = j["joined_at"].get<std::string>();
    }

    if (j.contains("nick") && !j["nick"].is_null()) {
        member.nick = j["nick"].get<std::string>();
    }

    if (j.contains("deaf") && !j["deaf"].is_null()) {
        member.deaf = j["deaf"].get<bool>();
    }

    if (j.contains("mute") && !j["mute"].is_null()) {
        member.mute = j["mute"].get<bool>();
    }

    if (j.contains("pending") && !j["pending"].is_null()) {
        member.pending = j["pending"].get<bool>();
    }

    return member;
}
