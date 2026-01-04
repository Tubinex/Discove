#include "ui/components/GuildFolderWidget.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

#include <algorithm>
#include <cmath>

#include "ui/AnimationManager.h"
#include "ui/IconManager.h"
#include "ui/LayoutConstants.h"
#include "ui/Theme.h"
#include "ui/components/GuildBar.h"
#include "ui/components/GuildIcon.h"

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
    if (barHeight <= width) {
        fl_rectf(leftX, y, width, barHeight);
        return;
    }

    int radius = width / 2;
    fl_rectf(leftX, y, width - radius, barHeight);
    fl_rectf(leftX + width - radius, y + radius, radius, barHeight - (radius * 2));
    fl_pie(leftX + width - (radius * 2), y, radius * 2, radius * 2, 0, 90);
    fl_pie(leftX + width - (radius * 2), y + barHeight - (radius * 2), radius * 2, radius * 2, 270, 360);
}
} // namespace

GuildFolderWidget::GuildFolderWidget(int x, int y, int size) : Fl_Group(x, y, size, size), iconSize_(size) {
    box(FL_NO_BOX);
    clip_children(1);
    end();
}

GuildFolderWidget::~GuildFolderWidget() {
    if (animationId_ != 0) {
        AnimationManager::get().unregisterAnimation(animationId_);
    }
    if (indicatorAnimationId_ != 0) {
        AnimationManager::get().unregisterAnimation(indicatorAnimationId_);
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
            icon->setIndicatorsEnabled(false);
            icon->show();
        } else if (showExpanded) {
            int expandedX = x() + expandedPadding;
            int expandedY = y() + iconSize_ + 8 + static_cast<int>(i) * (folderIconSize + 8);

            icon->resize(expandedX, expandedY, folderIconSize, folderIconSize);
            icon->setCornerRadius(10);
            icon->setMaskColor(folderColor);
            icon->setFallbackFontSize(20);
            icon->setIndicatorsEnabled(true);
            icon->show();
        } else {
            icon->setIndicatorsEnabled(false);
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
    startIndicatorAnimation();
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
        startIndicatorAnimation();
        return 1;

    case FL_LEAVE:
        hovered_ = false;
        startIndicatorAnimation();
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

void GuildFolderWidget::notifyChildSelectionChanged() { startIndicatorAnimation(); }

float GuildFolderWidget::indicatorTargetHeight() const {
    if (expanded_ || animationProgress_ >= 0.5f) {
        return 0.0f;
    }

    bool hasSelectedChild = std::any_of(guildIcons_.begin(), guildIcons_.end(),
                                        [](const GuildIcon *icon) { return icon && icon->isSelected(); });
    if (hasSelectedChild) {
        return static_cast<float>(iconSize_);
    }

    if (hovered_) {
        return static_cast<float>(
            std::max(LayoutConstants::kGuildHoverIndicatorMinHeight, iconSize_ / 2));
    }

    return 0.0f;
}

void GuildFolderWidget::startIndicatorAnimation() {
    float target = indicatorTargetHeight();
    if (std::abs(indicatorHeight_ - target) <= 0.5f) {
        indicatorHeight_ = target;
        return;
    }
    if (indicatorAnimationId_ != 0) {
        return;
    }
    indicatorAnimationId_ = AnimationManager::get().registerAnimation([this]() { return updateIndicatorAnimation(); });
}

bool GuildFolderWidget::updateIndicatorAnimation() {
    float target = indicatorTargetHeight();
    float delta = LayoutConstants::kGuildIndicatorAnimationSpeed * AnimationManager::get().getFrameTime();

    if (indicatorHeight_ < target) {
        indicatorHeight_ = std::min(indicatorHeight_ + delta, target);
    } else if (indicatorHeight_ > target) {
        indicatorHeight_ = std::max(indicatorHeight_ - delta, target);
    }

    int leftX = getGuildBarLeftX(this);
    damage(FL_DAMAGE_USER1, leftX, y(), w() + (x() - leftX), iconSize_);

    if (std::abs(indicatorHeight_ - target) <= 0.5f) {
        indicatorHeight_ = target;
        indicatorAnimationId_ = 0;
        return false;
    }

    return true;
}

void GuildFolderWidget::draw() {
    if (indicatorHeight_ > 0.5f) {
        int indicatorHeight = static_cast<int>(indicatorHeight_);
        int indicatorY = y() + (iconSize_ - indicatorHeight) / 2;
        drawIndicatorBar(getGuildBarLeftX(this), indicatorY, indicatorHeight, ThemeColors::TEXT_NORMAL);
    }

    Fl_Color folderColor = color_ ? color_ : ThemeColors::BG_TERTIARY;

    const int r = cornerRadius_;
    fl_color(folderColor);
    fl_pie(x(), y(), r * 2, r * 2, 90, 180);
    fl_pie(x() + iconSize_ - r * 2, y(), r * 2, r * 2, 0, 90);
    fl_pie(x(), y() + h() - r * 2, r * 2, r * 2, 180, 270);
    fl_pie(x() + iconSize_ - r * 2, y() + h() - r * 2, r * 2, r * 2, 270, 360);
    fl_rectf(x() + r, y(), iconSize_ - r * 2, h());
    fl_rectf(x(), y() + r, iconSize_, h() - r * 2);

    int leftX = getGuildBarLeftX(this);
    int clipW = (x() + w()) - leftX;
    fl_push_clip(leftX, y(), clipW, h());

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
