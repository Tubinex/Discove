#include "ui/components/GuildBar.h"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "models/GuildFolder.h"
#include "models/GuildInfo.h"
#include "state/AppState.h"
#include "ui/AnimationManager.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/Theme.h"
#include "ui/components/GuildFolderWidget.h"
#include "ui/components/GuildIcon.h"
#include "utils/Logger.h"

namespace {
int getGuildBarLeftX(const Fl_Widget *widget) {
    const Fl_Widget *current = widget;
    while (current) {
        if (dynamic_cast<const GuildBar *>(current)) {
            return current->x() + LayoutConstants::kGuildIndicatorInset;
        }
        current = current->parent();
    }
    return widget ? widget->x() : 0;
}

void drawIndicatorBar(int leftX, int y, int height, Fl_Color color) {
    int barHeight = height;
    const int width = LayoutConstants::kGuildIndicatorWidth;
    if (barHeight < width) {
        barHeight = width;
    }

    fl_color(color);
    int radius = width / 2;
    if (barHeight <= width) {
        fl_rectf(leftX, y, width, barHeight);
        return;
    }

    fl_rectf(leftX, y, width - radius, barHeight);
    fl_rectf(leftX + width - radius, y + radius, radius, barHeight - (radius * 2));
    fl_pie(leftX + width - (radius * 2), y, radius * 2, radius * 2, 0, 90);
    fl_pie(leftX + width - (radius * 2), y + barHeight - (radius * 2), radius * 2, radius * 2, 270, 360);
}

class HomeIcon : public Fl_Box {
  public:
    HomeIcon(int X, int Y, int W, int H) : Fl_Box(X, Y, W, H) { box(FL_NO_BOX); }
    ~HomeIcon() override {
        if (indicatorAnimationId_ != 0) {
            AnimationManager::get().unregisterAnimation(indicatorAnimationId_);
            indicatorAnimationId_ = 0;
        }
    }

    void setOnClickCallback(std::function<void()> callback) { onClickCallback_ = std::move(callback); }

    void setCornerRadius(int radius) {
        cornerRadius_ = radius;
        redraw();
    }

    void setSelected(bool selected) {
        if (selected_ == selected) {
            return;
        }
        selected_ = selected;
        startIndicatorAnimation();
    }

    int handle(int event) override {
        switch (event) {
        case FL_ENTER:
            hovered_ = true;
            startIndicatorAnimation();
            return 1;
        case FL_LEAVE:
            hovered_ = false;
            startIndicatorAnimation();
            return 1;
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
        if (indicatorHeight_ > 0.5f) {
            int indicatorHeight = static_cast<int>(indicatorHeight_);
            int indicatorY = y() + (h() - indicatorHeight) / 2;
            drawIndicatorBar(getGuildBarLeftX(this), indicatorY, indicatorHeight, ThemeColors::TEXT_NORMAL);
        }

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
    float indicatorTargetHeight() const {
        if (selected_) {
            return static_cast<float>(h());
        }
        if (hovered_) {
            return static_cast<float>(
                std::max(LayoutConstants::kGuildHoverIndicatorMinHeight, h() / 2));
        }
        return 0.0f;
    }

    void startIndicatorAnimation() {
        float target = indicatorTargetHeight();
        if (std::abs(indicatorHeight_ - target) <= 0.5f) {
            indicatorHeight_ = target;
            return;
        }
        if (indicatorAnimationId_ != 0) {
            return;
        }
        indicatorAnimationId_ = AnimationManager::get().registerAnimation([this]() {
            return updateIndicatorAnimation();
        });
    }

    bool updateIndicatorAnimation() {
        float target = indicatorTargetHeight();
        float delta = LayoutConstants::kGuildIndicatorAnimationSpeed * AnimationManager::get().getFrameTime();

        if (indicatorHeight_ < target) {
            indicatorHeight_ = std::min(indicatorHeight_ + delta, target);
        } else if (indicatorHeight_ > target) {
            indicatorHeight_ = std::max(indicatorHeight_ - delta, target);
        }

        int leftX = getGuildBarLeftX(this);
        damage(FL_DAMAGE_USER1, leftX, y(), w() + (x() - leftX), h());

        if (std::abs(indicatorHeight_ - target) <= 0.5f) {
            indicatorHeight_ = target;
            indicatorAnimationId_ = 0;
            return false;
        }
        return true;
    }

    std::function<void()> onClickCallback_;
    int cornerRadius_ = 12;
    bool hovered_ = false;
    bool selected_ = false;
    float indicatorHeight_ = 0.0f;
    AnimationManager::AnimationId indicatorAnimationId_ = 0;
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
        if (homeIcon) {
            homeIcon->setOnClickCallback([this]() {
                m_homeSelected = true;
                m_selectedGuildId.clear();
                applySelection();
                if (m_onHomeClicked) {
                    m_onHomeClicked();
                }
            });
        }
    }
}

void GuildBar::setOnGuildSelected(std::function<void(const std::string &)> cb) {
    m_onGuildSelected = std::move(cb);
    for (int i = 0; i < children(); i++) {
        auto *widget = child(i);

        if (auto *icon = dynamic_cast<GuildIcon *>(widget)) {
            bindGuildIcon(icon);
        }
        else if (auto *folder = dynamic_cast<GuildFolderWidget *>(widget)) {
            for (int j = 0; j < folder->children(); j++) {
                if (auto *icon = dynamic_cast<GuildIcon *>(folder->child(j))) {
                    bindGuildIcon(icon);
                }
            }
        }
    }
}

void GuildBar::draw() {
    if (!damage()) {
        return;
    }

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
        home->setSelected(m_homeSelected);
        home->setOnClickCallback([this]() {
            m_homeSelected = true;
            m_selectedGuildId.clear();
            applySelection();
            if (m_onHomeClicked) {
                m_onHomeClicked();
            }
        });

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
                        bindGuildIcon(icon);
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
                        bindGuildIcon(icon);
                        folderWidget->addGuild(icon);
                    }
                }

                folderWidget->layoutIcons();
            }
        }

        for (const auto &guild : guilds) {
            if (guildsInAnyFolder.find(guild.id) == guildsInAnyFolder.end()) {
                auto *icon = new GuildIcon(0, 0, iconSize, guild.id, guild.icon, guild.name);
                bindGuildIcon(icon);
            }
        }

        end();
        m_layoutSignature = std::move(newSignature);
    }

    repositionChildren();
}

void GuildBar::bindGuildIcon(GuildIcon *icon) {
    if (!icon) {
        return;
    }

    bool selected = !m_homeSelected && !m_selectedGuildId.empty() && icon->guildId() == m_selectedGuildId;
    icon->setSelected(selected);
    icon->setOnClickCallback([this](const std::string &guildId) {
        m_homeSelected = false;
        m_selectedGuildId = guildId;
        applySelection();
        if (m_onGuildSelected) {
            m_onGuildSelected(guildId);
        }
    });
}

void GuildBar::applySelection() {
    if (children() > 0) {
        if (auto *homeIcon = dynamic_cast<HomeIcon *>(child(0))) {
            homeIcon->setSelected(m_homeSelected);
        }
    }

    for (int i = 0; i < children(); i++) {
        auto *widget = child(i);

        if (auto *icon = dynamic_cast<GuildIcon *>(widget)) {
            bool selected = !m_homeSelected && !m_selectedGuildId.empty() && icon->guildId() == m_selectedGuildId;
            icon->setSelected(selected);
        } else if (auto *folder = dynamic_cast<GuildFolderWidget *>(widget)) {
            for (int j = 0; j < folder->children(); j++) {
                if (auto *icon = dynamic_cast<GuildIcon *>(folder->child(j))) {
                    bool selected =
                        !m_homeSelected && !m_selectedGuildId.empty() && icon->guildId() == m_selectedGuildId;
                    icon->setSelected(selected);
                }
            }
        }
    }
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
