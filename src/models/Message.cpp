#include "models/Message.h"

#include "utils/Time.h"

Message Message::fromJson(const nlohmann::json &j) {
    Message message;

    message.id = j.at("id").get<std::string>();
    message.channelId = j.at("channel_id").get<std::string>();
    message.content = j.at("content").get<std::string>();

    if (j.contains("author") && j["author"].is_object()) {
        const auto &author = j["author"];
        if (author.contains("id")) {
            message.authorId = author["id"].get<std::string>();
        }
    }

    if (j.contains("timestamp")) {
        std::string timestampStr = j["timestamp"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(timestampStr);
        if (parsed.has_value()) {
            message.timestamp = *parsed;
        } else {
            message.timestamp = std::chrono::system_clock::now();
        }
    }

    if (j.contains("type")) {
        message.type = static_cast<MessageType>(j["type"].get<int>());
    } else {
        message.type = MessageType::DEFAULT;
    }

    if (j.contains("guild_id") && !j["guild_id"].is_null()) {
        message.guildId = j["guild_id"].get<std::string>();
    }

    if (j.contains("edited_timestamp") && !j["edited_timestamp"].is_null()) {
        std::string editedStr = j["edited_timestamp"].get<std::string>();
        auto parsed = TimeUtils::parseISO8601(editedStr);
        if (parsed.has_value()) {
            message.editedTimestamp = *parsed;
        }
    }

    if (j.contains("tts") && !j["tts"].is_null()) {
        message.tts = j["tts"].get<bool>();
    }

    if (j.contains("mention_everyone") && !j["mention_everyone"].is_null()) {
        message.mentionEveryone = j["mention_everyone"].get<bool>();
    }

    if (j.contains("mentions") && j["mentions"].is_array()) {
        for (const auto &mention : j["mentions"]) {
            if (mention.contains("id")) {
                message.mentionIds.push_back(mention["id"].get<std::string>());
            }
        }
    }

    if (j.contains("mention_roles") && j["mention_roles"].is_array()) {
        for (const auto &roleId : j["mention_roles"]) {
            message.mentionRoleIds.push_back(roleId.get<std::string>());
        }
    }

    if (j.contains("attachments") && j["attachments"].is_array()) {
        for (const auto &attachmentJson : j["attachments"]) {
            message.attachments.push_back(Attachment::fromJson(attachmentJson));
        }
    }

    if (j.contains("embeds") && j["embeds"].is_array()) {
        for (const auto &embedJson : j["embeds"]) {
            message.embeds.push_back(Embed::fromJson(embedJson));
        }
    }

    if (j.contains("nonce") && !j["nonce"].is_null()) {
        if (j["nonce"].is_string()) {
            message.nonce = j["nonce"].get<std::string>();
        } else if (j["nonce"].is_number()) {
            message.nonce = std::to_string(j["nonce"].get<int64_t>());
        }
    }

    if (j.contains("pinned") && !j["pinned"].is_null()) {
        message.pinned = j["pinned"].get<bool>();
    }

    if (j.contains("webhook_id") && !j["webhook_id"].is_null()) {
        message.webhookId = j["webhook_id"].get<std::string>();
    }

    if (j.contains("application_id") && !j["application_id"].is_null()) {
        message.applicationId = j["application_id"].get<std::string>();
    }

    if (j.contains("message_reference") && j["message_reference"].is_object()) {
        const auto &ref = j["message_reference"];
        if (ref.contains("message_id") && !ref["message_id"].is_null()) {
            message.referencedMessageId = ref["message_id"].get<std::string>();
        }
    }

    return message;
}

bool Message::isReply() const { return type == MessageType::REPLY || referencedMessageId.has_value(); }

bool Message::isSystemMessage() const { return type != MessageType::DEFAULT && type != MessageType::REPLY; }

std::string Message::getJumpUrl() const {
    std::string base = "https://discord.com/channels/";
    if (guildId.has_value()) {
        return base + *guildId + "/" + channelId + "/" + id;
    } else {
        return base + "@me/" + channelId + "/" + id;
    }
}

bool Message::wasEdited() const { return editedTimestamp.has_value(); }
