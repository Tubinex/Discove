#pragma once

#include <string>

namespace CDNUtils {

constexpr const char *CDN_BASE = "https://cdn.discordapp.com";

/**
 * @brief Normalize size to valid Discord CDN size (16, 32, 48, 64, 128)
 * @param size Requested size
 * @return Closest valid Discord CDN size
 */
inline int normalizeSize(int size) {
    if (size <= 16)
        return 16;
    if (size <= 32)
        return 32;
    if (size <= 48)
        return 48;
    if (size <= 64)
        return 64;
    return 128;
}

/**
 * @brief Get the file extension for an image hash
 * @param hash The image hash
 * @param preferWebp Whether to prefer .webp over .png for static images
 * @return ".gif" for animated images, ".webp" or ".png" for static
 */
std::string getImageExtension(const std::string &hash, bool preferWebp = true);

/**
 * @brief Construct user avatar URL
 * @param userId User's snowflake ID
 * @param avatarHash User's avatar hash
 * @param size Desired size (16-4096)
 * @return CDN URL for user avatar
 * @see https://discord.com/developers/docs/reference#image-formatting
 */
std::string getUserAvatarUrl(const std::string &userId, const std::string &avatarHash, int size = 256);

/**
 * @brief Get default avatar URL for users without custom avatars
 * New username system: Uses user ID modulo 6
 * @param userId User's snowflake ID
 * @return CDN URL for default avatar (0-5)
 */
std::string getDefaultAvatarUrl(const std::string &userId);

/**
 * @brief Get default avatar URL for users with legacy discriminators
 * @param discriminator User's 4-digit discriminator
 * @return CDN URL for default avatar (discriminator % 5)
 */
std::string getDefaultAvatarUrlLegacy(const std::string &discriminator);

/**
 * @brief Construct user banner URL
 * @param userId User's snowflake ID
 * @param bannerHash User's banner hash
 * @param size Desired size (default 512)
 * @return CDN URL for user banner
 */
std::string getUserBannerUrl(const std::string &userId, const std::string &bannerHash, int size = 512);

/**
 * @brief Construct guild icon URL
 * @param guildId Guild's snowflake ID
 * @param iconHash Guild's icon hash
 * @param size Desired size (default 256)
 * @param preferWebp Whether to prefer .webp over .png for static images (default false)
 * @return CDN URL for guild icon
 */
std::string getGuildIconUrl(const std::string &guildId, const std::string &iconHash, int size = 256,
                            bool preferWebp = false);

/**
 * @brief Construct guild splash URL
 * @param guildId Guild's snowflake ID
 * @param splashHash Guild's splash hash
 * @param size Desired size (default 512)
 * @return CDN URL for guild splash
 */
std::string getGuildSplashUrl(const std::string &guildId, const std::string &splashHash, int size = 512);

/**
 * @brief Construct guild discovery splash URL
 * @param guildId Guild's snowflake ID
 * @param discoverySplashHash Guild's discovery splash hash
 * @param size Desired size (default 512)
 * @return CDN URL for discovery splash
 */
std::string getGuildDiscoverySplashUrl(const std::string &guildId, const std::string &discoverySplashHash,
                                       int size = 512);

/**
 * @brief Construct guild banner URL
 * @param guildId Guild's snowflake ID
 * @param bannerHash Guild's banner hash
 * @param size Desired size (default 512)
 * @return CDN URL for guild banner
 */
std::string getGuildBannerUrl(const std::string &guildId, const std::string &bannerHash, int size = 512);

/**
 * @brief Construct guild member avatar URL
 * @param guildId Guild's snowflake ID
 * @param userId User's snowflake ID
 * @param avatarHash Member's guild-specific avatar hash
 * @param size Desired size (default 256)
 * @return CDN URL for member avatar
 */
std::string getMemberAvatarUrl(const std::string &guildId, const std::string &userId, const std::string &avatarHash,
                               int size = 256);

/**
 * @brief Construct custom emoji URL
 * @param emojiId Emoji's snowflake ID
 * @param animated Whether the emoji is animated
 * @param size Desired size (default 128)
 * @return CDN URL for custom emoji
 */
std::string getEmojiUrl(const std::string &emojiId, bool animated, int size = 128);

/**
 * @brief Construct role icon URL
 * @param roleId Role's snowflake ID
 * @param iconHash Role's icon hash
 * @param size Desired size (default 64)
 * @return CDN URL for role icon
 */
std::string getRoleIconUrl(const std::string &roleId, const std::string &iconHash, int size = 64);

/**
 * @brief Construct channel icon URL
 * @param channelId Channel's snowflake ID
 * @param iconHash Channel's icon hash
 * @param size Desired size (default 256)
 * @return CDN URL for channel icon
 */
std::string getChannelIconUrl(const std::string &channelId, const std::string &iconHash, int size = 256);

/**
 * @brief Construct sticker URL
 * @param stickerId Sticker's snowflake ID
 * @return CDN URL for sticker
 */
std::string getStickerUrl(const std::string &stickerId);

/**
 * @brief Construct avatar decoration URL
 * @param assetHash Avatar decoration asset hash
 * @return CDN URL for avatar decoration preset
 * @note Animated decorations return .png URLs but contain APNG data, not .gif
 */
std::string getAvatarDecorationUrl(const std::string &assetHash);

/**
 * @brief Construct static nameplate URL
 * @param assetPath Nameplate asset path
 * @param size Unused - kept for API compatibility
 * @return CDN URL for static nameplate
 */
std::string getNameplateUrl(const std::string &assetPath, int size = 128);

/**
 * @brief Construct animated nameplate URL
 * @param assetPath Nameplate asset path
 * @return CDN URL for animated nameplate
 * @note Returns WebM video
 */
std::string getNameplateAnimatedUrl(const std::string &assetPath);

} // namespace CDNUtils
