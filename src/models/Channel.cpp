#include "models/Channel.h"

#include "utils/CDN.h"

std::unique_ptr<Channel> Channel::fromJson(const nlohmann::json &j) {
    ChannelType type = static_cast<ChannelType>(j.at("type").get<int>());
    std::unique_ptr<Channel> channel;

    switch (type) {
    case ChannelType::GUILD_TEXT:
    case ChannelType::GUILD_ANNOUNCEMENT:
        channel = std::make_unique<TextChannel>();
        break;
    case ChannelType::GUILD_VOICE:
    case ChannelType::GUILD_STAGE_VOICE:
        channel = std::make_unique<VoiceChannel>();
        break;
    case ChannelType::GUILD_CATEGORY:
        channel = std::make_unique<CategoryChannel>();
        break;
    case ChannelType::ANNOUNCEMENT_THREAD:
    case ChannelType::PUBLIC_THREAD:
    case ChannelType::PRIVATE_THREAD:
        channel = std::make_unique<ThreadChannel>();
        break;
    case ChannelType::GUILD_FORUM:
        channel = std::make_unique<ForumChannel>();
        break;
    case ChannelType::GUILD_MEDIA:
        channel = std::make_unique<MediaChannel>();
        break;
    case ChannelType::GUILD_DIRECTORY:
        channel = std::make_unique<DirectoryChannel>();
        break;
    case ChannelType::DM:
    case ChannelType::GROUP_DM:
        channel = std::make_unique<DMChannel>();
        break;
    }

    channel->id = j.at("id").get<std::string>();
    channel->type = type;

    if (j.contains("name") && !j["name"].is_null()) {
        channel->name = j["name"].get<std::string>();
    }

    if (j.contains("last_message_id") && !j["last_message_id"].is_null()) {
        channel->lastMessageId = j["last_message_id"].get<std::string>();
    }

    if (auto *guildChannel = dynamic_cast<GuildChannel *>(channel.get())) {
        if (j.contains("guild_id") && !j["guild_id"].is_null()) {
            guildChannel->guildId = j["guild_id"].get<std::string>();
        }

        if (j.contains("position") && !j["position"].is_null()) {
            guildChannel->position = j["position"].get<int>();
        }

        if (j.contains("parent_id") && !j["parent_id"].is_null()) {
            guildChannel->parentId = j["parent_id"].get<std::string>();
        }

        if (j.contains("permission_overwrites") && j["permission_overwrites"].is_array()) {
            guildChannel->permissionOverwrites = PermissionOverwrite::fromJsonArray(j["permission_overwrites"]);
        }

        if (auto *textChannel = dynamic_cast<TextChannel *>(channel.get())) {
            if (j.contains("topic") && !j["topic"].is_null()) {
                textChannel->topic = j["topic"].get<std::string>();
            }
            if (j.contains("nsfw") && !j["nsfw"].is_null()) {
                textChannel->nsfw = j["nsfw"].get<bool>();
            }
            if (j.contains("rate_limit_per_user") && !j["rate_limit_per_user"].is_null()) {
                textChannel->rateLimitPerUser = j["rate_limit_per_user"].get<int>();
            }
        }

        if (auto *voiceChannel = dynamic_cast<VoiceChannel *>(channel.get())) {
            if (j.contains("bitrate") && !j["bitrate"].is_null()) {
                voiceChannel->bitrate = j["bitrate"].get<int>();
            }
            if (j.contains("user_limit") && !j["user_limit"].is_null()) {
                voiceChannel->userLimit = j["user_limit"].get<int>();
            }
        }

        if (auto *threadChannel = dynamic_cast<ThreadChannel *>(channel.get())) {
            if (j.contains("owner_id") && !j["owner_id"].is_null()) {
                threadChannel->ownerId = j["owner_id"].get<std::string>();
            }
            if (j.contains("message_count") && !j["message_count"].is_null()) {
                threadChannel->messageCount = j["message_count"].get<int>();
            }
            if (j.contains("member_count") && !j["member_count"].is_null()) {
                threadChannel->memberCount = j["member_count"].get<int>();
            }
            if (j.contains("rate_limit_per_user") && !j["rate_limit_per_user"].is_null()) {
                threadChannel->rateLimitPerUser = j["rate_limit_per_user"].get<int>();
            }
        }

        if (auto *forumChannel = dynamic_cast<ForumChannel *>(channel.get())) {
            if (j.contains("topic") && !j["topic"].is_null()) {
                forumChannel->topic = j["topic"].get<std::string>();
            }
            if (j.contains("nsfw") && !j["nsfw"].is_null()) {
                forumChannel->nsfw = j["nsfw"].get<bool>();
            }
        }

        if (auto *mediaChannel = dynamic_cast<MediaChannel *>(channel.get())) {
            if (j.contains("topic") && !j["topic"].is_null()) {
                mediaChannel->topic = j["topic"].get<std::string>();
            }
            if (j.contains("nsfw") && !j["nsfw"].is_null()) {
                mediaChannel->nsfw = j["nsfw"].get<bool>();
            }
        }
    }

    if (auto *dmChannel = dynamic_cast<DMChannel *>(channel.get())) {
        if (j.contains("recipient_ids") && j["recipient_ids"].is_array()) {
            for (const auto &recipient : j["recipient_ids"]) {
                dmChannel->recipientIds.push_back(recipient.get<std::string>());
            }
        }

        if (j.contains("icon") && !j["icon"].is_null()) {
            dmChannel->icon = j["icon"].get<std::string>();
        }

        if (j.contains("owner_id") && !j["owner_id"].is_null()) {
            dmChannel->ownerId = j["owner_id"].get<std::string>();
        }
    }

    return channel;
}

std::string DMChannel::getIconUrl(int size) const {
    if (icon.has_value()) {
        return CDNUtils::getChannelIconUrl(id, *icon, size);
    }
    return "";
}
