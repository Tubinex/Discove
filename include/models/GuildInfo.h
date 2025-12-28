#pragma once

#include <string>

struct GuildInfo {
    std::string id;
    std::string name;
    std::string icon;
    std::string banner;

    bool operator==(const GuildInfo &other) const {
        return id == other.id && name == other.name && icon == other.icon && banner == other.banner;
    }

    bool operator!=(const GuildInfo &other) const { return !(*this == other); }
};
