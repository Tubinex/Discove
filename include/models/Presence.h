#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief User online status
 */
enum class Status { ONLINE, DND, IDLE, INVISIBLE, OFFLINE };

/**
 * @brief Activity type
 * @see https://discord.com/developers/docs/topics/gateway-events#activity-object-activity-types
 */
enum class ActivityType { GAME = 0, STREAMING = 1, LISTENING = 2, WATCHING = 3, CUSTOM = 4, COMPETING = 5 };

/**
 * @brief Represents a user activity
 */
struct Activity {
    std::string name;
    ActivityType type;
    std::optional<std::string> url;
    std::optional<std::string> state;
    std::optional<std::string> details;
    std::optional<std::chrono::system_clock::time_point> createdAt;
    std::optional<std::string> applicationId;

    static Activity fromJson(const nlohmann::json &j);
};

/**
 * @brief Represents a user's presence
 * @see https://discord.com/developers/docs/topics/gateway-events#presence-update
 */
class Presence {
  public:
    /**
     * @brief Deserialize presence from JSON
     * @param j JSON object
     * @return Presence instance
     */
    static Presence fromJson(const nlohmann::json &j);

    std::string userId;
    std::optional<std::string> guildId;
    Status status;
    std::vector<Activity> activities;
};
