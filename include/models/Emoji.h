#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a custom Discord emoji
 * @see https://discord.com/developers/docs/resources/emoji
 */
class Emoji {
  public:
    /**
     * @brief Deserialize emoji from JSON
     * @param j JSON object from Discord API
     * @return Emoji instance
     */
    static Emoji fromJson(const nlohmann::json &j);

    /**
     * @brief Get the CDN URL for this emoji
     * @param size Desired size (default 128)
     * @return CDN URL for the emoji
     */
    std::string getEmojiUrl(int size = 128) const;

    /**
     * @brief Get the mention string for this emoji (e.g., "<:name:id>")
     * @return Emoji mention string
     */
    std::string getMention() const;

    std::string id;                    ///< Emoji ID (snowflake)
    std::string name;                  ///< Emoji name
    std::vector<std::string> roleIds;  ///< Role IDs allowed to use this emoji (whitelist)
    std::optional<std::string> userId; ///< User who created this emoji
    bool requireColons = true;         ///< Whether emoji requires colons
    bool managed = false;              ///< Whether emoji is managed by an integration
    bool animated = false;             ///< Whether emoji is animated (.gif)
    bool available = true;             ///< Whether emoji can be used (guild boost level)
};
