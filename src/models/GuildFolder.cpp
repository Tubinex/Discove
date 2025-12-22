#include "models/GuildFolder.h"

GuildFolder GuildFolder::fromProtobuf(const ProtobufUtils::ParsedFolder &proto) {
    GuildFolder folder;

    folder.id = proto.id;
    folder.name = proto.name;
    folder.guildIds = proto.guildIds;

    if (proto.hasColor) {
        folder.color = proto.color;
    }

    return folder;
}

bool GuildFolder::isEmpty() const { return guildIds.empty(); }
