#include "utils/Uuid.h"

#include <iomanip>
#include <random>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <rpc.h>
#include <windows.h>
#pragma comment(lib, "rpcrt4.lib")
#endif

namespace UuidHelper {

std::string generate() {
#ifdef _WIN32
    UUID uuid;
    if (UuidCreate(&uuid) != RPC_S_OK) {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dis;

        std::stringstream ss;
        ss << std::hex << std::setfill('0');
        ss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
        ss << std::setw(4) << (dis(gen) & 0xFFFF) << "-";
        ss << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-";
        ss << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-";
        ss << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);
        return ss.str();
    }

    unsigned char *str;
    if (UuidToStringA(&uuid, &str) != RPC_S_OK) {
        return "";
    }

    std::string result(reinterpret_cast<char *>(str));
    RpcStringFreeA(&str);
    return result;
#else
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << (dis(gen) & 0xFFFF) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x0FFF) | 0x4000) << "-";
    ss << std::setw(4) << ((dis(gen) & 0x3FFF) | 0x8000) << "-";
    ss << std::setw(12) << (dis(gen) & 0xFFFFFFFFFFFF);
    return ss.str();
#endif
}

} // namespace UuidHelper
