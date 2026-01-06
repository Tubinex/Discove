#pragma once

#include <FL/Fl_Group.H>
#include <functional>
#include <string>
#include <vector>

#include "state/Store.h"
#include "ui/VirtualScroll.h"

class GuildBar : public Fl_Group {
  public:
    GuildBar(int x, int y, int w, int h, const char *label = nullptr);
    ~GuildBar() override;

    void refresh();
    void resize(int x, int y, int w, int h) override;
    int handle(int event) override;
    void draw() override;
    void repositionChildren();

    void setOnGuildSelected(std::function<void(const std::string &)> cb);
    void setOnHomeClicked(std::function<void()> cb);
    void setIconSize(int size) {
        m_iconSize = size;
        refresh();
    }
    int getIconSize() const { return m_iconSize; }
    void setAppIconSize(int size) {
        m_appIconSize = size;
        refresh();
    }
    int getAppIconSize() const { return m_appIconSize; }
    void setAppIconCornerRadius(int radius) {
        m_appIconCornerRadius = radius;
        refresh();
    }
    int getAppIconCornerRadius() const { return m_appIconCornerRadius; }

    void setSeparatorWidth(int width) {
        m_separatorWidth = width;
        refresh();
    }
    int getSeparatorWidth() const { return m_separatorWidth; }

    void setSeparatorHeight(int height) {
        m_separatorHeight = height;
        refresh();
    }
    int getSeparatorHeight() const { return m_separatorHeight; }

    void setSeparatorSpacing(int spacing) {
        m_separatorSpacing = spacing;
        refresh();
    }
    int getSeparatorSpacing() const { return m_separatorSpacing; }

  private:
    enum class BarItemType { Home, Separator, Space, Guild, Folder };

    struct BarItem {
        BarItemType type;
        int height;
        std::string guildId;
        std::vector<std::string> folderGuildIds;
        int folderId = -1;
        std::optional<uint32_t> folderColor;
        std::string guildIconHash;
        std::string guildName;
    };

    struct PooledWidget {
        Fl_Widget *widget = nullptr;
        int boundDataIndex = -1;
        bool inUse = false;
    };

    void subscribeToStore();
    void bindGuildIcon(class GuildIcon *icon);
    void applySelection();

    std::vector<BarItem> buildItemList();
    void updateVisibleWidgets();
    PooledWidget &acquireWidget(BarItemType type);
    void releaseWidget(PooledWidget &pooled);
    void bindItemToWidget(size_t itemIndex);
    Fl_Widget *createWidgetForType(BarItemType type);
    bool widgetMatchesType(Fl_Widget *widget, BarItemType type);

    Store::ListenerId m_guildDataListenerId = 0;
    double m_scrollOffset = 0.0;
    int m_iconSize = 42;
    int m_appIconSize = 24;
    int m_appIconCornerRadius = 12;

    int m_separatorWidth = 32;
    int m_separatorHeight = 1;
    int m_separatorSpacing = 8;

    std::function<void(const std::string &)> m_onGuildSelected;
    std::function<void()> m_onHomeClicked;
    std::vector<std::string> m_layoutSignature;
    std::string m_selectedGuildId;
    bool m_homeSelected = false;

    std::vector<BarItem> m_items;
    std::vector<PooledWidget> m_widgetPool;
    VirtualScroll::HeightCache m_heightCache;
};
