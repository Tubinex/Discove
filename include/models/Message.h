#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "models/Attachment.h"
#include "models/Embed.h"
#include "models/Reaction.h"

/**
 * @brief Discord message types
 * @see https://discord.com/developers/docs/resources/message#message-object-message-types
 */
enum class MessageType {
    DEFAULT = 0,
    RECIPIENT_ADD = 1,
    RECIPIENT_REMOVE = 2,
    CALL = 3,
    CHANNEL_NAME_CHANGE = 4,
    CHANNEL_ICON_CHANGE = 5,
    CHANNEL_PINNED_MESSAGE = 6,
    USER_JOIN = 7,
    GUILD_BOOST = 8,
    GUILD_BOOST_TIER_1 = 9,
    GUILD_BOOST_TIER_2 = 10,
    GUILD_BOOST_TIER_3 = 11,
    CHANNEL_FOLLOW_ADD = 12,
    GUILD_DISCOVERY_DISQUALIFIED = 14,
    GUILD_DISCOVERY_REQUALIFIED = 15,
    GUILD_DISCOVERY_GRACE_PERIOD_INITIAL_WARNING = 16,
    GUILD_DISCOVERY_GRACE_PERIOD_FINAL_WARNING = 17,
    THREAD_CREATED = 18,
    REPLY = 19,
    CHAT_INPUT_COMMAND = 20,
    THREAD_STARTER_MESSAGE = 21,
    GUILD_INVITE_REMINDER = 22,
    CONTEXT_MENU_COMMAND = 23,
    AUTO_MODERATION_ACTION = 24,
    ROLE_SUBSCRIPTION_PURCHASE = 25,
    INTERACTION_PREMIUM_UPSELL = 26,
    STAGE_START = 27,
    STAGE_END = 28,
    STAGE_SPEAKER = 29,
    STAGE_TOPIC = 31,
    GUILD_APPLICATION_PREMIUM_SUBSCRIPTION = 32,
    POLL = 46
};

/**
 * @brief Represents a Discord message
 * @see https://discord.com/developers/docs/resources/message
 */
class Message {
  public:
    /**
     * @brief Deserialize message from JSON
     * @param j JSON object
     * @return Message instance
     */
    static Message fromJson(const nlohmann::json &j);

    /**
     * @brief Check if message is a reply to another message
     * @return true if message is a reply
     */
    bool isReply() const;

    /**
     * @brief Check if message is a system message
     * @return true if message is system-generated
     */
    bool isSystemMessage() const;

    /**
     * @brief Get the jump URL for this message
     * @return Discord jump URL
     */
    std::string getJumpUrl() const;

    /**
     * @brief Get the display name for the message author
     * @return Display name
     */
    std::string getAuthorDisplayName() const;

    /**
     * @brief Get the avatar URL for the message author
     * @param size Desired image size (default 64)
     * @return CDN URL for avatar
     */
    std::string getAuthorAvatarUrl(int size = 64) const;

    /**
     * @brief Check if message was edited
     * @return true if message has been edited
     */
    bool wasEdited() const;

    std::string id;
    std::string channelId;
    std::string authorId;
    std::string authorUsername;
    std::string authorGlobalName;
    std::string authorNickname;
    std::string authorDiscriminator;
    std::string authorAvatarHash;
    std::string authorMemberAvatarHash;
    std::string content;
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::chrono::system_clock::time_point> editedTimestamp;
    bool tts = false;
    bool mentionEveryone = false;
    std::vector<std::string> mentionIds;
    std::vector<std::string> mentionDisplayNames;
    std::vector<std::string> mentionRoleIds;
    std::vector<Attachment> attachments;
    std::vector<Embed> embeds;
    std::vector<Reaction> reactions;
    std::optional<std::string> nonce;
    bool isPending = false;
    bool pinned = false;
    std::optional<std::string> webhookId;
    MessageType type = MessageType::DEFAULT;
    std::optional<std::string> applicationId;
    std::optional<std::string> referencedMessageId;
    std::optional<std::string> guildId;
};
