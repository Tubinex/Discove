#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a Discord guild (server)
 * @see https://discord.com/developers/docs/resources/guild
 */
class Guild {
  public:
    /**
     * @brief Deserialize guild from JSON
     * @param j JSON object from Discord API
     * @return Guild instance
     */
    static Guild fromJson(const nlohmann::json &j);

    /**
     * @brief Get the guild icon URL
     * @param size Desired size (default 256)
     * @return CDN URL for guild icon, or empty if none
     */
    std::string getIconUrl(int size = 256) const;

    /**
     * @brief Get the guild splash URL
     * @param size Desired size (default 512)
     * @return CDN URL for splash, or empty if none
     */
    std::string getSplashUrl(int size = 512) const;

    /**
     * @brief Get the guild banner URL
     * @param size Desired size (default 512)
     * @return CDN URL for banner, or empty if none
     */
    std::string getBannerUrl(int size = 512) const;

    /**
     * @brief Check if guild has a specific feature
     * @param feature Feature string (e.g., "COMMUNITY", "VERIFIED")
     * @return true if guild has the feature
     */
    bool hasFeature(const std::string &feature) const;

    std::string id;                             ///< Guild ID (snowflake)
    std::string name;                           ///< Guild name
    std::optional<std::string> icon;            ///< Icon hash
    std::optional<std::string> splash;          ///< Splash hash
    std::optional<std::string> discoverySplash; ///< Discovery splash hash
    std::optional<std::string> banner;          ///< Banner hash
    std::string ownerId;                        ///< Owner's user ID
    std::optional<std::string> description;     ///< Guild description
    std::vector<std::string> features;          ///< Guild features array
    int verificationLevel = 0;                  ///< Verification level (0-4)
    int defaultMessageNotifications = 0;        ///< Default notification level (0-1)
    int explicitContentFilter = 0;              ///< Explicit content filter (0-2)
    int mfaLevel = 0;                           ///< MFA level (0-1)
    int premiumTier = 0;                        ///< Boost level (0-3)
    int premiumSubscriptionCount = 0;           ///< Number of boosts
    std::optional<std::string> preferredLocale; ///< Preferred locale (e.g., "en-US")
    bool unavailable = false;                   ///< Whether guild is unavailable
    int memberCount = 0;                        ///< Approximate member count
    bool large = false;                         ///< Whether guild is large (>75 members)
};
