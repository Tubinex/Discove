#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a permission overwrite for a role or user in a channel
 * @see https://discord.com/developers/docs/resources/channel#overwrite-object
 */
struct PermissionOverwrite {
    std::string id;
    int type;
    uint64_t allow;
    uint64_t deny;

    /**
     * @brief Parse PermissionOverwrite from JSON
     * @param j JSON object
     * @return PermissionOverwrite object
     */
    static PermissionOverwrite fromJson(const nlohmann::json &j);

    /**
     * @brief Parse multiple permission overwrites from JSON array
     * @param j JSON array
     * @return Vector of PermissionOverwrite objects
     */
    static std::vector<PermissionOverwrite> fromJsonArray(const nlohmann::json &j);
};
