#pragma once

#include <string>

struct GuildInfo {
    std::string id;
    std::string name;
    std::string icon;
    std::string banner;
    std::string rulesChannelId;
    int premiumTier = 0;
    int premiumSubscriptionCount = 0;

    bool operator==(const GuildInfo &other) const {
        return id == other.id && name == other.name && icon == other.icon && banner == other.banner &&
               rulesChannelId == other.rulesChannelId && premiumTier == other.premiumTier &&
               premiumSubscriptionCount == other.premiumSubscriptionCount;
    }

    bool operator!=(const GuildInfo &other) const { return !(*this == other); }
};
