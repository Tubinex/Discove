#include "ui/components/DMSidebar.h"

#include "state/AppState.h"
#include "net/APIClient.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/AnimationManager.h"
#include "ui/GifAnimation.h"
#include "ui/RoundedWidget.h"
#include "ui/Theme.h"
#include "data/Database.h"
#include "models/Message.h"
#include "models/User.h"
#include "utils/Fonts.h"
#include "utils/Images.h"
#include "utils/Logger.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace {
std::string ellipsizeText(const std::string &text, int maxWidth) {
    if (maxWidth <= 0) {
        return "";
    }

    int fullWidth = static_cast<int>(fl_width(text.c_str()));
    if (fullWidth <= maxWidth) {
        return text;
    }

    const char *ellipsis = "...";
    int ellipsisWidth = static_cast<int>(fl_width(ellipsis));
    if (ellipsisWidth >= maxWidth) {
        return "";
    }

    std::string truncated = text;
    while (!truncated.empty()) {
        truncated.pop_back();
        int width = static_cast<int>(fl_width(truncated.c_str()));
        if (width <= maxWidth - ellipsisWidth) {
            break;
        }
    }

    return truncated.empty() ? std::string(ellipsis) : truncated + ellipsis;
}

Fl_Color statusToColor(const std::string &status) {
    if (status == "online") {
        return ThemeColors::STATUS_ONLINE;
    }
    if (status == "idle") {
        return ThemeColors::STATUS_IDLE;
    }
    if (status == "dnd") {
        return ThemeColors::STATUS_DND;
    }
    return ThemeColors::STATUS_OFFLINE;
}

char initialFromLabel(const std::string &label) {
    for (char c : label) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
    }
    return '?';
}

std::string buildAvatarKey(const std::string &url, int size) { return url + "#" + std::to_string(size); }

bool isGifUrl(const std::string &url) {
    size_t pos = url.rfind(".gif");
    if (pos == std::string::npos) {
        return false;
    }
    size_t end = pos + 4;
    return end == url.size() || url[end] == '?' || url[end] == '#';
}

std::string makeStaticAvatarUrl(const std::string &url) {
    if (!isGifUrl(url)) {
        return url;
    }

    std::string staticUrl = url;
    size_t pos = staticUrl.rfind(".gif");
    if (pos != std::string::npos) {
        staticUrl.replace(pos, 4, ".png");
    }
    return staticUrl;
}

struct DMChannelSignature {
    std::string id;
    std::string name;
    std::string icon;
    std::string lastMessageId;
    int type = 0;
    std::vector<std::string> recipientIds;
    std::vector<std::string> recipients;

    bool operator==(const DMChannelSignature &other) const {
        return id == other.id && name == other.name && icon == other.icon && lastMessageId == other.lastMessageId &&
               type == other.type && recipientIds == other.recipientIds && recipients == other.recipients;
    }

    bool operator!=(const DMChannelSignature &other) const { return !(*this == other); }
};

std::vector<DMChannelSignature> buildDMChannelSignatures(const AppState &state) {
    std::vector<DMChannelSignature> sigs;
    sigs.reserve(state.privateChannels.size());

    for (const auto &channel : state.privateChannels) {
        if (!channel) {
            continue;
        }

        DMChannelSignature sig;
        sig.id = channel->id;
        sig.name = channel->name.value_or("");
        sig.icon = channel->icon.value_or("");
        sig.lastMessageId = channel->lastMessageId.value_or("");
        sig.type = static_cast<int>(channel->type);

        sig.recipientIds = channel->recipientIds;
        std::sort(sig.recipientIds.begin(), sig.recipientIds.end());

        sig.recipients.reserve(channel->recipients.size());
        for (const auto &user : channel->recipients) {
            std::string entry;
            entry.reserve(96);
            entry += user.id;
            entry += "|";
            entry += user.username;
            entry += "|";
            entry += user.discriminator;
            entry += "|";
            entry += user.globalName.value_or("");
            entry += "|";
            entry += user.avatar.value_or("");
            sig.recipients.push_back(std::move(entry));
        }
        std::sort(sig.recipients.begin(), sig.recipients.end());

        sigs.push_back(std::move(sig));
    }
    return sigs;
}

bool statusesEqual(const std::unordered_map<std::string, std::string> &a, const std::unordered_map<std::string, std::string> &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (const auto &kv : a) {
        auto it = b.find(kv.first);
        if (it == b.end() || it->second != kv.second) {
            return false;
        }
    }
    return true;
}

bool upsertUser(std::unordered_map<std::string, User> &users, const User &user) {
    auto it = users.find(user.id);
    if (it == users.end()) {
        users.emplace(user.id, user);
        return true;
    }

    const User &existing = it->second;
    if (existing.username == user.username && existing.discriminator == user.discriminator && existing.globalName == user.globalName &&
        existing.avatar == user.avatar) {
        return false;
    }

    it->second = user;
    return true;
}

std::string buildGroupDisplayName(const DMChannel &channel) {
    std::vector<std::string> names;
    names.reserve(channel.recipients.size());
    for (const auto &user : channel.recipients) {
        std::string name = user.getDisplayName();
        if (!name.empty()) {
            names.push_back(std::move(name));
        }
    }

    if (names.empty()) {
        if (!channel.recipientIds.empty()) {
            return "Group DM";
        }
        return "Unknown Group";
    }

    if (names.size() == 1) {
        return names[0];
    }

    if (names.size() == 2) {
        return names[0] + ", " + names[1];
    }

    return names[0] + ", " + names[1] + " +" + std::to_string(names.size() - 2);
}
} // namespace

DMSidebar::DMSidebar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_NO_BOX);
    clip_children(1);
    end();

    m_isAlive = std::make_shared<bool>(true);

    m_storeListenerId = Store::get().subscribe<std::vector<DMChannelSignature>>(
        [alive = m_isAlive](const AppState &state) {
            if (!alive || !*alive) {
                return std::vector<DMChannelSignature>{};
            }
            return buildDMChannelSignatures(state);
        },
        [this, alive = m_isAlive](const std::vector<DMChannelSignature> &) {
            if (!alive || !*alive) {
                return;
            }
            loadDMsFromState();
            redraw();
        },
        [](const std::vector<DMChannelSignature> &a, const std::vector<DMChannelSignature> &b) {
            if (a.size() != b.size()) {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i) {
                if (a[i] != b[i]) {
                    return false;
                }
            }
            return true;
        },
        true);

    m_statusListenerId = Store::get().subscribe<std::unordered_map<std::string, std::string>>(
        [](const AppState &state) { return state.userStatuses; },
        [this, alive = m_isAlive](const std::unordered_map<std::string, std::string> &statuses) {
            if (!alive || !*alive) {
                return;
            }
            updateStatuses(statuses);
            redraw();
        },
        statusesEqual, true);

    m_userListenerId = Store::get().subscribe<uint64_t>(
        [](const AppState &state) { return state.usersRevision; },
        [this, alive = m_isAlive](uint64_t) {
            if (!alive || !*alive) {
                return;
            }
            loadDMsFromState();
            redraw();
        },
        std::equal_to<uint64_t>{}, true);
}

DMSidebar::~DMSidebar() {
    if (m_isAlive) {
        *m_isAlive = false;
    }
    setHoveredAvatarKey("");
    for (auto &entry : m_avatarGifCache) {
        stopAvatarAnimation(entry.first);
    }
    m_avatarGifCache.clear();
    m_avatarGifPending.clear();
    if (m_storeListenerId) {
        Store::get().unsubscribe(m_storeListenerId);
    }
    if (m_statusListenerId) {
        Store::get().unsubscribe(m_statusListenerId);
    }
    if (m_userListenerId) {
        Store::get().unsubscribe(m_userListenerId);
    }
}

void DMSidebar::setScrollOffset(int offset) {
    m_scrollOffset = offset;

    int contentHeight = calculateContentHeight();
    int maxScrollOffset = std::max(0, contentHeight - h());

    if (m_scrollOffset < 0) {
        m_scrollOffset = 0;
    } else if (m_scrollOffset > maxScrollOffset) {
        m_scrollOffset = maxScrollOffset;
    }

    redraw();
}

void DMSidebar::draw() {
    if (!damage() || damage() == FL_DAMAGE_CHILD) {
        return;
    }

    fl_push_clip(x(), y(), w(), h());

    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), h());

    drawHeader();

    fl_push_clip(x(), y() + HEADER_HEIGHT, w(), h() - HEADER_HEIGHT);

    constexpr int kRenderPadding = 200;
    int viewTop = y() + HEADER_HEIGHT;
    int viewBottom = y() + h();
    int renderTop = viewTop - kRenderPadding;
    int renderBottom = viewBottom + kRenderPadding;

    const int listTop = y() + HEADER_HEIGHT + LIST_PADDING;
    const int scrollStartY = listTop - m_scrollOffset;

    int firstIndex = 0;
    if (renderTop > scrollStartY) {
        firstIndex = (renderTop - scrollStartY) / DM_HEIGHT;
    }

    int lastIndex = -1;
    if (!m_dms.empty() && renderBottom > scrollStartY) {
        lastIndex = (renderBottom - scrollStartY) / DM_HEIGHT;
        lastIndex = std::min(lastIndex, static_cast<int>(m_dms.size()) - 1);
    }

    std::unordered_set<std::string> keepAvatarKeys;
    std::unordered_set<std::string> keepGifKeys;

    if (firstIndex < 0)
        firstIndex = 0;
    if (lastIndex >= 0) {
        for (int i = firstIndex; i <= lastIndex; ++i) {
            auto &dm = m_dms[static_cast<size_t>(i)];
            int itemY = scrollStartY + i * DM_HEIGHT;
            dm.yPos = itemY;

            bool selected = (dm.id == m_selectedDMId);
            bool hovered = (m_hoveredDMIndex == i);

            drawDM(dm, itemY, selected, hovered);

            if (!dm.secondaryAvatarUrl.empty()) {
                if (!dm.avatarUrl.empty()) {
                    keepAvatarKeys.insert(buildAvatarKey(dm.avatarUrl, 20));
                }
                if (!dm.secondaryAvatarUrl.empty()) {
                    keepAvatarKeys.insert(buildAvatarKey(dm.secondaryAvatarUrl, 20));
                }
                if (!dm.animatedAvatarUrl.empty()) {
                    keepGifKeys.insert(buildAvatarKey(dm.animatedAvatarUrl, 20));
                }
                if (!dm.secondaryAnimatedAvatarUrl.empty()) {
                    keepGifKeys.insert(buildAvatarKey(dm.secondaryAnimatedAvatarUrl, 20));
                }
            } else {
                if (!dm.avatarUrl.empty()) {
                    keepAvatarKeys.insert(buildAvatarKey(dm.avatarUrl, 32));
                }
                if (!dm.animatedAvatarUrl.empty()) {
                    keepGifKeys.insert(buildAvatarKey(dm.animatedAvatarUrl, 32));
                }
            }
        }
    }

    if (!m_hoveredAvatarKey.empty()) {
        keepGifKeys.insert(m_hoveredAvatarKey);
    }

    for (auto it = m_avatarCache.begin(); it != m_avatarCache.end();) {
        if (keepAvatarKeys.find(it->first) == keepAvatarKeys.end()) {
            it = m_avatarCache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_avatarGifCache.begin(); it != m_avatarGifCache.end();) {
        if (keepGifKeys.find(it->first) == keepGifKeys.end()) {
            stopAvatarAnimation(it->first);
            it = m_avatarGifCache.erase(it);
        } else {
            ++it;
        }
    }

    fl_pop_clip();

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x(), y(), x(), y() + h());

    fl_pop_clip();
}

void DMSidebar::drawHeader() {
    fl_color(ThemeColors::BG_SECONDARY);
    fl_rectf(x(), y(), w(), HEADER_HEIGHT);

    const int fontSize = LayoutConstants::kGuildHeaderFontSize;
    fl_color(ThemeColors::TEXT_NORMAL);
    fl_font(FontLoader::Fonts::INTER_SEMIBOLD, fontSize);

    const int leftMargin = 16;
    const int topMargin = 16;
    int textY = y() + topMargin + fontSize;
    int maxTextWidth = w() - leftMargin - 32;
    std::string label = ellipsizeText("Direct Messages", maxTextWidth);
    fl_draw(label.c_str(), x() + leftMargin, textY);

    const int iconSize = LayoutConstants::kDropdownIconFontSize;
    auto *icon = IconManager::load_recolored_icon("chevron_down", iconSize, ThemeColors::TEXT_NORMAL);
    if (icon) {
        int iconX = x() + w() - 24;
        int iconY = y() + topMargin + (fontSize - iconSize) / 2;
        icon->draw(iconX, iconY);
    }

    fl_color(ThemeColors::BG_TERTIARY);
    fl_line(x() + 1, y() + HEADER_HEIGHT - 1, x() + w(), y() + HEADER_HEIGHT - 1);
}

void DMSidebar::drawDM(const DMItem &dm, int yPos, bool selected, bool hovered) {
    const int leftMargin = 8;
    const int rightMargin = 8;
    const int itemX = x() + leftMargin;
    const int itemW = w() - leftMargin - rightMargin;
    const int itemY = yPos + 2;
    const int itemH = DM_HEIGHT - 4;
    const int radius = LayoutConstants::kChannelItemCornerRadius;

    if (selected || hovered) {
        Fl_Color bgColor = selected ? ThemeColors::BG_MODIFIER_SELECTED : ThemeColors::BG_MODIFIER_HOVER;
        RoundedStyle::drawRoundedRect(itemX, itemY, itemW, itemH, radius, radius, radius, radius, bgColor);
    }

    const int avatarSize = 32;
    const int avatarX = itemX + LayoutConstants::kChannelIconPadding;
    const int avatarY = itemY + (itemH - avatarSize) / 2;

    auto drawPlaceholderInitial = [&](int x, int y, int size, const std::string &label) {
        char initial = initialFromLabel(label);
        std::string initialStr(1, initial);
        fl_color(ThemeColors::TEXT_NORMAL);
        fl_font(FontLoader::Fonts::INTER_BOLD, 14);
        int textW = static_cast<int>(fl_width(initialStr.c_str()));
        int textH = fl_height();
        fl_draw(initialStr.c_str(), x + (size - textW) / 2, y + (size + textH) / 2 - fl_descent());
    };

    auto drawAvatar = [&](int x, int y, int size, const std::string &url, const std::string &gifUrl,
                          const std::string &label, bool animated) {
        fl_color(ThemeColors::BG_TERTIARY);
        fl_pie(x, y, size, size, 0, 360);

        if (animated && !gifUrl.empty()) {
            ensureAnimatedAvatar(gifUrl, size);
            if (auto *frame = getAnimatedAvatarFrame(gifUrl, size)) {
                frame->draw(x, y);
                return;
            }
        }

        if (!url.empty()) {
            requestCircularAvatar(url, size);
            if (auto *img = getCircularAvatar(url, size)) {
                img->draw(x, y);
                return;
            }
        }

        drawPlaceholderInitial(x, y, size, label);
    };

    if (!dm.secondaryAvatarUrl.empty()) {
        const int smallSize = 20;
        const int border = 2;
        const int firstX = avatarX;
        const int firstY = avatarY + (avatarSize - smallSize);
        const int secondX = avatarX + (avatarSize - smallSize);
        const int secondY = avatarY;

        Fl_Color avatarRing = ThemeColors::BG_SECONDARY;
        if (selected) {
            avatarRing = ThemeColors::BG_MODIFIER_SELECTED;
        } else if (hovered) {
            avatarRing = ThemeColors::BG_MODIFIER_HOVER;
        }

        fl_color(avatarRing);
        fl_pie(firstX - border, firstY - border, smallSize + border * 2, smallSize + border * 2, 0, 360);
        drawAvatar(firstX, firstY, smallSize, dm.avatarUrl, dm.animatedAvatarUrl, dm.avatarLabel, hovered);

        fl_color(avatarRing);
        fl_pie(secondX - border, secondY - border, smallSize + border * 2, smallSize + border * 2, 0, 360);
        drawAvatar(secondX, secondY, smallSize, dm.secondaryAvatarUrl, dm.secondaryAnimatedAvatarUrl,
                   dm.secondaryAvatarLabel, hovered);
    } else {
        drawAvatar(avatarX, avatarY, avatarSize, dm.avatarUrl, dm.animatedAvatarUrl, dm.avatarLabel, hovered);
    }

    Fl_Color textColor = (selected || hovered) ? ThemeColors::TEXT_NORMAL : ThemeColors::TEXT_MUTED;
    fl_color(textColor);
    fl_font(FontLoader::Fonts::INTER_MEDIUM, 16);
    int textX = avatarX + avatarSize + 12;
    int textY = itemY + (itemH / 2) + 6;
    int maxTextWidth = itemX + itemW - 12 - textX;
    std::string label = ellipsizeText(dm.displayName, maxTextWidth);
    fl_draw(label.c_str(), textX, textY);

    if (dm.showStatus) {
        int statusSize = 10;
        int statusX = avatarX + avatarSize - statusSize + 2;
        int statusY = avatarY + avatarSize - statusSize + 2;

        fl_color(statusToColor(dm.status));
        fl_pie(statusX, statusY, statusSize, statusSize, 0, 360);

        Fl_Color ringColor = ThemeColors::BG_SECONDARY;
        if (selected) {
            ringColor = ThemeColors::BG_MODIFIER_SELECTED;
        } else if (hovered) {
            ringColor = ThemeColors::BG_MODIFIER_HOVER;
        }
        fl_color(ringColor);
        fl_arc(statusX, statusY, statusSize, statusSize, 0, 360);
    }
}

int DMSidebar::handle(int event) {
    switch (event) {
    case FL_PUSH: {
        if (Fl::event_button() == FL_LEFT_MOUSE) {
            int mx = Fl::event_x();
            int my = Fl::event_y();
            int dmIndex = getDMAt(mx, my);

            if (dmIndex >= 0 && dmIndex < static_cast<int>(m_dms.size())) {
                if (m_onDMSelected) {
                    m_onDMSelected(m_dms[dmIndex].id);
                }
                return 1;
            }
        }
        break;
    }

    case FL_MOVE: {
        int mx = Fl::event_x();
        int my = Fl::event_y();
        int newHovered = getDMAt(mx, my);

        if (newHovered != m_hoveredDMIndex) {
            m_hoveredDMIndex = newHovered;
            if (m_hoveredDMIndex >= 0 && m_hoveredDMIndex < static_cast<int>(m_dms.size())) {
                const auto &dm = m_dms[static_cast<size_t>(m_hoveredDMIndex)];
                if (!dm.animatedAvatarUrl.empty() && dm.secondaryAvatarUrl.empty()) {
                    setHoveredAvatarKey(buildAvatarKey(dm.animatedAvatarUrl, 32));
                } else {
                    setHoveredAvatarKey("");
                }
            } else {
                setHoveredAvatarKey("");
            }
            redraw();
        }
        return 1;
    }

    case FL_LEAVE:
        if (m_hoveredDMIndex != -1) {
            m_hoveredDMIndex = -1;
            setHoveredAvatarKey("");
            redraw();
        }
        return 1;

    case FL_MOUSEWHEEL: {
        int dy = Fl::event_dy();
        m_scrollOffset += dy * 20;

        int contentHeight = calculateContentHeight();
        int maxScrollOffset = std::max(0, contentHeight - h());

        if (m_scrollOffset < 0)
            m_scrollOffset = 0;
        if (m_scrollOffset > maxScrollOffset)
            m_scrollOffset = maxScrollOffset;

        redraw();
        return 1;
    }
    }

    return Fl_Group::handle(event);
}

void DMSidebar::setSelectedDM(const std::string &dmId) {
    if (m_selectedDMId != dmId) {
        m_selectedDMId = dmId;
        redraw();
    }
}

int DMSidebar::getDMAt(int mx, int my) const {
    if (mx < x() || mx > x() + w()) {
        return -1;
    }

    const int listTop = y() + HEADER_HEIGHT + LIST_PADDING;
    if (my < listTop || my > y() + h()) {
        return -1;
    }

    int contentY = my - listTop + m_scrollOffset;
    if (contentY < 0) {
        return -1;
    }

    int index = contentY / DM_HEIGHT;
    if (index < 0 || index >= static_cast<int>(m_dms.size())) {
        return -1;
    }

    return index;
}

int DMSidebar::calculateContentHeight() const {
    int totalHeight = HEADER_HEIGHT;
    totalHeight += LIST_PADDING;
    totalHeight += static_cast<int>(m_dms.size()) * DM_HEIGHT;
    totalHeight += LIST_PADDING;
    return totalHeight;
}

void DMSidebar::loadDMsFromState() {
    m_dms.clear();
    m_hoveredDMIndex = -1;

    AppState state = Store::get().snapshot();
    std::unordered_map<std::string, User> discoveredUsers;

    auto findUser = [&](const std::string &userId) -> const User * {
        auto it = state.usersById.find(userId);
        if (it != state.usersById.end()) {
            return &it->second;
        }
        auto dt = discoveredUsers.find(userId);
        if (dt != discoveredUsers.end()) {
            return &dt->second;
        }
        return nullptr;
    };

    auto tryDiscoverUserFromDb = [&](const std::string &channelId, const std::string &userId) {
        if (userId.empty() || findUser(userId) != nullptr) {
            return;
        }

        auto messages = Data::Database::get().getChannelMessages(channelId, 50);
        for (const auto &msg : messages) {
            if (msg.authorId != userId) {
                continue;
            }
            if (msg.authorUsername.empty()) {
                continue;
            }

            User user;
            user.id = msg.authorId;
            user.username = msg.authorUsername;
            user.discriminator = msg.authorDiscriminator.empty() ? "0" : msg.authorDiscriminator;
            if (!msg.authorGlobalName.empty()) {
                user.globalName = msg.authorGlobalName;
            }
            if (!msg.authorAvatarHash.empty()) {
                user.avatar = msg.authorAvatarHash;
            }

            discoveredUsers[userId] = std::move(user);
            break;
        }
    };

    auto resolveDisplayName = [&](const std::string &userId) -> std::string {
        if (const User *user = findUser(userId)) {
            return user->getDisplayName();
        }
        return "";
    };

    auto resolveAvatarUrl = [&](const std::string &userId) -> std::string {
        if (const User *user = findUser(userId)) {
            return user->getAvatarUrl(64);
        }
        return "";
    };

    for (const auto &channel : state.privateChannels) {
        if (!channel)
            continue;

        DMItem item;
        item.id = channel->id;

        const bool isGroup = channel->type == ChannelType::GROUP_DM || channel->recipientIds.size() > 1;
        std::string channelIconUrl = isGroup ? channel->getIconUrl(64) : "";
        std::string channelIconStaticUrl = makeStaticAvatarUrl(channelIconUrl);
        std::string channelIconAnimatedUrl = isGifUrl(channelIconUrl) ? channelIconUrl : "";

        if (isGroup) {
            if (channel->name.has_value() && !channel->name->empty()) {
                item.displayName = *channel->name;
            } else {
                if (!channel->recipients.empty()) {
                    item.displayName = buildGroupDisplayName(*channel);
                } else {
                    std::vector<std::string> names;
                    for (const auto &id : channel->recipientIds) {
                        tryDiscoverUserFromDb(channel->id, id);
                        std::string name = resolveDisplayName(id);
                        if (!name.empty()) {
                            names.push_back(std::move(name));
                        }
                        if (names.size() >= 3) {
                            break;
                        }
                    }

                    if (names.empty()) {
                        item.displayName = "Group DM";
                    } else if (names.size() == 1) {
                        item.displayName = names[0];
                    } else if (names.size() == 2) {
                        item.displayName = names[0] + ", " + names[1];
                    } else {
                        item.displayName = names[0] + ", " + names[1] + " +" + std::to_string(names.size() - 2);
                    }
                }
            }

            item.showStatus = false;

            if (!channelIconUrl.empty()) {
                item.avatarUrl = channelIconStaticUrl;
                item.animatedAvatarUrl = channelIconAnimatedUrl;
                item.avatarLabel = item.displayName;
            } else if (!channel->recipients.empty() || !channel->recipientIds.empty()) {
                std::vector<User> users;
                users.reserve(2);

                for (const auto &u : channel->recipients) {
                    users.push_back(u);
                    if (users.size() >= 2) {
                        break;
                    }
                }

                for (const auto &id : channel->recipientIds) {
                    if (users.size() >= 2) {
                        break;
                    }
                    if (std::any_of(users.begin(), users.end(), [&](const User &u) { return u.id == id; })) {
                        continue;
                    }

                    tryDiscoverUserFromDb(channel->id, id);
                    if (const User *found = findUser(id)) {
                        users.push_back(*found);
                    }
                }

                if (!users.empty()) {
                    std::string avatarUrl = users[0].getAvatarUrl(64);
                    item.avatarUrl = makeStaticAvatarUrl(avatarUrl);
                    item.animatedAvatarUrl = isGifUrl(avatarUrl) ? avatarUrl : "";
                    item.avatarLabel = users[0].getDisplayName();
                } else {
                    item.avatarLabel = item.displayName;
                }

                if (users.size() > 1) {
                    std::string avatar2Url = users[1].getAvatarUrl(64);
                    item.secondaryAvatarUrl = makeStaticAvatarUrl(avatar2Url);
                    item.secondaryAnimatedAvatarUrl = isGifUrl(avatar2Url) ? avatar2Url : "";
                    item.secondaryAvatarLabel = users[1].getDisplayName();
                }
            } else {
                item.avatarLabel = item.displayName;
            }
        } else {
            if (!channel->recipients.empty()) {
                const User &user = channel->recipients[0];
                item.displayName = user.getDisplayName();
                item.recipientId = user.id;
                std::string avatarUrl = user.getAvatarUrl(64);
                item.avatarUrl = makeStaticAvatarUrl(avatarUrl);
                item.animatedAvatarUrl = isGifUrl(avatarUrl) ? avatarUrl : "";
                item.avatarLabel = item.displayName;
            } else if (!channel->recipientIds.empty()) {
                item.recipientId = channel->recipientIds[0];
                tryDiscoverUserFromDb(channel->id, item.recipientId);

                if (std::string name = resolveDisplayName(item.recipientId); !name.empty()) {
                    item.displayName = std::move(name);
                } else {
                    enqueueUserResolve(item.recipientId);
                    item.displayName = "User " + item.recipientId.substr(0, 8);
                }

                if (std::string avatarUrl = resolveAvatarUrl(item.recipientId); !avatarUrl.empty()) {
                    item.avatarUrl = makeStaticAvatarUrl(avatarUrl);
                    item.animatedAvatarUrl = isGifUrl(avatarUrl) ? avatarUrl : "";
                }
                item.avatarLabel = item.displayName;
            } else {
                item.displayName = "Unknown DM";
                item.avatarLabel = item.displayName;
            }

            item.showStatus = !item.recipientId.empty();
        }

        m_dms.push_back(item);
    }

    updateStatuses(state.userStatuses);

    if (!discoveredUsers.empty()) {
        Store::get().update([users = std::move(discoveredUsers)](AppState &state) mutable {
            bool changed = false;
            for (auto &entry : users) {
                changed |= upsertUser(state.usersById, entry.second);
            }
            if (changed) {
                state.usersRevision++;
            }
        });
    }
}

void DMSidebar::updateStatuses(const std::unordered_map<std::string, std::string> &statuses) {
    for (auto &dm : m_dms) {
        if (!dm.showStatus || dm.recipientId.empty()) {
            dm.status = "offline";
            continue;
        }

        auto it = statuses.find(dm.recipientId);
        if (it != statuses.end()) {
            dm.status = it->second;
        } else {
            dm.status = "offline";
        }
    }
}

Fl_RGB_Image *DMSidebar::getCircularAvatar(const std::string &url, int size) {
    if (url.empty()) {
        return nullptr;
    }

    std::string key = buildAvatarKey(url, size);
    auto it = m_avatarCache.find(key);
    if (it != m_avatarCache.end()) {
        return it->second.get();
    }

    if (Fl_RGB_Image *cached = Images::getCachedImage(url)) {
        if (Fl_RGB_Image *circular = Images::makeCircular(cached, size)) {
            m_avatarCache[key] = std::unique_ptr<Fl_RGB_Image>(circular);
            return circular;
        }
    }

    return nullptr;
}

void DMSidebar::requestCircularAvatar(const std::string &url, int size) {
    if (url.empty()) {
        return;
    }

    std::string key = buildAvatarKey(url, size);
    if (m_avatarCache.find(key) != m_avatarCache.end()) {
        return;
    }
    if (m_avatarPending.find(key) != m_avatarPending.end()) {
        return;
    }

    m_avatarPending.insert(key);
    auto alive = m_isAlive;
    Images::loadImageAsync(url, [this, alive, url, key, size](Fl_RGB_Image *image) {
        if (!alive || !*alive) {
            return;
        }

        m_avatarPending.erase(key);

        if (!image || image->w() <= 0 || image->h() <= 0) {
            return;
        }

        Fl_RGB_Image *circular = Images::makeCircular(image, size);
        if (!circular) {
            return;
        }

        m_avatarCache[key] = std::unique_ptr<Fl_RGB_Image>(circular);
        redraw();
    });
}

Fl_RGB_Image *DMSidebar::getAnimatedAvatarFrame(const std::string &gifUrl, int size) {
    if (gifUrl.empty()) {
        return nullptr;
    }

    std::string key = buildAvatarKey(gifUrl, size);
    auto it = m_avatarGifCache.find(key);
    if (it == m_avatarGifCache.end()) {
        return nullptr;
    }

    auto &state = it->second;
    if (!state.animation || state.frames.empty()) {
        return nullptr;
    }

    size_t index = state.animation->getCurrentFrameIndex();
    if (index >= state.frames.size() || !state.frames[index]) {
        return nullptr;
    }

    return state.frames[index].get();
}

void DMSidebar::ensureAnimatedAvatar(const std::string &gifUrl, int size) {
    if (gifUrl.empty()) {
        return;
    }

    std::string cacheKey = buildAvatarKey(gifUrl, size);
    if (m_avatarGifCache.find(cacheKey) != m_avatarGifCache.end()) {
        if (m_hoveredAvatarKey == cacheKey) {
            startAvatarAnimation(cacheKey);
        }
        return;
    }

    if (m_avatarGifPending.find(cacheKey) != m_avatarGifPending.end()) {
        return;
    }

    m_avatarGifPending.insert(cacheKey);

    auto alive = m_isAlive;
    Images::loadImageAsync(gifUrl, [this, alive, gifUrl, size, cacheKey](Fl_RGB_Image *image) {
        if (!alive || !*alive) {
            return;
        }

        m_avatarGifPending.erase(cacheKey);

        if (!image) {
            return;
        }

        std::string gifPath = Images::getCacheFilePath(gifUrl, "gif");
        if (!std::filesystem::exists(gifPath)) {
            return;
        }

        auto animation = std::make_unique<GifAnimation>(gifPath, GifAnimation::ScalingStrategy::Lazy);
        if (!animation || !animation->isValid() || !animation->isAnimated()) {
            return;
        }

        AnimatedAvatarState state;
        state.animation = std::move(animation);

        size_t frameCount = state.animation->frameCount();
        state.frames.reserve(frameCount);

        for (size_t i = 0; i < frameCount; ++i) {
            Fl_RGB_Image *frame = state.animation->getFrame(i);
            if (!frame) {
                state.frames.emplace_back(nullptr);
                continue;
            }
            Fl_RGB_Image *circularFrame = Images::makeCircular(frame, size);
            state.frames.emplace_back(circularFrame);
        }

        bool hasFrame = std::any_of(state.frames.begin(), state.frames.end(),
                                    [](const std::unique_ptr<Fl_RGB_Image> &frame) { return frame != nullptr; });
        if (!hasFrame) {
            return;
        }

        state.animation->setFrame(0);
        m_avatarGifCache[cacheKey] = std::move(state);

        if (m_hoveredAvatarKey == cacheKey) {
            startAvatarAnimation(cacheKey);
        }

        redraw();
    });
}

void DMSidebar::setHoveredAvatarKey(const std::string &key) {
    if (key == m_hoveredAvatarKey) {
        return;
    }

    if (!m_hoveredAvatarKey.empty()) {
        stopAvatarAnimation(m_hoveredAvatarKey);
    }

    m_hoveredAvatarKey = key;

    if (!m_hoveredAvatarKey.empty()) {
        startAvatarAnimation(m_hoveredAvatarKey);
    }
}

bool DMSidebar::updateAvatarAnimation(const std::string &key) {
    auto it = m_avatarGifCache.find(key);
    if (it == m_avatarGifCache.end()) {
        return false;
    }

    auto &state = it->second;
    if (!state.animation || !state.animation->isAnimated() || state.frames.empty()) {
        return false;
    }

    bool advanced = state.animation->advance(AnimationManager::get().getFrameTime());
    if (advanced) {
        redraw();
    }

    return true;
}

void DMSidebar::startAvatarAnimation(const std::string &key) {
    auto it = m_avatarGifCache.find(key);
    if (it == m_avatarGifCache.end()) {
        return;
    }

    auto &state = it->second;
    if (state.running) {
        return;
    }

    state.running = true;
    state.frameTimeAccumulated = 0.0;

    if (state.animationId != 0) {
        AnimationManager::get().unregisterAnimation(state.animationId);
        state.animationId = 0;
    }

    state.animationId = AnimationManager::get().registerAnimation([this, key]() { return updateAvatarAnimation(key); });
}

void DMSidebar::stopAvatarAnimation(const std::string &key) {
    auto it = m_avatarGifCache.find(key);
    if (it == m_avatarGifCache.end()) {
        return;
    }

    auto &state = it->second;
    if (!state.running) {
        return;
    }

    if (state.animationId != 0) {
        AnimationManager::get().unregisterAnimation(state.animationId);
        state.animationId = 0;
    }

    state.running = false;
    state.frameTimeAccumulated = 0.0;
    if (state.animation) {
        state.animation->reset();
    }
}

void DMSidebar::enqueueUserResolve(const std::string &userId) {
    if (userId.empty()) {
        return;
    }
    if (m_userResolveQueuedUserIds.find(userId) != m_userResolveQueuedUserIds.end()) {
        return;
    }

    m_userResolveQueuedUserIds.insert(userId);
    m_userResolveQueue.push_back(userId);
    pumpUserResolveQueue();
}

void DMSidebar::pumpUserResolveQueue() {
    constexpr int kMaxInFlight = 3;

    while (m_userResolveInFlight < kMaxInFlight && !m_userResolveQueue.empty()) {
        std::string userId = std::move(m_userResolveQueue.front());
        m_userResolveQueue.pop_front();
        m_userResolveInFlight++;

        auto alive = m_isAlive;
        Discord::APIClient::get().getUser(
            userId,
            [this, alive, userId](const Discord::APIClient::Json &json) {
                if (!alive || !*alive) {
                    return;
                }

                try {
                    User user = User::fromJson(json);
                    Store::get().update([user = std::move(user)](AppState &state) mutable {
                        if (upsertUser(state.usersById, user)) {
                            state.usersRevision++;
                        }
                    });
                } catch (const std::exception &e) {
                    Logger::warn("Failed to parse user " + userId + ": " + std::string(e.what()));
                }

                m_userResolveInFlight--;
                pumpUserResolveQueue();
            },
            [this, alive, userId](int code, const std::string &error) {
                if (!alive || !*alive) {
                    return;
                }
                Logger::warn("Failed to resolve user " + userId + " (HTTP " + std::to_string(code) + "): " + error);
                if (code == 401 || code == -1) {
                    m_userResolveQueuedUserIds.erase(userId);
                }
                m_userResolveInFlight--;
                pumpUserResolveQueue();
            });
    }
}
