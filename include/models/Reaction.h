#pragma once

#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a Discord reaction on a message
 * @see https://discord.com/developers/docs/resources/channel#reaction-object
 */
class Reaction {
  public:
    /**
     * @brief Deserialize reaction from JSON
     * @param j JSON object
     * @return Reaction instance
     */
    static Reaction fromJson(const nlohmann::json &j);

    int count = 0;
    bool me = false;
    std::string emojiId;
    std::string emojiName;
    bool emojiAnimated = false;
};
