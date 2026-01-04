#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "models/Channel.h"
#include "models/GuildFolder.h"
#include "models/GuildInfo.h"
#include "models/GuildMember.h"
#include "models/Message.h"
#include "models/Role.h"

struct CustomStatus {
    std::string text;
    std::string emojiName;
    std::string emojiId;
    std::string emojiUrl;
    bool emojiAnimated = false;

    bool isEmpty() const { return text.empty() && emojiUrl.empty(); }

    bool operator==(const CustomStatus &other) const {
        return text == other.text && emojiName == other.emojiName && emojiId == other.emojiId &&
               emojiUrl == other.emojiUrl && emojiAnimated == other.emojiAnimated;
    }

    bool operator!=(const CustomStatus &other) const { return !(*this == other); }
};

struct RouteState {
    std::string currentPath = "";
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> query;
    std::vector<std::string> history;
    int historyIndex = 0;
    bool isTransitioning = false;

    bool operator==(const RouteState &other) const {
        return currentPath == other.currentPath && params == other.params && query == other.query &&
               historyIndex == other.historyIndex && isTransitioning == other.isTransitioning;
    }

    bool operator!=(const RouteState &other) const { return !(*this == other); }
};

struct UserProfile {
    std::string id;
    std::string username;
    std::string discriminator = "0";
    std::string globalName;
    std::string avatarHash;
    std::string avatarUrl;
    std::string status = "online";
    std::optional<CustomStatus> customStatus;

    bool operator==(const UserProfile &other) const {
        return id == other.id && username == other.username && discriminator == other.discriminator &&
               globalName == other.globalName && avatarHash == other.avatarHash && avatarUrl == other.avatarUrl &&
               status == other.status && customStatus == other.customStatus;
    }

    bool operator!=(const UserProfile &other) const { return !(*this == other); }
};

struct AppState {
    int counter = 0;
    RouteState route;
    std::optional<UserProfile> currentUser;
    std::vector<GuildFolder> guildFolders;
    std::vector<uint64_t> guildPositions;
    std::vector<GuildInfo> guilds;
    std::vector<std::shared_ptr<DMChannel>> privateChannels;
    std::unordered_map<std::string, std::vector<std::shared_ptr<GuildChannel>>> guildChannels;
    std::unordered_map<std::string, GuildMember> guildMembers;
    std::unordered_map<std::string, std::vector<Role>> guildRoles;
    std::unordered_map<std::string, std::vector<Message>> channelMessages;
};
