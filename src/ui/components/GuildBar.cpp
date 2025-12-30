#include "ui/components/GuildBar.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "models/GuildFolder.h"
#include "models/GuildInfo.h"
#include "state/AppState.h"
#include "ui/IconManager.h"
#include "ui/Theme.h"
#include "ui/components/GuildFolderWidget.h"
#include "ui/components/GuildIcon.h"
#include "utils/Logger.h"

namespace {
class HomeIcon : public Fl_Box {
  public:
    HomeIcon(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) { box(FL_NO_BOX); }

    void setOnClickCallback(std::function<void()> callback) { onClickCallback_ = std::move(callback); }

    void setCornerRadius(int radius) {
        cornerRadius_ = radius;
        redraw();
    }

    int handle(int event) override {
        switch (event) {
        case FL_PUSH:
            if (onClickCallback_) {
                onClickCallback_();
            }
            return 1;
        default:
            return Fl_Box::handle(event);
        }
    }

    void draw() override {
        fl_color(ThemeColors::BRAND_PRIMARY);

        const int r = cornerRadius_;
        fl_rectf(x() + r, y(), w() - 2 * r, h());
        fl_rectf(x(), y() + r, w(), h() - 2 * r);

        fl_pie(x(), y(), r * 2, r * 2, 90, 180);
        fl_pie(x() + w() - r * 2, y(), r * 2, r * 2, 0, 90);
        fl_pie(x(), y() + h() - r * 2, r * 2, r * 2, 180, 270);
        fl_pie(x() + w() - r * 2, y() + h() - r * 2, r * 2, r * 2, 270, 360);

        if (image()) {
            Fl_Box::draw();
        }
    }

  private:
    std::function<void()> onClickCallback_;
    int cornerRadius_ = 12;
};
} // namespace

GuildBar::GuildBar(int x, int y, int w, int h, const char *label) : Fl_Group(x, y, w, h, label) {
    box(FL_FLAT_BOX);
    color(ThemeColors::BG_SECONDARY);
    clip_children(1);
    resizable(nullptr);
    end();

    subscribeToStore();
    refresh();
}

GuildBar::~GuildBar() {
    if (m_guildDataListenerId != 0) {
        Store::get().unsubscribe(m_guildDataListenerId);
    }
}

void GuildBar::setOnHomeClicked(std::function<void()> cb) {
    m_onHomeClicked = std::move(cb);
    if (children() > 0) {
        auto *homeIcon = dynamic_cast<HomeIcon *>(child(0));
        if (homeIcon && m_onHomeClicked) {
            homeIcon->setOnClickCallback(m_onHomeClicked);
        }
    }
}

void GuildBar::setOnGuildSelected(std::function<void(const std::string &)> cb) {
    m_onGuildSelected = std::move(cb);

    // Update callbacks on existing guild icons
    for (int i = 0; i < children(); i++) {
        auto *widget = child(i);

        // Check if it's a direct GuildIcon
        if (auto *icon = dynamic_cast<GuildIcon *>(widget)) {
            if (m_onGuildSelected) {
                icon->setOnClickCallback(m_onGuildSelected);
            }
        }
        // Check if it's a GuildFolderWidget containing icons
        else if (auto *folder = dynamic_cast<GuildFolderWidget *>(widget)) {
            for (int j = 0; j < folder->children(); j++) {
                if (auto *icon = dynamic_cast<GuildIcon *>(folder->child(j))) {
                    if (m_onGuildSelected) {
                        icon->setOnClickCallback(m_onGuildSelected);
                    }
                }
            }
        }
    }
}

void GuildBar::draw() {
    draw_box();
    draw_children();
}

void GuildBar::subscribeToStore() {
    struct GuildData {
        std::vector<GuildInfo> guilds;
        std::vector<GuildFolder> folders;

        bool operator==(const GuildData &other) const {
            return guilds.size() == other.guilds.size() && folders.size() == other.folders.size() &&
                   std::equal(guilds.begin(), guilds.end(), other.guilds.begin(),
                              [](const GuildInfo &a, const GuildInfo &b) { return a.id == b.id; }) &&
                   std::equal(folders.begin(), folders.end(), other.folders.begin(),
                              [](const GuildFolder &a, const GuildFolder &b) {
                                  return a.id == b.id && a.guildIds == b.guildIds;
                              });
        }

        bool operator!=(const GuildData &other) const { return !(*this == other); }
    };

    m_guildDataListenerId = Store::get().subscribe<GuildData>(
        [](const AppState &state) -> GuildData { return GuildData{state.guilds, state.guildFolders}; },
        [this](const GuildData &) { refresh(); });
}

void GuildBar::refresh() {
    const AppState state = Store::get().snapshot();
    const int iconSize = m_iconSize;
    const int spacing = 8;
    const int guildBarWidth = w();
    const int iconMargin = (guildBarWidth - iconSize) / 2;

    auto &guilds = state.guilds;
    auto &folders = state.guildFolders;

    std::vector<std::string> newSignature;
    newSignature.push_back("guilds:" + std::to_string(guilds.size()));
    newSignature.push_back("folders:" + std::to_string(folders.size()));
    for (const auto &folder : folders) {
        newSignature.push_back("f" + std::to_string(folder.id) + ":" + std::to_string(folder.guildIds.size()));
    }

    bool needsRebuild = (newSignature != m_layoutSignature) || (children() == 0);
    if (!needsRebuild) {
        return;
    }

    if (needsRebuild) {
        while (children() > 0) {
            auto *w = child(children() - 1);
            remove(w);
            delete w;
        }

        begin();

        auto *home = new HomeIcon(0, 0, iconSize, iconSize);
        home->setCornerRadius(m_appIconCornerRadius);
        if (m_onHomeClicked) {
            home->setOnClickCallback(m_onHomeClicked);
        }

        auto *logoIcon = IconManager::load_recolored_icon("discord", m_appIconSize, ThemeColors::BRAND_ON_PRIMARY);
        if (logoIcon) {
            home->image(logoIcon);
        }

        new Fl_Box(0, 0, guildBarWidth, m_separatorSpacing);
        auto *separator = new Fl_Box(0, 0, guildBarWidth - m_separatorWidth, m_separatorHeight);
        separator->box(FL_FLAT_BOX);
        separator->color(ThemeColors::SEPARATOR_GUILD);

        std::unordered_set<std::string> guildsInAnyFolder;
        for (const auto &folder : folders) {
            for (const auto &guildId : folder.guildIds) {
                guildsInAnyFolder.insert(guildId);
            }
        }

        auto findGuild = [&guilds](const std::string &id) -> const GuildInfo * {
            for (const auto &g : guilds) {
                if (g.id == id)
                    return &g;
            }
            return nullptr;
        };

        for (const auto &folder : folders) {
            if (folder.guildIds.empty())
                continue;

            if (folder.id == -1) {
                for (const auto &guildId : folder.guildIds) {
                    const GuildInfo *guild = findGuild(guildId);
                    if (guild) {
                        auto *icon = new GuildIcon(0, 0, iconSize, guild->id, guild->icon, guild->name);
                        if (m_onGuildSelected) {
                            icon->setOnClickCallback(m_onGuildSelected);
                        }
                    }
                }
            } else {
                auto *folderWidget = new GuildFolderWidget(0, 0, iconSize);
                if (folder.color.has_value()) {
                    folderWidget->setColor(folder.color.value());
                }

                for (const auto &guildId : folder.guildIds) {
                    const GuildInfo *guild = findGuild(guildId);
                    if (guild) {
                        auto *icon = new GuildIcon(0, 0, iconSize, guild->id, guild->icon, guild->name);
                        if (m_onGuildSelected) {
                            icon->setOnClickCallback(m_onGuildSelected);
                        }
                        folderWidget->addGuild(icon);
                    }
                }

                folderWidget->layoutIcons();
            }
        }

        for (const auto &guild : guilds) {
            if (guildsInAnyFolder.find(guild.id) == guildsInAnyFolder.end()) {
                auto *icon = new GuildIcon(0, 0, iconSize, guild.id, guild.icon, guild.name);
                if (m_onGuildSelected) {
                    icon->setOnClickCallback(m_onGuildSelected);
                }
            }
        }

        end();
        m_layoutSignature = std::move(newSignature);
    }

    repositionChildren();
}

void GuildBar::repositionChildren() {
    const int iconSize = m_iconSize;
    const int spacing = 8;
    const int guildBarWidth = w();
    const int iconMargin = (guildBarWidth - iconSize) / 2;
    const int topPadding = iconMargin;

    int contentHeight = 0;
    if (children() > 0)
        contentHeight += topPadding + iconSize;
    if (children() > 1)
        contentHeight += m_separatorSpacing;
    if (children() > 2)
        contentHeight += m_separatorHeight + m_separatorSpacing;

    for (int i = 3; i < children(); ++i) {
        contentHeight += child(i)->h() + spacing;
    }

    const int maxScroll = contentHeight - h();
    if (maxScroll > 0) {
        if (m_scrollOffset > 0)
            m_scrollOffset = 0;
        if (m_scrollOffset < -maxScroll)
            m_scrollOffset = -maxScroll;
    } else {
        m_scrollOffset = 0;
    }

    int currentY = y() + topPadding + static_cast<int>(m_scrollOffset);
    if (children() > 0) {
        child(0)->resize(x() + iconMargin, currentY, iconSize, iconSize);
        currentY += iconSize;
    }

    if (children() > 1) {
        child(1)->resize(x(), currentY, guildBarWidth, m_separatorSpacing);
        currentY += m_separatorSpacing;
    }

    if (children() > 2) {
        child(2)->resize(x() + m_separatorWidth / 2, currentY, guildBarWidth - m_separatorWidth, m_separatorHeight);
        currentY += m_separatorHeight + m_separatorSpacing;
    }

    for (int i = 3; i < children(); ++i) {
        auto *w = child(i);
        w->resize(x() + iconMargin, currentY, iconSize, w->h());
        currentY += w->h() + spacing;
    }

    redraw();
}

void GuildBar::resize(int x, int y, int w, int h) {
    Fl_Group::resize(x, y, w, h);
    if (children() > 0) {
        repositionChildren();
    }
}

int GuildBar::handle(int event) {
    if (event == FL_MOUSEWHEEL) {
        if (!Fl::event_inside(this)) {
            return Fl_Group::handle(event);
        }

        const int iconSize = m_iconSize;
        const int spacing = 8;
        const int guildBarWidth = w();
        const int iconMargin = (guildBarWidth - iconSize) / 2;
        const int topPadding = iconMargin;

        int contentHeight = 0;
        if (children() > 0)
            contentHeight += topPadding + iconSize;
        if (children() > 1)
            contentHeight += m_separatorSpacing;
        if (children() > 2)
            contentHeight += m_separatorHeight + m_separatorSpacing;

        for (int i = 3; i < children(); ++i) {
            contentHeight += child(i)->h() + spacing;
        }

        const int maxScroll = contentHeight - h();

        if (maxScroll > 0) {
            m_scrollOffset -= Fl::event_dy() * 20.0;

            if (m_scrollOffset > 0)
                m_scrollOffset = 0;
            if (m_scrollOffset < -maxScroll)
                m_scrollOffset = -maxScroll;

            repositionChildren();
            return 1;
        }
    }

    return Fl_Group::handle(event);
}
