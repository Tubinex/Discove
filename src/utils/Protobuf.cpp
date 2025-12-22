#include "utils/Protobuf.h"

#include <cstring>
#include <sstream>

namespace ProtobufUtils {
static bool base64Decode(const std::string &input, std::vector<uint8_t> &out);
static bool readVarint(const std::vector<uint8_t> &data, size_t &offset, uint64_t &out);

bool base64Decode(const std::string &input, std::vector<uint8_t> &out) {
    static const int8_t kDecTable[256] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55,
        56, 57, 58, 59, 60, 61, -1, -1, -1, 0,  -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
        13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, 32,
        33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    out.clear();
    out.reserve(input.size() * 3 / 4);
    uint32_t buffer = 0;
    int bitsCollected = 0;
    for (unsigned char c : input) {
        if (c == '=')
            break;
        int8_t value = kDecTable[c];
        if (value < 0)
            continue; 
        buffer = (buffer << 6) | static_cast<uint32_t>(value);
        bitsCollected += 6;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            out.push_back(static_cast<uint8_t>((buffer >> bitsCollected) & 0xFF));
        }
    }
    return !out.empty();
}

bool readVarint(const std::vector<uint8_t> &data, size_t &offset, uint64_t &out) {
    out = 0;
    int shift = 0;
    while (offset < data.size() && shift <= 63) {
        uint8_t byte = data[offset++];
        out |= static_cast<uint64_t>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    return false;
}

bool parseGuildFoldersProto(const std::string &base64, std::vector<ParsedFolder> &foldersOut,
                            std::vector<uint64_t> &positionsOut) {
    std::vector<uint8_t> bytes;
    if (!base64Decode(base64, bytes)) {
        return false;
    }
    std::vector<uint8_t> guildFoldersBytes;
    size_t idx = 0;
    while (idx < bytes.size()) {
        uint64_t key;
        if (!readVarint(bytes, idx, key))
            break;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (wire == 0) {
            uint64_t dummy;
            if (!readVarint(bytes, idx, dummy))
                break;
        } else if (wire == 1) {
            idx += 8;
        } else if (wire == 5) {
            idx += 4;
        } else if (wire == 2) {
            uint64_t len;
            if (!readVarint(bytes, idx, len) || idx + len > bytes.size())
                break;
            if (field == 14) {
                guildFoldersBytes.assign(bytes.begin() + idx, bytes.begin() + idx + static_cast<size_t>(len));
                break;
            }
            idx += static_cast<size_t>(len);
        } else {
            break;
        }
    }

    if (guildFoldersBytes.empty()) {
        return false;
    }

    size_t pos = 0;
    while (pos < guildFoldersBytes.size()) {
        uint64_t key;
        if (!readVarint(guildFoldersBytes, pos, key))
            break;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (wire == 0) {
            uint64_t dummy;
            if (!readVarint(guildFoldersBytes, pos, dummy))
                break;
        } else if (wire == 1) {
            if (pos + 8 > guildFoldersBytes.size())
                break;
            uint64_t value = 0;
            std::memcpy(&value, guildFoldersBytes.data() + pos, 8);
            positionsOut.push_back(value);
            pos += 8;
        } else if (wire == 5) {
            pos += 4;
        } else if (wire == 2) {
            uint64_t len;
            if (!readVarint(guildFoldersBytes, pos, len) || pos + len > guildFoldersBytes.size())
                break;
            const size_t fieldStart = pos;
            pos += static_cast<size_t>(len);
            if (field == 1) {
                ParsedFolder folder;
                size_t fp = fieldStart;
                const size_t fend = fieldStart + static_cast<size_t>(len);
                while (fp < fend) {
                    uint64_t fkey;
                    if (!readVarint(guildFoldersBytes, fp, fkey))
                        break;
                    const uint32_t ff = static_cast<uint32_t>(fkey >> 3);
                    const uint32_t fw = static_cast<uint32_t>(fkey & 7);
                    if (fw == 2 && ff == 1) {
                        uint64_t flen;
                        if (!readVarint(guildFoldersBytes, fp, flen) || fp + flen > fend)
                            break;
                        size_t sub = fp;
                        fp += static_cast<size_t>(flen);
                        while (sub + 8 <= fp) {
                            uint64_t gid = 0;
                            std::memcpy(&gid, guildFoldersBytes.data() + sub, 8);
                            folder.guildIds.push_back(std::to_string(gid));
                            sub += 8;
                        }
                    } else if (fw == 2 && ff == 2) {
                        uint64_t ilen;
                        if (!readVarint(guildFoldersBytes, fp, ilen) || fp + ilen > fend)
                            break;
                        size_t sub = fp;
                        fp += static_cast<size_t>(ilen);
                        uint64_t innerKey;
                        if (readVarint(guildFoldersBytes, sub, innerKey) && (innerKey & 7) == 0) {
                            uint64_t val;
                            if (readVarint(guildFoldersBytes, sub, val)) {
                                folder.id = static_cast<int64_t>(val);
                            }
                        }
                    } else if (fw == 2 && ff == 3) {
                        uint64_t nlen;
                        if (!readVarint(guildFoldersBytes, fp, nlen) || fp + nlen > fend)
                            break;
                        size_t sub = fp;
                        fp += static_cast<size_t>(nlen);
                        uint64_t innerKey;
                        if (readVarint(guildFoldersBytes, sub, innerKey) && (innerKey & 7) == 2) {
                            uint64_t slen;
                            if (readVarint(guildFoldersBytes, sub, slen) && sub + slen <= fp) {
                                folder.name.assign(reinterpret_cast<const char *>(guildFoldersBytes.data() + sub),
                                                   static_cast<size_t>(slen));
                            }
                        }
                    } else if (fw == 2 && ff == 4) {
                        uint64_t clen;
                        if (!readVarint(guildFoldersBytes, fp, clen) || fp + clen > fend)
                            break;
                        size_t sub = fp;
                        fp += static_cast<size_t>(clen);
                        uint64_t innerKey;
                        if (readVarint(guildFoldersBytes, sub, innerKey) && (innerKey & 7) == 0) {
                            uint64_t val;
                            if (readVarint(guildFoldersBytes, sub, val)) {
                                folder.color = static_cast<uint32_t>(val);
                                folder.hasColor = true;
                            }
                        }
                    } else {
                        if (fw == 0) {
                            uint64_t dummy;
                            if (!readVarint(guildFoldersBytes, fp, dummy))
                                break;
                        } else if (fw == 1) {
                            fp += 8;
                        } else if (fw == 5) {
                            fp += 4;
                        } else if (fw == 2) {
                            uint64_t skip;
                            if (!readVarint(guildFoldersBytes, fp, skip) || fp + skip > fend)
                                break;
                            fp += static_cast<size_t>(skip);
                        } else {
                            break;
                        }
                    }
                }
                foldersOut.push_back(std::move(folder));
            } else if (field == 2) {
                size_t sub = fieldStart;
                while (sub + 8 <= fieldStart + static_cast<size_t>(len)) {
                    uint64_t val = 0;
                    std::memcpy(&val, guildFoldersBytes.data() + sub, 8);
                    positionsOut.push_back(val);
                    sub += 8;
                }
            }
        } else {
            break;
        }
    }
    return true;
}

} // namespace ProtobufUtils
