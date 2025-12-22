#include "models/Attachment.h"

Attachment Attachment::fromJson(const nlohmann::json &j) {
    Attachment attachment;

    attachment.id = j.at("id").get<std::string>();
    attachment.filename = j.at("filename").get<std::string>();
    attachment.size = j.at("size").get<uint64_t>();
    attachment.url = j.at("url").get<std::string>();
    attachment.proxyUrl = j.at("proxy_url").get<std::string>();

    if (j.contains("description") && !j["description"].is_null()) {
        attachment.description = j["description"].get<std::string>();
    }

    if (j.contains("content_type") && !j["content_type"].is_null()) {
        attachment.contentType = j["content_type"].get<std::string>();
    }

    if (j.contains("height") && !j["height"].is_null()) {
        attachment.height = j["height"].get<int>();
    }

    if (j.contains("width") && !j["width"].is_null()) {
        attachment.width = j["width"].get<int>();
    }

    if (j.contains("ephemeral") && !j["ephemeral"].is_null()) {
        attachment.ephemeral = j["ephemeral"].get<bool>();
    }

    if (j.contains("duration_secs") && !j["duration_secs"].is_null()) {
        attachment.durationSecs = j["duration_secs"].get<double>();
    }

    if (j.contains("waveform") && !j["waveform"].is_null()) {
        attachment.waveform = j["waveform"].get<std::string>();
    }

    if (j.contains("flags") && !j["flags"].is_null()) {
        attachment.flags = j["flags"].get<uint32_t>();
    }

    return attachment;
}

bool Attachment::isImage() const {
    if (!contentType.has_value()) {
        return false;
    }

    const std::string &type = *contentType;
    return type.find("image/") == 0;
}

bool Attachment::isVideo() const {
    if (!contentType.has_value()) {
        return false;
    }

    const std::string &type = *contentType;
    return type.find("video/") == 0;
}

bool Attachment::isAudio() const {
    if (!contentType.has_value()) {
        return false;
    }

    const std::string &type = *contentType;
    return type.find("audio/") == 0;
}
