#include "ui/EmojiManager.h"

#include "utils/Images.h"
#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/Fl_RGB_Image.H>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace EmojiManager {

namespace fs = std::filesystem;
using Json = nlohmann::json;

namespace {

struct EmojiRecord {
    std::string emoji;
    std::string name;
    std::string sheetUrl;
    int bgPosX = 0;
    int bgPosY = 0;
    int bgSizeW = 0;
    int bgSizeH = 0;
    int tileW = 0;
    int tileH = 0;
};

std::once_flag init_once;
std::mutex manager_mutex;

bool loaded = false;
fs::path root_path;
std::unordered_map<std::string, EmojiRecord> emoji_map;
std::vector<std::string> emoji_keys;

struct TrieNode {
    std::unordered_map<unsigned char, int> next;
    int terminalIndex = -1;
};
std::vector<TrieNode> emoji_trie;

std::unordered_map<std::string, std::unique_ptr<Fl_Image>> emoji_image_cache;

std::unordered_set<std::string> pending_atlas_urls;
std::mutex atlas_mutex;

std::string readFileToString(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool parsePxPair(const std::string &s, int &outA, int &outB) {
    auto parseOne = [](const std::string &v, size_t &i, int &out) -> bool {
        while (i < v.size() && v[i] == ' ') {
            ++i;
        }
        if (i >= v.size()) {
            return false;
        }
        int sign = 1;
        if (v[i] == '-') {
            sign = -1;
            ++i;
        } else if (v[i] == '+') {
            ++i;
        }
        if (i >= v.size() || v[i] < '0' || v[i] > '9') {
            return false;
        }
        int value = 0;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
            value = value * 10 + (v[i] - '0');
            ++i;
        }
        while (i < v.size() && v[i] != ' ') {
            ++i;
        }
        out = value * sign;
        return true;
    };

    size_t i = 0;
    int a = 0;
    int b = 0;
    if (!parseOne(s, i, a)) {
        return false;
    }
    if (!parseOne(s, i, b)) {
        return false;
    }
    outA = a;
    outB = b;
    return true;
}

std::unique_ptr<Fl_RGB_Image> copyRegionScaled(Fl_RGB_Image *src, int x, int y, int w, int h, int targetSize) {
    if (!src || w <= 0 || h <= 0 || targetSize <= 0) {
        return nullptr;
    }
    if (x < 0 || y < 0 || x + w > src->w() || y + h > src->h()) {
        return nullptr;
    }

    int depth = src->d();
    if (depth < 3) {
        return nullptr;
    }

    const unsigned char *srcData = reinterpret_cast<const unsigned char *>(src->data()[0]);
    if (!srcData) {
        return nullptr;
    }

    int srcLineSize = src->ld() ? src->ld() : src->w() * depth;
    std::vector<unsigned char> buffer(w * h * depth);

    for (int row = 0; row < h; ++row) {
        const unsigned char *srcRow = srcData + (y + row) * srcLineSize + x * depth;
        unsigned char *dstRow = buffer.data() + row * w * depth;
        std::memcpy(dstRow, srcRow, w * depth);
    }

    unsigned char *heapData = new unsigned char[buffer.size()];
    std::memcpy(heapData, buffer.data(), buffer.size());

    auto *region = new Fl_RGB_Image(heapData, w, h, depth);
    region->alloc_array = 1;

    Fl_Image *scaled = region->copy(targetSize, targetSize);
    delete region;
    if (!scaled) {
        return nullptr;
    }

    return std::unique_ptr<Fl_RGB_Image>(static_cast<Fl_RGB_Image *>(scaled));
}

std::string extractDiscordAssetHashPng(const std::string &url) {
    size_t start = url.rfind("assets/");
    if (start == std::string::npos) {
        return "";
    }
    start += std::string("assets/").size();
    size_t end = url.find(".png", start);
    if (end == std::string::npos || end <= start) {
        return "";
    }
    std::string hash = url.substr(start, end - start);
    for (char c : hash) {
        bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!ok) {
            return "";
        }
    }
    std::transform(hash.begin(), hash.end(), hash.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return hash;
}

constexpr const char *kDiscordAssetsBaseUrl = "https://discord.com/assets/";

bool isBaseYellowAtlasHash(const std::string &hash) { return hash == "9d6f7bad0b4786fd" || hash == "668ed2f8f314c3b7"; }

std::string discordAssetPngUrl(const char *hash) { return std::string(kDiscordAssetsBaseUrl) + hash + ".png"; }

std::string atlasUrlForFitzpatrick(uint32_t modifier) {
    switch (modifier) {
    case 0x1F3FB:
        return discordAssetPngUrl("42a77a907c6518a2");
    case 0x1F3FC:
        return discordAssetPngUrl("13d7c7712a8c345d");
    case 0x1F3FD:
        return discordAssetPngUrl("7ba098a1f59e8c25");
    case 0x1F3FE:
        return discordAssetPngUrl("553d87e518760843");
    case 0x1F3FF:
        return discordAssetPngUrl("bce9faa083202df0");
    default:
        return "";
    }
}

bool decodeNextUtf8Codepoint(const std::string &s, size_t &ioPos, uint32_t &outCp) {
    if (ioPos >= s.size()) {
        return false;
    }

    unsigned char lead = static_cast<unsigned char>(s[ioPos]);
    if (lead < 0x80) {
        outCp = lead;
        ioPos += 1;
        return true;
    }

    auto getCont = [&](size_t idx, unsigned char &out) -> bool {
        if (idx >= s.size()) {
            return false;
        }
        unsigned char c = static_cast<unsigned char>(s[idx]);
        if ((c & 0xC0) != 0x80) {
            return false;
        }
        out = c;
        return true;
    };

    if ((lead >> 5) == 0x6) {
        unsigned char c1 = 0;
        if (!getCont(ioPos + 1, c1)) {
            return false;
        }
        outCp = ((lead & 0x1F) << 6) | (c1 & 0x3F);
        ioPos += 2;
        return true;
    }
    if ((lead >> 4) == 0xE) {
        unsigned char c1 = 0;
        unsigned char c2 = 0;
        if (!getCont(ioPos + 1, c1) || !getCont(ioPos + 2, c2)) {
            return false;
        }
        outCp = ((lead & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
        ioPos += 3;
        return true;
    }
    if ((lead >> 3) == 0x1E) {
        unsigned char c1 = 0;
        unsigned char c2 = 0;
        unsigned char c3 = 0;
        if (!getCont(ioPos + 1, c1) || !getCont(ioPos + 2, c2) || !getCont(ioPos + 3, c3)) {
            return false;
        }
        outCp = ((lead & 0x07) << 18) | ((c1 & 0x3F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
        ioPos += 4;
        return true;
    }

    return false;
}

bool findFitzpatrickModifier(const std::string &emoji, uint32_t &outModifier, size_t &outBytePos, size_t &outByteLen) {
    size_t pos = 0;
    while (pos < emoji.size()) {
        uint32_t cp = 0;
        size_t prev = pos;
        if (!decodeNextUtf8Codepoint(emoji, pos, cp)) {
            pos = prev + 1;
            continue;
        }
        if (cp >= 0x1F3FB && cp <= 0x1F3FF) {
            outModifier = cp;
            outBytePos = prev;
            outByteLen = pos - prev;
            return true;
        }
    }
    return false;
}

fs::path findDefaultRoot() {
    fs::path bundled = fs::path("assets") / "emojis";
    if (fs::exists(bundled / "mappings.json")) {
        return bundled;
    }
    return {};
}

bool loadFromRootUnlocked(const fs::path &root) {
    fs::path jsonPath = root / "mappings.json";
    if (!fs::exists(jsonPath)) {
        return false;
    }

    try {
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            return false;
        }

        Json data;
        file >> data;

        if (!data.contains("items") || !data["items"].is_array()) {
            return false;
        }

        std::unordered_map<std::string, EmojiRecord> newMap;

        for (const auto &it : data["items"]) {
            if (!it.is_object()) {
                continue;
            }
            if (!it.contains("surrogates") || !it["surrogates"].is_string()) {
                continue;
            }

            EmojiRecord rec;
            rec.emoji = it["surrogates"].get<std::string>();
            if (rec.emoji.empty()) {
                continue;
            }

            if (it.contains("name") && it["name"].is_string()) {
                rec.name = it["name"].get<std::string>();
            }

            if (it.contains("sheetUrl") && it["sheetUrl"].is_string()) {
                rec.sheetUrl = it["sheetUrl"].get<std::string>();
            }

            if (it.contains("bgPos") && it["bgPos"].is_string()) {
                parsePxPair(it["bgPos"].get<std::string>(), rec.bgPosX, rec.bgPosY);
            }

            if (it.contains("bgSize") && it["bgSize"].is_string()) {
                parsePxPair(it["bgSize"].get<std::string>(), rec.bgSizeW, rec.bgSizeH);
            }

            if (it.contains("w") && it["w"].is_number()) {
                rec.tileW = it["w"].get<int>();
            }
            if (it.contains("h") && it["h"].is_number()) {
                rec.tileH = it["h"].get<int>();
            }

            std::string key = rec.emoji;
            newMap.emplace(key, std::move(rec));
        }

        std::vector<std::string> newKeys;
        newKeys.reserve(newMap.size());
        for (const auto &kv : newMap) {
            newKeys.push_back(kv.first);
        }

        std::vector<TrieNode> newTrie;
        newTrie.emplace_back();
        for (size_t idx = 0; idx < newKeys.size(); ++idx) {
            const std::string &emoji = newKeys[idx];
            int node = 0;
            for (unsigned char ch : emoji) {
                auto it = newTrie[node].next.find(ch);
                if (it == newTrie[node].next.end()) {
                    int nextIndex = static_cast<int>(newTrie.size());
                    newTrie[node].next.emplace(ch, nextIndex);
                    newTrie.emplace_back();
                    node = nextIndex;
                } else {
                    node = it->second;
                }
            }
            newTrie[node].terminalIndex = static_cast<int>(idx);
        }

        root_path = root;
        emoji_map = std::move(newMap);
        emoji_keys = std::move(newKeys);
        emoji_trie = std::move(newTrie);
        emoji_image_cache.clear();
        {
            std::scoped_lock lock(atlas_mutex);
            pending_atlas_urls.clear();
        }
        loaded = !emoji_map.empty();
        return loaded;
    } catch (const std::exception &e) {
        Logger::warn(std::string("EmojiManager failed to parse emoji map: ") + e.what());
        return false;
    }
}

void ensureInitialized() {
    std::call_once(init_once, []() {
        fs::path root = findDefaultRoot();
        if (root.empty()) {
            Logger::warn("EmojiManager: bundled emoji dataset not found at assets/emojis/mappings.json");
            return;
        }
        std::scoped_lock lock(manager_mutex);
        if (loadFromRootUnlocked(root)) {
            Logger::info("EmojiManager loaded unicode emoji dataset from: " + root_path.string());
        }
    });
}

} // namespace

bool initialize(const std::string &rootDir) {
    std::scoped_lock lock(manager_mutex);
    return loadFromRootUnlocked(fs::path(rootDir));
}

void initializeFromDefaultLocations() { ensureInitialized(); }

bool isAvailable() {
    ensureInitialized();
    std::scoped_lock lock(manager_mutex);
    return loaded;
}

std::string makeCacheKey(const std::string &emoji, int size) { return "unicode:" + emoji + "#" + std::to_string(size); }

bool hasEmoji(const std::string &emoji) {
    ensureInitialized();
    std::scoped_lock lock(manager_mutex);
    if (emoji_map.find(emoji) != emoji_map.end()) {
        return true;
    }

    uint32_t modifier = 0;
    size_t modPos = 0;
    size_t modLen = 0;
    if (!findFitzpatrickModifier(emoji, modifier, modPos, modLen)) {
        return false;
    }

    std::string base = emoji;
    base.erase(modPos, modLen);
    auto it = emoji_map.find(base);
    if (it == emoji_map.end()) {
        return false;
    }

    std::string atlasHash = extractDiscordAssetHashPng(it->second.sheetUrl);
    return isBaseYellowAtlasHash(atlasHash) && !atlasUrlForFitzpatrick(modifier).empty();
}

bool tryMatch(const std::string &text, size_t pos, size_t &outLength, std::string &outEmoji) {
    ensureInitialized();
    std::scoped_lock lock(manager_mutex);
    if (!loaded || pos >= text.size()) {
        return false;
    }

    int node = 0;
    int lastTerminal = -1;
    size_t lastLen = 0;
    for (size_t i = pos; i < text.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        auto it = emoji_trie[node].next.find(ch);
        if (it == emoji_trie[node].next.end()) {
            break;
        }
        node = it->second;
        if (emoji_trie[node].terminalIndex >= 0) {
            lastTerminal = emoji_trie[node].terminalIndex;
            lastLen = i - pos + 1;
        }
    }

    if (lastTerminal >= 0 && static_cast<size_t>(lastTerminal) < emoji_keys.size()) {
        const std::string &baseEmoji = emoji_keys[static_cast<size_t>(lastTerminal)];
        outEmoji = baseEmoji;
        outLength = lastLen;

        auto it = emoji_map.find(baseEmoji);
        if (it == emoji_map.end()) {
            return true;
        }

        std::string atlasHash = extractDiscordAssetHashPng(it->second.sheetUrl);
        if (!isBaseYellowAtlasHash(atlasHash)) {
            return true;
        }

        size_t nextPos = pos + lastLen;
        if (nextPos >= text.size()) {
            return true;
        }

        size_t scanPos = nextPos;
        uint32_t cp = 0;
        if (!decodeNextUtf8Codepoint(text, scanPos, cp)) {
            return true;
        }
        if (cp < 0x1F3FB || cp > 0x1F3FF) {
            return true;
        }

        if (atlasUrlForFitzpatrick(cp).empty()) {
            return true;
        }

        outLength = scanPos - pos;
        outEmoji = text.substr(pos, outLength);
        return true;
    }

    return false;
}

Fl_Image *loadEmoji(const std::string &emoji, int size) {
    if (emoji.empty() || size <= 0) {
        return nullptr;
    }

    ensureInitialized();
    std::string cacheKey;
    EmojiRecord rec;
    bool hasRecord = false;

    {
        std::scoped_lock lock(manager_mutex);
        if (!loaded) {
            return nullptr;
        }

        cacheKey = makeCacheKey(emoji, size);
        auto cached = emoji_image_cache.find(cacheKey);
        if (cached != emoji_image_cache.end()) {
            return cached->second.get();
        }

        auto it = emoji_map.find(emoji);
        if (it == emoji_map.end()) {
            uint32_t modifier = 0;
            size_t modPos = 0;
            size_t modLen = 0;
            if (!findFitzpatrickModifier(emoji, modifier, modPos, modLen)) {
                return nullptr;
            }
            std::string base = emoji;
            base.erase(modPos, modLen);
            auto baseIt = emoji_map.find(base);
            if (baseIt == emoji_map.end()) {
                return nullptr;
            }

            std::string atlasHash = extractDiscordAssetHashPng(baseIt->second.sheetUrl);
            if (!isBaseYellowAtlasHash(atlasHash)) {
                return nullptr;
            }

            std::string toneUrl = atlasUrlForFitzpatrick(modifier);
            if (toneUrl.empty()) {
                return nullptr;
            }

            rec = baseIt->second;
            rec.sheetUrl = toneUrl;
            hasRecord = true;
        } else {
            rec = it->second;
            hasRecord = true;
        }
    }

    if (!hasRecord) {
        return nullptr;
    }

    bool atlasAvailable = !rec.sheetUrl.empty() && rec.bgSizeW > 0 && rec.bgSizeH > 0 && rec.tileW > 0 && rec.tileH > 0;
    if (atlasAvailable) {
        Fl_RGB_Image *atlas = Images::getCachedImage(rec.sheetUrl);
        if (!atlas) {
            bool shouldRequest = false;
            {
                std::scoped_lock lock(atlas_mutex);
                if (pending_atlas_urls.find(rec.sheetUrl) == pending_atlas_urls.end()) {
                    pending_atlas_urls.insert(rec.sheetUrl);
                    shouldRequest = true;
                }
            }

            if (shouldRequest) {
                Images::loadImageAsync(rec.sheetUrl, [sheetUrl = rec.sheetUrl](Fl_RGB_Image *) {
                    {
                        std::scoped_lock lock(atlas_mutex);
                        pending_atlas_urls.erase(sheetUrl);
                    }
                    Fl::redraw();
                });
            }
            return nullptr;
        }

        if (atlas->w() > 0 && atlas->h() > 0) {
            double scaleX = static_cast<double>(atlas->w()) / static_cast<double>(rec.bgSizeW);
            double scaleY = static_cast<double>(atlas->h()) / static_cast<double>(rec.bgSizeH);

            int srcX = static_cast<int>(std::lround((-rec.bgPosX) * scaleX));
            int srcY = static_cast<int>(std::lround((-rec.bgPosY) * scaleY));
            int srcW = static_cast<int>(std::lround(rec.tileW * scaleX));
            int srcH = static_cast<int>(std::lround(rec.tileH * scaleY));

            auto rgb = copyRegionScaled(atlas, srcX, srcY, srcW, srcH, size);
            if (rgb) {
                Fl_Image *result = rgb.get();
                std::scoped_lock lock(manager_mutex);
                emoji_image_cache[cacheKey] = std::move(rgb);
                return result;
            }
        }
    }

    return nullptr;
}

void pruneCache(const std::unordered_set<std::string> &keepKeys) {
    ensureInitialized();
    std::scoped_lock lock(manager_mutex);
    if (!loaded) {
        return;
    }

    for (auto it = emoji_image_cache.begin(); it != emoji_image_cache.end();) {
        if (keepKeys.find(it->first) == keepKeys.end()) {
            it = emoji_image_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void clearCache() {
    std::scoped_lock lock(manager_mutex);
    emoji_image_cache.clear();
    {
        std::scoped_lock lock(atlas_mutex);
        pending_atlas_urls.clear();
    }
}

} // namespace EmojiManager
