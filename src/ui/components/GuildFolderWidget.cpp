#include "ui/components/GuildFolderWidget.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include "ui/AnimationManager.h"
#include "ui/IconManager.h"
#include "ui/Theme.h"
#include "ui/components/GuildBar.h"
#include "ui/components/GuildIcon.h"

GuildFolderWidget::GuildFolderWidget(int x, int y, int size) : Fl_Group(x, y, size, size), iconSize_(size) {
    box(FL_NO_BOX);
    clip_children(1);
    end();
}

GuildFolderWidget::~GuildFolderWidget() {
    if (animationId_ != 0) {
        AnimationManager::get().unregisterAnimation(animationId_);
    }
}

void GuildFolderWidget::addGuild(GuildIcon *icon) {
    if (icon) {
        guildIcons_.push_back(icon);
        add(icon);
    }
}

void GuildFolderWidget::layoutIcons() {
    Fl_Color folderColor = color_ ? color_ : ThemeColors::BG_TERTIARY;

    float progress = animationProgress_;

    const int gridPadding = 4;
    const int gridIconSize = (iconSize_ - 3 * gridPadding) / 2;

    const int folderIconSize = static_cast<int>(iconSize_ * 0.85);
    const int expandedPadding = (iconSize_ - folderIconSize) / 2;

    bool showGrid = progress < 0.5f;
    bool showExpanded = progress > 0.01f;

    for (size_t i = 0; i < guildIcons_.size(); ++i) {
        auto *icon = guildIcons_[i];

        if (showGrid && i < 4) {
            int gridX = x() + gridPadding + static_cast<int>(i % 2) * (gridIconSize + gridPadding);
            int gridY = y() + gridPadding + static_cast<int>(i / 2) * (gridIconSize + gridPadding);

            int expandedHeight = getExpandedHeight();
            int slideOffset = static_cast<int>((expandedHeight - iconSize_) * progress);
            int currentY = gridY + slideOffset;

            icon->resize(gridX, currentY, gridIconSize, gridIconSize);
            icon->setCornerRadius(gridIconSize / 2);
            icon->setMaskColor(folderColor);
            icon->setFallbackFontSize(14);
            icon->show();
        } else if (showExpanded) {
            int expandedX = x() + expandedPadding;
            int expandedY = y() + iconSize_ + 8 + static_cast<int>(i) * (folderIconSize + 8);

            icon->resize(expandedX, expandedY, folderIconSize, folderIconSize);
            icon->setCornerRadius(10);
            icon->setMaskColor(folderColor);
            icon->setFallbackFontSize(20);
            icon->show();
        } else {
            icon->hide();
        }
    }
}

void GuildFolderWidget::resize(int x, int y, int w, int h) {
    if (animating_) {
        Fl_Group::resize(x, y, w, h);
    } else {
        int correctHeight = expanded_ ? getExpandedHeight() : getCollapsedHeight();
        Fl_Group::resize(x, y, w, correctHeight);
    }

    layoutIcons();
}

void GuildFolderWidget::setExpanded(bool expanded) {
    if (expanded_ == expanded && !animating_) {
        return;
    }

    targetExpanded_ = expanded;
    if (!animating_ || (animating_ && expanded_ != targetExpanded_)) {
        animating_ = true;

        if (animationId_ != 0) {
            AnimationManager::get().unregisterAnimation(animationId_);
        }

        animationId_ = AnimationManager::get().registerAnimation([this]() { return updateAnimation(); });
    }
}

void GuildFolderWidget::toggle() { setExpanded(!expanded_); }

bool GuildFolderWidget::updateAnimation() {
    if (!animating_) {
        animationId_ = 0;
        return false;
    }

    const float animationSpeed = 0.15f;

    if (targetExpanded_) {
        animationProgress_ += animationSpeed;
        if (animationProgress_ >= 1.0f) {
            animationProgress_ = 1.0f;
            expanded_ = true;
            animating_ = false;
        }
    } else {
        animationProgress_ -= animationSpeed;
        if (animationProgress_ <= 0.0f) {
            animationProgress_ = 0.0f;
            expanded_ = false;
            animating_ = false;
        }
    }

    float progress = animationProgress_;

    int collapsedHeight = getCollapsedHeight();
    int expandedHeight = getExpandedHeight();
    int currentHeight = collapsedHeight + static_cast<int>((expandedHeight - collapsedHeight) * progress);

    Fl_Group::resize(x(), y(), w(), currentHeight);

    layoutIcons();
    redraw();

    if (parent()) {
        parent()->redraw();
        auto *guildBar = dynamic_cast<GuildBar *>(parent());
        if (guildBar) {
            guildBar->repositionChildren();
        }
    }

    if (!animating_) {
        animationId_ = 0;
        return false;
    }

    return true;
}

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

    const int r = cornerRadius_;
    fl_color(folderColor);
    fl_pie(x(), y(), r * 2, r * 2, 90, 180);
    fl_pie(x() + iconSize_ - r * 2, y(), r * 2, r * 2, 0, 90);
    fl_pie(x(), y() + h() - r * 2, r * 2, r * 2, 180, 270);
    fl_pie(x() + iconSize_ - r * 2, y() + h() - r * 2, r * 2, r * 2, 270, 360);
    fl_rectf(x() + r, y(), iconSize_ - r * 2, h());
    fl_rectf(x(), y() + r, iconSize_, h() - r * 2);

    fl_push_clip(x(), y(), w(), h());

    float progress = animationProgress_;
    if (progress > 0.01f) {
        auto *folderIcon = IconManager::load_recolored_icon("folder", folderIconSize_, ThemeColors::BRAND_PRIMARY);
        if (folderIcon) {
            int finalY = y() + (iconSize_ - folderIconSize_) / 2;
            int startY = y() - folderIconSize_;
            int currentY = startY + static_cast<int>((finalY - startY) * progress);

            folderIcon->draw(x() + (iconSize_ - folderIconSize_) / 2, currentY);
        }
    }

    for (int i = 0; i < children(); i++) {
        Fl_Widget *child = this->child(i);
        if (child && child->visible()) {
            draw_child(*child);
        }
    }

    fl_pop_clip();
}
