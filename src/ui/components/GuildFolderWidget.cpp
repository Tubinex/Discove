#include "ui/components/GuildFolderWidget.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "ui/IconManager.h"
#include "ui/Theme.h"
#include "ui/components/GuildBar.h"
#include "ui/components/GuildIcon.h"

GuildFolderWidget::GuildFolderWidget(int x, int y, int size) : Fl_Group(x, y, size, size), iconSize_(size) {
    box(FL_NO_BOX);
    end();
}

void GuildFolderWidget::addGuild(GuildIcon *icon) {
    if (icon) {
        guildIcons_.push_back(icon);
        add(icon);
    }
}

void GuildFolderWidget::layoutIcons() {
    Fl_Color folderColor = color_ ? color_ : ThemeColors::BG_TERTIARY;

    if (expanded_) {
        const int folderIconSize = static_cast<int>(iconSize_ * 0.85);
        const int padding = (iconSize_ - folderIconSize) / 2;
        int currentY = y() + iconSize_ + 8;

        for (auto *icon : guildIcons_) {
            icon->resize(x() + padding, currentY, folderIconSize, folderIconSize);
            icon->setCornerRadius(10);
            icon->setMaskColor(folderColor);
            icon->setFallbackFontSize(20);
            icon->show();
            currentY += folderIconSize + 8;
        }
    } else {
        const int padding = 4;
        const int gridIconSize = (iconSize_ - 3 * padding) / 2;

        for (size_t i = 0; i < guildIcons_.size() && i < 4; ++i) {
            int gridX = x() + padding + static_cast<int>(i % 2) * (gridIconSize + padding);
            int gridY = y() + padding + static_cast<int>(i / 2) * (gridIconSize + padding);
            guildIcons_[i]->resize(gridX, gridY, gridIconSize, gridIconSize);
            guildIcons_[i]->setCornerRadius(gridIconSize / 2);
            guildIcons_[i]->setMaskColor(folderColor);
            guildIcons_[i]->setFallbackFontSize(14);
            guildIcons_[i]->show();
        }
        for (size_t i = 4; i < guildIcons_.size(); ++i) {
            guildIcons_[i]->hide();
        }
    }
}

void GuildFolderWidget::resize(int x, int y, int w, int h) {
    int correctHeight;
    if (expanded_) {
        const int folderIconSize = static_cast<int>(iconSize_ * 0.85);
        correctHeight = iconSize_ + 8 + (static_cast<int>(guildIcons_.size()) * (folderIconSize + 8));
    } else {
        correctHeight = iconSize_;
    }

    Fl_Group::resize(x, y, w, correctHeight);
    layoutIcons();
}

void GuildFolderWidget::setExpanded(bool expanded) {
    if (expanded_ == expanded) {
        return;
    }

    expanded_ = expanded;

    if (expanded_) {
        const int folderIconSize = static_cast<int>(iconSize_ * 0.85);
        int totalHeight = iconSize_ + 8 + (static_cast<int>(guildIcons_.size()) * (folderIconSize + 8));
        Fl_Group::resize(x(), y(), w(), totalHeight);
    } else {
        Fl_Group::resize(x(), y(), w(), iconSize_);
    }

    layoutIcons();
    redraw();

    if (parent()) {
        parent()->redraw();
        auto *guildBar = dynamic_cast<GuildBar *>(parent());
        if (guildBar) {
            guildBar->repositionChildren();
        }
    }
}

void GuildFolderWidget::toggle() { setExpanded(!expanded_); }

int GuildFolderWidget::handle(int event) {
    switch (event) {
    case FL_ENTER:
        hovered_ = true;
        redraw();
        return 1;

    case FL_LEAVE:
        hovered_ = false;
        redraw();
        return 1;

    case FL_PUSH:
        if (Fl::event_y() >= y() && Fl::event_y() < y() + iconSize_) {
            toggle();
            return 1;
        }
        return Fl_Group::handle(event);

    default:
        return Fl_Group::handle(event);
    }
}

void GuildFolderWidget::draw() {
    Fl_Color folderColor = color_ ? color_ : ThemeColors::BG_TERTIARY;

    if (expanded_) {
        const int r = cornerRadius_;
        fl_color(folderColor);
        fl_pie(x(), y(), r * 2, r * 2, 90, 180);
        fl_pie(x() + iconSize_ - r * 2, y(), r * 2, r * 2, 0, 90);
        fl_pie(x(), y() + h() - r * 2, r * 2, r * 2, 180, 270);
        fl_pie(x() + iconSize_ - r * 2, y() + h() - r * 2, r * 2, r * 2, 270, 360);
        fl_rectf(x() + r, y(), iconSize_ - r * 2, h());
        fl_rectf(x(), y() + r, iconSize_, h() - r * 2);

        auto *folderIcon = IconManager::load_icon("folder", 24);
        if (folderIcon) {
            folderIcon->draw(x() + (iconSize_ - 24) / 2, y() + (iconSize_ - 24) / 2);
        }

        Fl_Group::draw();
    } else {
        const int r = cornerRadius_;
        fl_color(folderColor);
        fl_pie(x(), y(), r * 2, r * 2, 90, 180);
        fl_pie(x() + iconSize_ - r * 2, y(), r * 2, r * 2, 0, 90);
        fl_pie(x(), y() + iconSize_ - r * 2, r * 2, r * 2, 180, 270);
        fl_pie(x() + iconSize_ - r * 2, y() + iconSize_ - r * 2, r * 2, r * 2, 270, 360);
        fl_rectf(x() + r, y(), iconSize_ - r * 2, iconSize_);
        fl_rectf(x(), y() + r, iconSize_, iconSize_ - r * 2);

        Fl_Group::draw();
    }
}
