#include "screens/UILibraryScreen.h"
#include "router/Router.h"
#include "ui/RoundedButton.h"
#include "ui/Theme.h"
#include "utils/Logger.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/fl_draw.H>

UILibraryScreen::UILibraryScreen(int x, int y, int w, int h)
    : Screen(x, y, w, h, "UI Library"), m_currentY(20) {
    setupUI();
}

UILibraryScreen::~UILibraryScreen() {}

void UILibraryScreen::setupUI() {
    begin();

    m_scroll = new Fl_Scroll(0, 0, w(), h());
    m_scroll->type(Fl_Scroll::VERTICAL);
    m_scroll->box(FL_FLAT_BOX);
    m_scroll->color(ThemeColors::BG_PRIMARY);
    m_scroll->begin();

    Fl_Box *title = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 60, "UI Component Library");
    title->labelsize(36);
    title->labelfont(FL_BOLD);
    title->labelcolor(ThemeColors::TEXT_NORMAL);
    title->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 70;

    Fl_Box *subtitle = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 30,
                                   "All available components for the Discove application");
    subtitle->labelsize(14);
    subtitle->labelcolor(fl_color_average(ThemeColors::TEXT_NORMAL, ThemeColors::BG_PRIMARY, 0.7f));
    subtitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 40;

    addColors();
    addTypography();
    addButtons();
    addSpacing();

    RoundedButton *backBtn = new RoundedButton(m_padding, m_currentY, 140, 40, "Back to Home");
    backBtn->setBorderRadius(8);
    backBtn->color(ThemeColors::BTN_SECONDARY);
    backBtn->selection_color(ThemeColors::BTN_SECONDARY_HOVER);
    backBtn->labelcolor(FL_WHITE);
    backBtn->callback([](Fl_Widget *, void *) { Router::navigate("/"); });
    m_currentY += 60;

    m_scroll->end();
    end();
}

void UILibraryScreen::addSectionTitle(const char *title) {
    m_currentY += m_sectionSpacing;

    Fl_Box *sectionTitle = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 40, title);
    sectionTitle->labelsize(24);
    sectionTitle->labelfont(FL_BOLD);
    sectionTitle->labelcolor(ThemeColors::TEXT_NORMAL);
    sectionTitle->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 50;
}

void UILibraryScreen::addColors() {
    addSectionTitle("Color Palette");

    struct ColorInfo {
        const char *name;
        Fl_Color color;
        const char *hexCode;
    };

    ColorInfo colors[] = {{"BG Primary", ThemeColors::BG_PRIMARY, "#121214"},
                          {"BG Secondary", ThemeColors::BG_SECONDARY, "#1a1a1e"},
                          {"BG Tertiary", ThemeColors::BG_TERTIARY, "#202024"},
                          {"BG Accent", ThemeColors::BG_ACCENT, "#333338"},
                          {"Text Normal", ThemeColors::TEXT_NORMAL, "#DCDCDC"},
                          {"Btn Primary", ThemeColors::BTN_PRIMARY, "#4F6FFF"},
                          {"Btn Secondary", ThemeColors::BTN_SECONDARY, "#4A4A4F"},
                          {"Btn Danger", ThemeColors::BTN_DANGER, "#E53E3E"}};

    int colorBoxSize = 80;
    int colorBoxSpacing = 20;
    int startX = m_padding;

    for (const auto &colorInfo : colors) {
        Fl_Box *colorBox = new Fl_Box(startX, m_currentY, colorBoxSize, colorBoxSize);
        colorBox->box(FL_FLAT_BOX);
        colorBox->color(colorInfo.color);

        Fl_Box *colorName =
            new Fl_Box(startX, m_currentY + colorBoxSize + 5, colorBoxSize + 100, 20, colorInfo.name);
        colorName->labelsize(12);
        colorName->labelfont(FL_BOLD);
        colorName->labelcolor(ThemeColors::TEXT_NORMAL);
        colorName->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        Fl_Box *colorHex =
            new Fl_Box(startX, m_currentY + colorBoxSize + 25, colorBoxSize + 100, 20, colorInfo.hexCode);
        colorHex->labelsize(11);
        colorHex->labelcolor(fl_color_average(ThemeColors::TEXT_NORMAL, ThemeColors::BG_PRIMARY, 0.7f));
        colorHex->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);

        startX += colorBoxSize + colorBoxSpacing + 100;
    }

    m_currentY += colorBoxSize + 60;
}

void UILibraryScreen::addTypography() {
    addSectionTitle("Typography");

    struct FontExample {
        const char *text;
        int size;
        Fl_Font font;
    };

    FontExample examples[] = {
        {"Heading 1 - Bold 36px", 36, FL_BOLD},
        {"Heading 2 - Bold 24px", 24, FL_BOLD},
        {"Body - Regular 14px", 14, FL_HELVETICA},
        {"Small - Regular 12px", 12, FL_HELVETICA},
    };

    for (const auto &example : examples) {
        Fl_Box *fontBox = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, example.size + 10, example.text);
        fontBox->labelsize(example.size);
        fontBox->labelfont(example.font);
        fontBox->labelcolor(ThemeColors::TEXT_NORMAL);
        fontBox->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        m_currentY += example.size + 20;
    }
}

void UILibraryScreen::addButtons() {
    addSectionTitle("Buttons");

    Fl_Button *primaryBtn = new Fl_Button(m_padding, m_currentY, 160, 40, "Primary Button");
    primaryBtn->box(FL_FLAT_BOX);
    primaryBtn->down_box(FL_FLAT_BOX);
    primaryBtn->color(ThemeColors::BTN_PRIMARY);
    primaryBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    primaryBtn->labelcolor(FL_WHITE);
    primaryBtn->labelsize(14);
    primaryBtn->clear_visible_focus();
    primaryBtn->callback([](Fl_Widget *, void *) { Logger::info("Primary button clicked"); });
    m_currentY += 50;

    Fl_Button *secondaryBtn = new Fl_Button(m_padding, m_currentY, 160, 40, "Secondary Button");
    secondaryBtn->box(FL_FLAT_BOX);
    secondaryBtn->down_box(FL_FLAT_BOX);
    secondaryBtn->color(ThemeColors::BTN_SECONDARY);
    secondaryBtn->selection_color(ThemeColors::BTN_SECONDARY_HOVER);
    secondaryBtn->labelcolor(FL_WHITE);
    secondaryBtn->labelsize(14);
    secondaryBtn->clear_visible_focus();
    secondaryBtn->callback([](Fl_Widget *, void *) { Logger::info("Secondary button clicked"); });
    m_currentY += 50;

    Fl_Button *dangerBtn = new Fl_Button(m_padding, m_currentY, 160, 40, "Danger Button");
    dangerBtn->box(FL_FLAT_BOX);
    dangerBtn->down_box(FL_FLAT_BOX);
    dangerBtn->color(ThemeColors::BTN_DANGER);
    dangerBtn->selection_color(ThemeColors::BTN_DANGER_HOVER);
    dangerBtn->labelcolor(FL_WHITE);
    dangerBtn->labelsize(14);
    dangerBtn->clear_visible_focus();
    dangerBtn->callback([](Fl_Widget *, void *) { Logger::info("Danger button clicked"); });
    m_currentY += 60;

    Fl_Box *sizeLabel = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 30, "Size Variations:");
    sizeLabel->labelsize(16);
    sizeLabel->labelfont(FL_BOLD);
    sizeLabel->labelcolor(ThemeColors::TEXT_NORMAL);
    sizeLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 40;

    Fl_Button *smallBtn = new Fl_Button(m_padding, m_currentY, 120, 32, "Small");
    smallBtn->box(FL_FLAT_BOX);
    smallBtn->down_box(FL_FLAT_BOX);
    smallBtn->color(ThemeColors::BTN_PRIMARY);
    smallBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    smallBtn->labelcolor(FL_WHITE);
    smallBtn->labelsize(12);
    smallBtn->clear_visible_focus();
    m_currentY += 42;

    Fl_Button *largeBtn = new Fl_Button(m_padding, m_currentY, 200, 50, "Large Button");
    largeBtn->box(FL_FLAT_BOX);
    largeBtn->down_box(FL_FLAT_BOX);
    largeBtn->color(ThemeColors::BTN_PRIMARY);
    largeBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    largeBtn->labelcolor(FL_WHITE);
    largeBtn->labelsize(16);
    largeBtn->clear_visible_focus();
    m_currentY += 80;

    Fl_Box *roundedLabel = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 30, "Rounded Buttons:");
    roundedLabel->labelsize(16);
    roundedLabel->labelfont(FL_BOLD);
    roundedLabel->labelcolor(ThemeColors::TEXT_NORMAL);
    roundedLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 40;

    RoundedButton *noRadiusBtn = new RoundedButton(m_padding, m_currentY, 160, 40, "Radius: 0px");
    noRadiusBtn->setBorderRadius(0);
    noRadiusBtn->color(ThemeColors::BTN_PRIMARY);
    noRadiusBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    noRadiusBtn->labelcolor(FL_WHITE);
    noRadiusBtn->labelsize(14);
    noRadiusBtn->callback([](Fl_Widget *, void *) { Logger::info("No radius button clicked"); });
    m_currentY += 50;

    RoundedButton *smallRadiusBtn = new RoundedButton(m_padding, m_currentY, 160, 40, "Radius: 4px");
    smallRadiusBtn->setBorderRadius(4);
    smallRadiusBtn->color(ThemeColors::BTN_PRIMARY);
    smallRadiusBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    smallRadiusBtn->labelcolor(FL_WHITE);
    smallRadiusBtn->labelsize(14);
    smallRadiusBtn->callback([](Fl_Widget *, void *) { Logger::info("Small radius button clicked"); });
    m_currentY += 50;

    RoundedButton *mediumRadiusBtn = new RoundedButton(m_padding, m_currentY, 160, 40, "Radius: 8px");
    mediumRadiusBtn->setBorderRadius(8);
    mediumRadiusBtn->color(ThemeColors::BTN_PRIMARY);
    mediumRadiusBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    mediumRadiusBtn->labelcolor(FL_WHITE);
    mediumRadiusBtn->labelsize(14);
    mediumRadiusBtn->callback([](Fl_Widget *, void *) { Logger::info("Medium radius button clicked"); });
    m_currentY += 50;

    RoundedButton *largeRadiusBtn = new RoundedButton(m_padding, m_currentY, 160, 40, "Radius: 16px");
    largeRadiusBtn->setBorderRadius(16);
    largeRadiusBtn->color(ThemeColors::BTN_PRIMARY);
    largeRadiusBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    largeRadiusBtn->labelcolor(FL_WHITE);
    largeRadiusBtn->labelsize(14);
    largeRadiusBtn->callback([](Fl_Widget *, void *) { Logger::info("Large radius button clicked"); });
    m_currentY += 50;

    RoundedButton *pillBtn = new RoundedButton(m_padding, m_currentY, 160, 40, "Pill Shape");
    pillBtn->setBorderRadius(20);
    pillBtn->color(ThemeColors::BTN_PRIMARY);
    pillBtn->selection_color(ThemeColors::BTN_PRIMARY_HOVER);
    pillBtn->labelcolor(FL_WHITE);
    pillBtn->labelsize(14);
    pillBtn->callback([](Fl_Widget *, void *) { Logger::info("Pill button clicked"); });
    m_currentY += 60;

    Fl_Box *coloredLabel = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 30, "Colored Rounded Buttons:");
    coloredLabel->labelsize(16);
    coloredLabel->labelfont(FL_BOLD);
    coloredLabel->labelcolor(ThemeColors::TEXT_NORMAL);
    coloredLabel->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    m_currentY += 40;

    RoundedButton *roundedSecondary = new RoundedButton(m_padding, m_currentY, 160, 40, "Secondary");
    roundedSecondary->setBorderRadius(8);
    roundedSecondary->color(ThemeColors::BTN_SECONDARY);
    roundedSecondary->selection_color(ThemeColors::BTN_SECONDARY_HOVER);
    roundedSecondary->labelcolor(FL_WHITE);
    roundedSecondary->labelsize(14);
    m_currentY += 50;

    RoundedButton *roundedDanger = new RoundedButton(m_padding, m_currentY, 160, 40, "Danger");
    roundedDanger->setBorderRadius(8);
    roundedDanger->color(ThemeColors::BTN_DANGER);
    roundedDanger->selection_color(ThemeColors::BTN_DANGER_HOVER);
    roundedDanger->labelcolor(FL_WHITE);
    roundedDanger->labelsize(14);
    m_currentY += 60;
}

void UILibraryScreen::addSpacing() {
    addSectionTitle("Spacing");

    const char *spacingInfo =
        "Standard spacing:\n"
        "  Small: 10px\n"
        "  Medium: 20px\n"
        "  Large: 40px\n"
        "  Section: 60px";

    Fl_Box *spacingBox = new Fl_Box(m_padding, m_currentY, w() - m_padding * 2, 120, spacingInfo);
    spacingBox->labelsize(14);
    spacingBox->labelfont(FL_COURIER);
    spacingBox->labelcolor(ThemeColors::TEXT_NORMAL);
    spacingBox->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_TOP);
    m_currentY += 140;
}

void UILibraryScreen::onCreate(const Context &ctx) { Logger::info("UI Library screen created"); }

void UILibraryScreen::onEnter(const Context &ctx) { Logger::info("Entered UI Library screen"); }

void UILibraryScreen::onTransitionIn(float progress) {
    int baseY = 0;
    int offset = static_cast<int>((1.0f - progress) * 30);
    position(x(), baseY + offset);
    redraw();
}

void UILibraryScreen::onTransitionOut(float progress) {
    int baseY = 0;
    int offset = static_cast<int>(progress * -30);
    position(x(), baseY + offset);
    redraw();
}
