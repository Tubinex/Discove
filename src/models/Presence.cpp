#include "models/Presence.h"

#include "utils/Time.h"

#include <map>

Activity Activity::fromJson(const nlohmann::json &j) {
    Activity activity;

    activity.name = j.at("name").get<std::string>();
    activity.type = static_cast<ActivityType>(j.at("type").get<int>());

    if (j.contains("url") && !j["url"].is_null()) {
        activity.url = j["url"].get<std::string>();
    }

    if (j.contains("state") && !j["state"].is_null()) {
        activity.state = j["state"].get<std::string>();
    }

    if (j.contains("details") && !j["details"].is_null()) {
        activity.details = j["details"].get<std::string>();
    }

    if (j.contains("created_at") && !j["created_at"].is_null()) {
        int64_t timestamp = j["created_at"].get<int64_t>();
        auto duration = std::chrono::milliseconds(timestamp);
        activity.createdAt = std::chrono::system_clock::time_point(duration);
    }

    if (j.contains("application_id") && !j["application_id"].is_null()) {
        activity.applicationId = j["application_id"].get<std::string>();
    }

    return activity;
}

Presence Presence::fromJson(const nlohmann::json &j) {
    Presence presence;

    if (j.contains("user") && j["user"].is_object()) {
        const auto &user = j["user"];
        if (user.contains("id")) {
            presence.userId = user["id"].get<std::string>();
        }
    }

    if (j.contains("guild_id") && !j["guild_id"].is_null()) {
        presence.guildId = j["guild_id"].get<std::string>();
    }

    if (j.contains("status")) {
        std::string statusStr = j["status"].get<std::string>();
        static const std::map<std::string, Status> statusMap = {{"online", Status::ONLINE},
                                                                {"dnd", Status::DND},
                                                                {"idle", Status::IDLE},
                                                                {"invisible", Status::INVISIBLE},
                                                                {"offline", Status::OFFLINE}};

        auto it = statusMap.find(statusStr);
        if (it != statusMap.end()) {
            presence.status = it->second;
        } else {
            presence.status = Status::OFFLINE;
        }
    } else {
        presence.status = Status::OFFLINE;
    }

    if (j.contains("activities") && j["activities"].is_array()) {
        for (const auto &activityJson : j["activities"]) {
            presence.activities.push_back(Activity::fromJson(activityJson));
        }
    }

    return presence;
}
