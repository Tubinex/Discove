#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ProtobufUtils {

struct ParsedFolder {
    std::vector<std::string> guildIds;
    std::string name;
    uint32_t color{0};
    bool hasColor{false};
    int64_t id{-1};
};

struct ParsedStatus {
    std::string status;
    bool found{false};
};

bool parseGuildFoldersProto(const std::string &base64, std::vector<ParsedFolder> &foldersOut,
                            std::vector<uint64_t> &positionsOut);

bool parseStatusProto(const std::string &base64, ParsedStatus &statusOut);

} // namespace ProtobufUtils
