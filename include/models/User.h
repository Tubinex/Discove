#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a Discord user
 * @see https://discord.com/developers/docs/resources/user
 */
class User {
  public:
    /**
     * @brief Deserialize user from JSON
     * @param j JSON object from Discord API
     * @return User instance
     */
    static User fromJson(const nlohmann::json &j);

    /**
     * @brief Get the user's display name (globalName or username)
     * @return Display name
     */
    std::string getDisplayName() const;

    /**
     * @brief Get the user's avatar URL from Discord CDN
     * @param size Desired image size (default 256)
     * @return CDN URL for avatar, or default avatar if none set
     */
    std::string getAvatarUrl(int size = 256) const;

    /**
     * @brief Get the user's banner URL from Discord CDN
     * @param size Desired image size (default 512)
     * @return CDN URL for banner, or empty string if none set
     */
    std::string getBannerUrl(int size = 512) const;

    /**
     * @brief Get the default avatar URL (for users without custom avatars)
     * @return Default avatar URL
     */
    std::string getDefaultAvatarUrl() const;

    std::string id;                              ///< User ID (snowflake)
    std::string username;                        ///< User's username
    std::string discriminator;                   ///< 4-digit tag ("0" for new system, "1234" for legacy)
    std::optional<std::string> globalName;       ///< User's display name (set by user)
    std::optional<std::string> avatar;           ///< Avatar hash
    std::optional<std::string> avatarDecoration; ///< Avatar decoration hash
    std::optional<std::string> banner;           ///< Banner hash
    std::optional<uint32_t> accentColor;         ///< Banner color (if no banner)
    bool bot = false;                            ///< Whether user is a bot
    bool system = false;                         ///< Whether user is a Discord system user
    std::optional<int> premiumType;              ///< Nitro type (0=None, 1=Classic, 2=Nitro, 3=Basic)
    std::optional<int> publicFlags;              ///< User flags bitfield
};
