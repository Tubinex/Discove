#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

/**
 * @brief Represents a file attachment in a Discord message
 * @see https://discord.com/developers/docs/resources/message#attachment-object
 */
class Attachment {
  public:
    /**
     * @brief Deserialize attachment from JSON
     * @param j JSON object from Discord API
     * @return Attachment instance
     */
    static Attachment fromJson(const nlohmann::json &j);

    /**
     * @brief Check if attachment is an image
     * @return true if content type is image/*
     */
    bool isImage() const;

    /**
     * @brief Check if attachment is a video
     * @return true if content type is video/*
     */
    bool isVideo() const;

    /**
     * @brief Check if attachment is audio
     * @return true if content type is audio/*
     */
    bool isAudio() const;

    std::string id;                         ///< Attachment ID (snowflake)
    std::string filename;                   ///< Original filename
    std::optional<std::string> description; ///< Alt text description
    std::optional<std::string> contentType; ///< MIME type (e.g., "image/png")
    uint64_t size = 0;                      ///< File size in bytes
    std::string url;                        ///< CDN URL for the file
    std::string proxyUrl;                   ///< Proxied URL
    std::optional<int> height;              ///< Image/video height in pixels
    std::optional<int> width;               ///< Image/video width in pixels
    std::optional<bool> ephemeral;          ///< Whether attachment is ephemeral
    std::optional<double> durationSecs;     ///< Audio/video duration in seconds
    std::optional<std::string> waveform;    ///< Base64 encoded waveform for voice messages
    std::optional<uint32_t> flags;          ///< Attachment flags bitfield
};
