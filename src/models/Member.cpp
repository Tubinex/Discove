#include "models/Member.h"

#include "models/User.h"
#include "utils/CDN.h"
#include "utils/Time.h"

Member Member::fromJson(const nlohmann::json &j, const std::string &guildId) {
    Member member;

    member.guildId = guildId;
    if (j.contains("user") && j["user"].is_object()) {
        const auto &user = j["user"];
        if (user.contains("id")) {
            member.userId = user["id"].get<std::string>();
        }
    }

    if (j.contains("nick") && !j["nick"].is_null()) {
        member.nick = j["nick"].get<std::string>();
    }

    if (j.contains("avatar") && !j["avatar"].is_null()) {
        member.avatar = j["avatar"].get<std::string>();
    }

    if (j.contains("roles") && j["roles"].is_array()) {
        for (const auto &roleId : j["roles"]) {
            member.roleIds.push_back(roleId.get<std::string>());
        }
    }

    if (j.contains("joined_at")) {
        std::string joinedAtStr = j["joined_at"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(joinedAtStr);
        if (parsed.has_value()) {
            member.joinedAt = *parsed;
        } else {
            member.joinedAt = std::chrono::system_clock::now();
        }
    }

    if (j.contains("premium_since") && !j["premium_since"].is_null()) {
        std::string premiumSinceStr = j["premium_since"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(premiumSinceStr);
        if (parsed.has_value()) {
            member.premiumSince = *parsed;
        }
    }

    if (j.contains("deaf") && !j["deaf"].is_null()) {
        member.deaf = j["deaf"].get<bool>();
    }

    if (j.contains("mute") && !j["mute"].is_null()) {
        member.mute = j["mute"].get<bool>();
    }

    if (j.contains("flags") && !j["flags"].is_null()) {
        member.flags = j["flags"].get<int>();
    }

    if (j.contains("pending") && !j["pending"].is_null()) {
        member.pending = j["pending"].get<bool>();
    }

    if (j.contains("communication_disabled_until") && !j["communication_disabled_until"].is_null()) {
        std::string timeoutStr = j["communication_disabled_until"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(timeoutStr);
        if (parsed.has_value()) {
            member.communicationDisabledUntil = *parsed;
        }
    }

    return member;
}

std::string Member::getDisplayName(const User &user) const {
    if (nick.has_value() && !nick->empty()) {
        return *nick;
    }
    return user.getDisplayName();
}

std::string Member::getAvatarUrl(const User &user, int size) const {
    if (avatar.has_value()) {
        return CDNUtils::getMemberAvatarUrl(guildId, userId, *avatar, size);
    }
    return user.getAvatarUrl(size);
}

bool Member::isBoosting() const { return premiumSince.has_value(); }

bool Member::isTimedOut() const {
    if (!communicationDisabledUntil.has_value()) {
        return false;
    }
    return *communicationDisabledUntil > std::chrono::system_clock::now();
}
