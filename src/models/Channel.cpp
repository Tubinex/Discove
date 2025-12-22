#include "models/Channel.h"

#include "utils/CDN.h"

Channel Channel::fromJson(const nlohmann::json &j) {
    Channel channel;

    channel.id = j.at("id").get<std::string>();
    channel.type = static_cast<ChannelType>(j.at("type").get<int>());

    if (j.contains("guild_id") && !j["guild_id"].is_null()) {
        channel.guildId = j["guild_id"].get<std::string>();
    }

    if (j.contains("position") && !j["position"].is_null()) {
        channel.position = j["position"].get<int>();
    }

    if (j.contains("name") && !j["name"].is_null()) {
        channel.name = j["name"].get<std::string>();
    }

    if (j.contains("topic") && !j["topic"].is_null()) {
        channel.topic = j["topic"].get<std::string>();
    }

    if (j.contains("nsfw") && !j["nsfw"].is_null()) {
        channel.nsfw = j["nsfw"].get<bool>();
    }

    if (j.contains("last_message_id") && !j["last_message_id"].is_null()) {
        channel.lastMessageId = j["last_message_id"].get<std::string>();
    }

    if (j.contains("bitrate") && !j["bitrate"].is_null()) {
        channel.bitrate = j["bitrate"].get<int>();
    }

    if (j.contains("user_limit") && !j["user_limit"].is_null()) {
        channel.userLimit = j["user_limit"].get<int>();
    }

    if (j.contains("rate_limit_per_user") && !j["rate_limit_per_user"].is_null()) {
        channel.rateLimitPerUser = j["rate_limit_per_user"].get<int>();
    }

    if (j.contains("recipients") && j["recipients"].is_array()) {
        for (const auto &recipient : j["recipients"]) {
            if (recipient.contains("id")) {
                channel.recipientIds.push_back(recipient["id"].get<std::string>());
            }
        }
    }

    if (j.contains("icon") && !j["icon"].is_null()) {
        channel.icon = j["icon"].get<std::string>();
    }

    if (j.contains("owner_id") && !j["owner_id"].is_null()) {
        channel.ownerId = j["owner_id"].get<std::string>();
    }

    if (j.contains("parent_id") && !j["parent_id"].is_null()) {
        channel.parentId = j["parent_id"].get<std::string>();
    }

    if (j.contains("message_count") && !j["message_count"].is_null()) {
        channel.messageCount = j["message_count"].get<int>();
    }

    if (j.contains("member_count") && !j["member_count"].is_null()) {
        channel.memberCount = j["member_count"].get<int>();
    }

    return channel;
}

bool Channel::isGuildChannel() const { return guildId.has_value(); }

bool Channel::isDM() const { return type == ChannelType::DM || type == ChannelType::GROUP_DM; }

bool Channel::isThread() const {
    return type == ChannelType::ANNOUNCEMENT_THREAD || type == ChannelType::PUBLIC_THREAD ||
           type == ChannelType::PRIVATE_THREAD;
}

bool Channel::isVoice() const { return type == ChannelType::GUILD_VOICE || type == ChannelType::GUILD_STAGE_VOICE; }

std::string Channel::getIconUrl(int size) const {
    if (icon.has_value()) {
        return CDNUtils::getChannelIconUrl(id, *icon, size);
    }
    return "";
}
