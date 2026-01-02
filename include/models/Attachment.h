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
     * @param j JSON object
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

    std::string id;
    std::string filename;
    std::optional<std::string> description;
    std::optional<std::string> contentType;
    uint64_t size = 0;
    std::string url;
    std::string proxyUrl;
    std::optional<int> height;
    std::optional<int> width;
    std::optional<bool> ephemeral;
    std::optional<double> durationSecs;
    std::optional<std::string> waveform;
    std::optional<uint32_t> flags;
};
