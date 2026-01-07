#pragma once

#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a sticker item referenced by a Discord message
 * @see https://discord.com/developers/docs/resources/message#message-object-message-structure
 */
class StickerItem {
  public:
    /**
     * @brief Deserialize sticker item from JSON
     * @param j JSON object
     * @return StickerItem instance
     */
    static StickerItem fromJson(const nlohmann::json &j);

    std::string id;
    std::string name;
    int formatType = 0;
};

