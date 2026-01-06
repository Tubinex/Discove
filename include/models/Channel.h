#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "models/PermissionOverwrite.h"
#include "models/User.h"
#include <nlohmann/json.hpp>

/**
 * @brief Discord channel types
 * @see https://discord.com/developers/docs/resources/channel#channel-object-channel-types
 */
enum class ChannelType {
    GUILD_TEXT = 0,
    DM = 1,
    GUILD_VOICE = 2,
    GROUP_DM = 3,
    GUILD_CATEGORY = 4,
    GUILD_ANNOUNCEMENT = 5,
    ANNOUNCEMENT_THREAD = 10,
    PUBLIC_THREAD = 11,
    PRIVATE_THREAD = 12,
    GUILD_STAGE_VOICE = 13,
    GUILD_DIRECTORY = 14,
    GUILD_FORUM = 15,
    GUILD_MEDIA = 16
};

/**
 * @brief Base class for all Discord channels
 * @see https://discord.com/developers/docs/resources/channel
 */
class Channel {
  public:
    virtual ~Channel() = default;

    /**
     * @brief Factory method to deserialize channel from JSON
     * @param j JSON object
     * @return Unique pointer to appropriate channel subclass
     */
    static std::unique_ptr<Channel> fromJson(const nlohmann::json &j);

    virtual bool isGuildChannel() const = 0;

    virtual bool isDM() const = 0;

    virtual bool isThread() const { return false; }

    virtual bool isVoice() const { return false; }

    std::string id;
    ChannelType type;
    std::optional<std::string> name;
    std::optional<std::string> lastMessageId;
};

class GuildChannel : public Channel {
  public:
    bool isGuildChannel() const override { return true; }
    bool isDM() const override { return false; }

    std::string guildId;
    int position = 0;
    std::optional<std::string> parentId;
    std::vector<PermissionOverwrite> permissionOverwrites;
};

class TextChannel : public GuildChannel {
  public:
    std::optional<std::string> topic;
    bool nsfw = false;
    std::optional<int> rateLimitPerUser;
};

class VoiceChannel : public GuildChannel {
  public:
    bool isVoice() const override { return true; }

    std::optional<int> bitrate;
    std::optional<int> userLimit;
};

class CategoryChannel : public GuildChannel {
  public:
};

class ThreadChannel : public GuildChannel {
  public:
    bool isThread() const override { return true; }

    std::optional<std::string> ownerId;
    std::optional<int> messageCount;
    std::optional<int> memberCount;
    std::optional<int> rateLimitPerUser;
};

class ForumChannel : public GuildChannel {
  public:
    std::optional<std::string> topic;
    bool nsfw = false;
};

class MediaChannel : public GuildChannel {
  public:
    std::optional<std::string> topic;
    bool nsfw = false;
};

class DirectoryChannel : public GuildChannel {
  public:
};

class DMChannel : public Channel {
  public:
    bool isGuildChannel() const override { return false; }
    bool isDM() const override { return true; }

    /**
     * @brief Get channel icon URL
     * @param size Desired size (default 256)
     * @return CDN URL for icon, or empty if none
     */
    std::string getIconUrl(int size = 256) const;

    std::vector<std::string> recipientIds;
    std::vector<User> recipients;
    std::optional<std::string> icon;
    std::optional<std::string> ownerId;
};
