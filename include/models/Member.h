#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class User;

/**
 * @brief Represents a guild member
 * @see https://discord.com/developers/docs/resources/guild#guild-member-object
 */
class Member {
  public:
    /**
     * @brief Deserialize member from JSON
     * @param j JSON object
     * @param guildId Guild ID
     * @return Member instance
     */
    static Member fromJson(const nlohmann::json &j, const std::string &guildId);

    /**
     * @brief Get display name
     * @param user The User object for this member
     * @return Display name
     */
    std::string getDisplayName(const User &user) const;

    /**
     * @brief Get avatar URL
     * @param user The User object for this member
     * @param size Desired size (default 256)
     * @return CDN URL for avatar
     */
    std::string getAvatarUrl(const User &user, int size = 256) const;

    /**
     * @brief Check if member is boosting the guild
     * @return true if member is a booster
     */
    bool isBoosting() const;

    /**
     * @brief Check if member is currently timed out
     * @return true if member is timed out
     */
    bool isTimedOut() const;

    std::string userId;
    std::string guildId;
    std::optional<std::string> nick;
    std::optional<std::string> avatar;
    std::vector<std::string> roleIds;
    std::chrono::system_clock::time_point joinedAt;
    std::optional<std::chrono::system_clock::time_point> premiumSince;
    bool deaf = false;
    bool mute = false;
    std::optional<int> flags;
    bool pending = false;
    std::optional<std::chrono::system_clock::time_point> communicationDisabledUntil;
};
