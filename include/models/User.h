#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a user's nameplate collectible
 */
struct Nameplate {
    std::string asset;
    std::optional<std::string> expiresAt;
    std::optional<std::string> label;
    std::optional<std::string> palette;
    std::optional<std::string> skuId;
};

/**
 * @brief Represents a Discord user
 * @see https://discord.com/developers/docs/resources/user
 */
class User {
  public:
    /**
     * @brief Deserialize user from JSON
     * @param j JSON object
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

    /**
     * @brief Get the user's avatar decoration URL from Discord CDN
     * @return CDN URL for avatar decoration, or empty string if none set
     */
    std::string getAvatarDecorationUrl() const;

    /**
     * @brief Get the user's static nameplate URL from Discord CDN
     * @param size Desired image size (default 128, options: 64, 128, 256, 512)
     * @return CDN URL for static nameplate, or empty string if none set
     */
    std::string getNameplateUrl(int size = 128) const;

    /**
     * @brief Get the user's animated nameplate URL from Discord CDN
     * @return CDN URL for animated nameplate (WebM format), or empty string if none set
     */
    std::string getNameplateAnimatedUrl() const;

    std::string id;
    std::string username;
    std::string discriminator;
    std::optional<std::string> globalName;
    std::optional<std::string> avatar;
    std::optional<std::string> avatarDecoration;
    std::optional<std::string> banner;
    std::optional<uint32_t> accentColor;
    std::optional<Nameplate> nameplate;
    bool bot = false;
    bool system = false;
    std::optional<int> premiumType;
    std::optional<int> publicFlags;
};
