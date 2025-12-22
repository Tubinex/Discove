#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

// Forward declaration
class User;

/**
 * @brief Represents a guild member (guild-specific user data)
 * @see https://discord.com/developers/docs/resources/guild#guild-member-object
 */
class Member {
  public:
    /**
     * @brief Deserialize member from JSON
     * @param j JSON object from Discord API
     * @param guildId Guild ID (not always in JSON)
     * @return Member instance
     */
    static Member fromJson(const nlohmann::json &j, const std::string &guildId);

    /**
     * @brief Get display name (nickname or user's display name)
     * @param user The User object for this member
     * @return Display name
     */
    std::string getDisplayName(const User &user) const;

    /**
     * @brief Get avatar URL (guild-specific avatar or user avatar)
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

    std::string userId;                                                ///< User ID (snowflake)
    std::string guildId;                                               ///< Guild ID (snowflake)
    std::optional<std::string> nick;                                   ///< Guild nickname
    std::optional<std::string> avatar;                                 ///< Guild-specific avatar hash
    std::vector<std::string> roleIds;                                  ///< Array of role IDs
    std::chrono::system_clock::time_point joinedAt;                    ///< When user joined guild
    std::optional<std::chrono::system_clock::time_point> premiumSince; ///< When user started boosting
    bool deaf = false;                                                 ///< Whether user is deafened
    bool mute = false;                                                 ///< Whether user is muted
    std::optional<int> flags;                                          ///< Member flags
    bool pending = false;                                              ///< Whether user has passed membership screening
    std::optional<std::chrono::system_clock::time_point> communicationDisabledUntil; ///< Timeout expiry
};
