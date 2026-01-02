#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a Discord guild role
 * @see https://discord.com/developers/docs/topics/permissions#role-object
 */
class Role {
  public:
    /**
     * @brief Deserialize role from JSON
     * @param j JSON object
     * @return Role instance
     */
    static Role fromJson(const nlohmann::json &j);

    /**
     * @brief Get the CDN URL for the role icon
     * @param size Desired size (default 64)
     * @return CDN URL for role icon, or empty string if no icon
     */
    std::string getIconUrl(int size = 64) const;

    /**
     * @brief Check if this role has a specific permission
     * @param permission Permission bit to check
     * @return true if role has the permission
     */
    bool hasPermission(uint64_t permission) const;

    /**
     * @brief Get permissions as uint64_t
     * @return Permissions bitfield as integer
     */
    uint64_t getPermissionsInt() const;

    std::string id;
    std::string name;
    uint32_t color = 0;
    bool hoist = false;
    std::optional<std::string> icon;
    std::optional<std::string> unicodeEmoji;
    int position = 0;
    std::string permissions;
    bool managed = false;
    bool mentionable = false;
    std::optional<uint32_t> flags;
};
