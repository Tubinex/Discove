#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a Discord guild
 * @see https://discord.com/developers/docs/resources/guild
 */
class Guild {
  public:
    /**
     * @brief Deserialize guild from JSON
     * @param j JSON object
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

    std::string id;
    std::string name;
    std::optional<std::string> icon;
    std::optional<std::string> splash;
    std::optional<std::string> discoverySplash;
    std::optional<std::string> banner;
    std::string ownerId;
    std::optional<std::string> description;
    std::vector<std::string> features;
    int verificationLevel = 0;
    int defaultMessageNotifications = 0;
    int explicitContentFilter = 0;
    int mfaLevel = 0;
    int premiumTier = 0;
    int premiumSubscriptionCount = 0;
    std::optional<std::string> preferredLocale;
    bool unavailable = false;
    int memberCount = 0;
    bool large = false;
};
