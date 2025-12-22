#include "ui/Theme.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>

void init_theme() {
    Fl::scheme("gtk+");

    Fl::background(ThemeColors::red(ThemeColors::BG_PRIMARY), ThemeColors::green(ThemeColors::BG_PRIMARY),
                   ThemeColors::blue(ThemeColors::BG_PRIMARY));
    Fl::background2(ThemeColors::red(ThemeColors::BG_SECONDARY), ThemeColors::green(ThemeColors::BG_SECONDARY),
                    ThemeColors::blue(ThemeColors::BG_SECONDARY));
    Fl::foreground(ThemeColors::red(ThemeColors::TEXT_NORMAL), ThemeColors::green(ThemeColors::TEXT_NORMAL),
                   ThemeColors::blue(ThemeColors::TEXT_NORMAL));

    FL_NORMAL_SIZE = 14;
}