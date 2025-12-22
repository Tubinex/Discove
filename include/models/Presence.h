#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief User online status
 */
enum class Status {
    ONLINE,
    DND, // Do Not Disturb
    IDLE,
    INVISIBLE,
    OFFLINE
};

/**
 * @brief Activity type
 * @see https://discord.com/developers/docs/topics/gateway-events#activity-object-activity-types
 */
enum class ActivityType { GAME = 0, STREAMING = 1, LISTENING = 2, WATCHING = 3, CUSTOM = 4, COMPETING = 5 };

/**
 * @brief Represents a user activity
 */
struct Activity {
    std::string name;                                               ///< Activity name
    ActivityType type;                                              ///< Activity type
    std::optional<std::string> url;                                 ///< Stream URL (for STREAMING type)
    std::optional<std::string> state;                               ///< Custom status text
    std::optional<std::string> details;                             ///< Activity details
    std::optional<std::chrono::system_clock::time_point> createdAt; ///< When activity was added
    std::optional<std::string> applicationId;                       ///< Application ID

    static Activity fromJson(const nlohmann::json &j);
};

/**
 * @brief Represents a user's presence (online status and activities)
 * @see https://discord.com/developers/docs/topics/gateway-events#presence-update
 */
class Presence {
  public:
    /**
     * @brief Deserialize presence from JSON
     * @param j JSON object from Discord API
     * @return Presence instance
     */
    static Presence fromJson(const nlohmann::json &j);

    std::string userId;                 ///< User ID (snowflake)
    std::optional<std::string> guildId; ///< Guild ID (for guild-specific presence)
    Status status;                      ///< Online status
    std::vector<Activity> activities;   ///< Current activities
};
