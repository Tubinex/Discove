#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

/**
 * @brief Embed footer with text and optional icon
 */
struct EmbedFooter {
    std::string text;
    std::optional<std::string> iconUrl;
    std::optional<std::string> proxyIconUrl;

    static EmbedFooter fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed image
 */
struct EmbedImage {
    std::string url;
    std::optional<std::string> proxyUrl;
    std::optional<int> height;
    std::optional<int> width;

    static EmbedImage fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed thumbnail (smaller image)
 */
struct EmbedThumbnail {
    std::string url;
    std::optional<std::string> proxyUrl;
    std::optional<int> height;
    std::optional<int> width;

    static EmbedThumbnail fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed video
 */
struct EmbedVideo {
    std::optional<std::string> url;
    std::optional<std::string> proxyUrl;
    std::optional<int> height;
    std::optional<int> width;

    static EmbedVideo fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed provider (e.g., YouTube)
 */
struct EmbedProvider {
    std::optional<std::string> name;
    std::optional<std::string> url;

    static EmbedProvider fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed author with name and optional URL/icon
 */
struct EmbedAuthor {
    std::string name;
    std::optional<std::string> url;
    std::optional<std::string> iconUrl;
    std::optional<std::string> proxyIconUrl;

    static EmbedAuthor fromJson(const nlohmann::json &j);
};

/**
 * @brief Embed field
 */
struct EmbedField {
    std::string name;
    std::string value;
    bool inline_ = false;

    static EmbedField fromJson(const nlohmann::json &j);
};

/**
 * @brief Represents a rich embed in a Discord message
 * @see https://discord.com/developers/docs/resources/message#embed-object
 */
class Embed {
  public:
    /**
     * @brief Deserialize embed from JSON
     * @param j JSON object
     * @return Embed instance
     */
    static Embed fromJson(const nlohmann::json &j);

    std::optional<std::string> title;
    std::optional<std::string> type;
    std::optional<std::string> description;
    std::optional<std::string> url;
    std::optional<std::chrono::system_clock::time_point> timestamp;
    std::optional<uint32_t> color;
    std::optional<EmbedFooter> footer;
    std::optional<EmbedImage> image;
    std::optional<EmbedThumbnail> thumbnail;
    std::optional<EmbedVideo> video;
    std::optional<EmbedProvider> provider;
    std::optional<EmbedAuthor> author;
    std::vector<EmbedField> fields;
};
