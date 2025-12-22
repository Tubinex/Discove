#include "models/Embed.h"

#include "utils/Time.h"

EmbedFooter EmbedFooter::fromJson(const nlohmann::json &j) {
    EmbedFooter footer;
    footer.text = j.at("text").get<std::string>();

    if (j.contains("icon_url") && !j["icon_url"].is_null()) {
        footer.iconUrl = j["icon_url"].get<std::string>();
    }

    if (j.contains("proxy_icon_url") && !j["proxy_icon_url"].is_null()) {
        footer.proxyIconUrl = j["proxy_icon_url"].get<std::string>();
    }

    return footer;
}

EmbedImage EmbedImage::fromJson(const nlohmann::json &j) {
    EmbedImage image;
    image.url = j.at("url").get<std::string>();

    if (j.contains("proxy_url") && !j["proxy_url"].is_null()) {
        image.proxyUrl = j["proxy_url"].get<std::string>();
    }

    if (j.contains("height") && !j["height"].is_null()) {
        image.height = j["height"].get<int>();
    }

    if (j.contains("width") && !j["width"].is_null()) {
        image.width = j["width"].get<int>();
    }

    return image;
}

EmbedThumbnail EmbedThumbnail::fromJson(const nlohmann::json &j) {
    EmbedThumbnail thumbnail;
    thumbnail.url = j.at("url").get<std::string>();

    if (j.contains("proxy_url") && !j["proxy_url"].is_null()) {
        thumbnail.proxyUrl = j["proxy_url"].get<std::string>();
    }

    if (j.contains("height") && !j["height"].is_null()) {
        thumbnail.height = j["height"].get<int>();
    }

    if (j.contains("width") && !j["width"].is_null()) {
        thumbnail.width = j["width"].get<int>();
    }

    return thumbnail;
}

EmbedVideo EmbedVideo::fromJson(const nlohmann::json &j) {
    EmbedVideo video;

    if (j.contains("url") && !j["url"].is_null()) {
        video.url = j["url"].get<std::string>();
    }

    if (j.contains("proxy_url") && !j["proxy_url"].is_null()) {
        video.proxyUrl = j["proxy_url"].get<std::string>();
    }

    if (j.contains("height") && !j["height"].is_null()) {
        video.height = j["height"].get<int>();
    }

    if (j.contains("width") && !j["width"].is_null()) {
        video.width = j["width"].get<int>();
    }

    return video;
}

EmbedProvider EmbedProvider::fromJson(const nlohmann::json &j) {
    EmbedProvider provider;

    if (j.contains("name") && !j["name"].is_null()) {
        provider.name = j["name"].get<std::string>();
    }

    if (j.contains("url") && !j["url"].is_null()) {
        provider.url = j["url"].get<std::string>();
    }

    return provider;
}

EmbedAuthor EmbedAuthor::fromJson(const nlohmann::json &j) {
    EmbedAuthor author;
    author.name = j.at("name").get<std::string>();

    if (j.contains("url") && !j["url"].is_null()) {
        author.url = j["url"].get<std::string>();
    }

    if (j.contains("icon_url") && !j["icon_url"].is_null()) {
        author.iconUrl = j["icon_url"].get<std::string>();
    }

    if (j.contains("proxy_icon_url") && !j["proxy_icon_url"].is_null()) {
        author.proxyIconUrl = j["proxy_icon_url"].get<std::string>();
    }

    return author;
}

EmbedField EmbedField::fromJson(const nlohmann::json &j) {
    EmbedField field;
    field.name = j.at("name").get<std::string>();
    field.value = j.at("value").get<std::string>();

    if (j.contains("inline") && !j["inline"].is_null()) {
        field.inline_ = j["inline"].get<bool>();
    }

    return field;
}

Embed Embed::fromJson(const nlohmann::json &j) {
    Embed embed;

    if (j.contains("title") && !j["title"].is_null()) {
        embed.title = j["title"].get<std::string>();
    }

    if (j.contains("type") && !j["type"].is_null()) {
        embed.type = j["type"].get<std::string>();
    }

    if (j.contains("description") && !j["description"].is_null()) {
        embed.description = j["description"].get<std::string>();
    }

    if (j.contains("url") && !j["url"].is_null()) {
        embed.url = j["url"].get<std::string>();
    }

    if (j.contains("timestamp") && !j["timestamp"].is_null()) {
        std::string timestampStr = j["timestamp"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(timestampStr);
        if (parsed.has_value()) {
            embed.timestamp = *parsed;
        }
    }

    if (j.contains("color") && !j["color"].is_null()) {
        embed.color = j["color"].get<uint32_t>();
    }

    if (j.contains("footer") && !j["footer"].is_null()) {
        embed.footer = EmbedFooter::fromJson(j["footer"]);
    }

    if (j.contains("image") && !j["image"].is_null()) {
        embed.image = EmbedImage::fromJson(j["image"]);
    }

    if (j.contains("thumbnail") && !j["thumbnail"].is_null()) {
        embed.thumbnail = EmbedThumbnail::fromJson(j["thumbnail"]);
    }

    if (j.contains("video") && !j["video"].is_null()) {
        embed.video = EmbedVideo::fromJson(j["video"]);
    }

    if (j.contains("provider") && !j["provider"].is_null()) {
        embed.provider = EmbedProvider::fromJson(j["provider"]);
    }

    if (j.contains("author") && !j["author"].is_null()) {
        embed.author = EmbedAuthor::fromJson(j["author"]);
    }

    if (j.contains("fields") && j["fields"].is_array()) {
        for (const auto &fieldJson : j["fields"]) {
            embed.fields.push_back(EmbedField::fromJson(fieldJson));
        }
    }

    return embed;
}
