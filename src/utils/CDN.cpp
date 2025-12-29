#include "utils/CDN.h"

#include <cstdint>
#include <sstream>

namespace CDNUtils {

std::string getImageExtension(const std::string &hash, bool preferWebp) {
    if (hash.length() >= 2 && hash[0] == 'a' && hash[1] == '_') {
        return ".gif";
    }

    return preferWebp ? ".webp" : ".png";
}

std::string getUserAvatarUrl(const std::string &userId, const std::string &avatarHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(avatarHash);
    std::ostringstream url;
    url << CDN_BASE << "/avatars/" << userId << "/" << avatarHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getDefaultAvatarUrl(const std::string &userId) {
    try {
        uint64_t id = std::stoull(userId);
        int index = ((id >> 22) % 6);
        std::ostringstream url;
        url << CDN_BASE << "/embed/avatars/" << index << ".png";
        return url.str();
    } catch (...) {
        return std::string(CDN_BASE) + "/embed/avatars/0.png";
    }
}

std::string getDefaultAvatarUrlLegacy(const std::string &discriminator) {
    try {
        int discrim = std::stoi(discriminator);
        int index = discrim % 5;
        std::ostringstream url;
        url << CDN_BASE << "/embed/avatars/" << index << ".png";
        return url.str();
    } catch (...) {
        return std::string(CDN_BASE) + "/embed/avatars/0.png";
    }
}

std::string getUserBannerUrl(const std::string &userId, const std::string &bannerHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(bannerHash);
    std::ostringstream url;
    url << CDN_BASE << "/banners/" << userId << "/" << bannerHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getGuildIconUrl(const std::string &guildId, const std::string &iconHash, int size, bool preferWebp) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(iconHash, preferWebp);
    std::ostringstream url;
    url << CDN_BASE << "/icons/" << guildId << "/" << iconHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getGuildSplashUrl(const std::string &guildId, const std::string &splashHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(splashHash);
    std::ostringstream url;
    url << CDN_BASE << "/splashes/" << guildId << "/" << splashHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getGuildDiscoverySplashUrl(const std::string &guildId, const std::string &discoverySplashHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(discoverySplashHash);
    std::ostringstream url;
    url << CDN_BASE << "/discovery-splashes/" << guildId << "/" << discoverySplashHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getGuildBannerUrl(const std::string &guildId, const std::string &bannerHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(bannerHash);
    std::ostringstream url;
    url << CDN_BASE << "/banners/" << guildId << "/" << bannerHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getMemberAvatarUrl(const std::string &guildId, const std::string &userId, const std::string &avatarHash,
                               int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(avatarHash);
    std::ostringstream url;
    url << CDN_BASE << "/guilds/" << guildId << "/users/" << userId << "/avatars/" << avatarHash << ext
        << "?size=" << normalizedSize;
    return url.str();
}

std::string getEmojiUrl(const std::string &emojiId, bool animated, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = animated ? ".gif" : ".webp";
    std::ostringstream url;
    url << CDN_BASE << "/emojis/" << emojiId << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getRoleIconUrl(const std::string &roleId, const std::string &iconHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(iconHash);
    std::ostringstream url;
    url << CDN_BASE << "/role-icons/" << roleId << "/" << iconHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getChannelIconUrl(const std::string &channelId, const std::string &iconHash, int size) {
    int normalizedSize = normalizeSize(size);
    std::string ext = getImageExtension(iconHash);
    std::ostringstream url;
    url << CDN_BASE << "/channel-icons/" << channelId << "/" << iconHash << ext << "?size=" << normalizedSize;
    return url.str();
}

std::string getStickerUrl(const std::string &stickerId) {
    std::ostringstream url;
    url << CDN_BASE << "/stickers/" << stickerId << ".png";
    return url.str();
}

std::string getAvatarDecorationUrl(const std::string &assetHash) {
    std::ostringstream url;
    // Avatar decorations always use .png (APNG for animated, regular PNG for static)
    url << CDN_BASE << "/avatar-decoration-presets/" << assetHash << ".png";
    return url.str();
}

std::string getNameplateUrl(const std::string &assetPath, int size) {
    (void)size; // Size parameter unused - nameplates use static.png
    std::ostringstream url;
    url << CDN_BASE << "/assets/collectibles/" << assetPath << "static.png";
    return url.str();
}

std::string getNameplateAnimatedUrl(const std::string &assetPath) {
    std::ostringstream url;
    url << CDN_BASE << "/assets/collectibles/" << assetPath << "asset.webm";
    return url.str();
}

} // namespace CDNUtils
