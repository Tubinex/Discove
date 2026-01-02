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
     * @param j JSON object
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
     * @brief Get the mention string for this emoji (<:name:id>)
     * @return Emoji mention string
     */
    std::string getMention() const;

    std::string id;
    std::string name;
    std::vector<std::string> roleIds;
    std::optional<std::string> userId;
    bool requireColons = true;
    bool managed = false;
    bool animated = false;
    bool available = true;
};
