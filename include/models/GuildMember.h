#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a guild member
 * @see https://discord.com/developers/docs/resources/guild#guild-member-object
 */
struct GuildMember {
    std::string userId;
    std::vector<std::string> roleIds;
    std::string joinedAt;
    std::string nick;
    bool deaf = false;
    bool mute = false;
    bool pending = false;

    /**
     * @brief Parse GuildMember from JSON
     * @param j JSON object
     * @return GuildMember object
     */
    static GuildMember fromJson(const nlohmann::json &j);
};
