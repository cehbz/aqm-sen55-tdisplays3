#include "ui.hpp"

#include "lvgl.h"

#include <cstdio>

LV_FONT_DECLARE(montserrat_bold_digits);

static void SetLabel(lv_obj_t* label, char* buf, size_t buf_len, const char* fmt, float val)
{
    std::snprintf(buf, buf_len, fmt, val);
    lv_label_set_text_static(label, buf);
}

static constexpr size_t kSeverityCount = 4;

static const lv_color_t kSeverityColors[kSeverityCount] = {
    lv_color_hex(0x47A33F),  // good — spring green (hue ~115°)
    lv_color_hex(0xB0A341),  // moderate — goldenrod (hue ~53°)
    lv_color_hex(0xD4693B),  // unhealthy for sensitive groups — burnt sienna (hue ~18°)
    lv_color_hex(0xE0263C),  // unhealthy — crimson (hue ~353°)
};

struct Thresholds { float good; float moderate; float usg; };

static size_t PmSeverity(float v, const Thresholds& t)
{
    if (v <= t.good)     return 0;
    if (v <= t.moderate) return 1;
    if (v <= t.usg)      return 2;
    return 3;
}

static constexpr Thresholds kPm25Thresholds = {12.f, 35.4f, 55.4f};
static constexpr Thresholds kPm1Thresholds  = {12.f, 35.f,  55.f};
static constexpr Thresholds kPm4Thresholds  = {25.f, 50.f,  75.f};
static constexpr Thresholds kPm10Thresholds = {54.f, 154.f, 254.f};

// Comfort range — two-sided, can't use threshold ladder
static size_t TempSeverity(float v)
{
    if (v >= 18.f && v <= 24.f) return 0;
    if (v >= 15.f && v <= 28.f) return 1;
    if (v >= 10.f && v <= 32.f) return 2;
    return 3;
}

struct Ui::Impl {
    static constexpr size_t kBufLen = 16;

    struct SecondaryPanel {
        lv_obj_t* row{};
        lv_obj_t* value_label{};
        char buf[kBufLen]{};
        size_t last_severity{SIZE_MAX};
    };

    lv_obj_t* scr{};
    lv_obj_t* pm25_label{};
    char pm25_buf[kBufLen]{};
    lv_obj_t* pm25_name{};
    SecondaryPanel pm1{};
    SecondaryPanel pm4{};
    SecondaryPanel pm10{};
    SecondaryPanel temp{};
    lv_obj_t* status_label{};
    size_t last_pm25_severity{SIZE_MAX};

    static SecondaryPanel MakeSecondaryRow(lv_obj_t* parent, const char* name)
    {
        SecondaryPanel panel;

        panel.row = lv_obj_create(parent);
        lv_obj_remove_style_all(panel.row);
        lv_obj_set_size(panel.row, LV_PCT(100), LV_PCT(100));
        lv_obj_set_flex_grow(panel.row, 1);
        lv_obj_set_flex_flow(panel.row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(panel.row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(panel.row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(panel.row, kSeverityColors[0], 0);
        lv_obj_set_style_radius(panel.row, 4, 0);
        lv_obj_set_style_pad_hor(panel.row, 4, 0);
        lv_obj_set_style_pad_ver(panel.row, 2, 0);

        panel.value_label = lv_label_create(panel.row);
        lv_label_set_text(panel.value_label, "--");
        lv_obj_set_style_text_font(panel.value_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(panel.value_label, lv_color_white(), 0);

        auto* name_lbl = lv_label_create(panel.row);
        lv_label_set_text(name_lbl, name);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_opa(name_lbl, LV_OPA_70, 0);

        return panel;
    }

    static void UpdatePanel(SecondaryPanel& p, float val, const char* fmt,
                            const Thresholds& thresholds)
    {
        SetLabel(p.value_label, p.buf, kBufLen, fmt, val);
        auto level = PmSeverity(val, thresholds);
        if (level != p.last_severity) {
            lv_obj_set_style_bg_color(p.row, kSeverityColors[level], 0);
            p.last_severity = level;
        }
    }

    static void UpdateTempPanel(SecondaryPanel& p, float val)
    {
        SetLabel(p.value_label, p.buf, kBufLen, "%.0f", val);
        auto level = TempSeverity(val);
        if (level != p.last_severity) {
            lv_obj_set_style_bg_color(p.row, kSeverityColors[level], 0);
            p.last_severity = level;
        }
    }
};

Ui::Ui()
{
    static Impl impl;
    impl_ = &impl;
    impl_->scr = lv_screen_active();
    lv_obj_set_style_bg_color(impl_->scr, kSeverityColors[0], 0);
    lv_obj_set_style_bg_opa(impl_->scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(impl_->scr, 4, 0);
    lv_obj_clear_flag(impl_->scr, LV_OBJ_FLAG_SCROLLABLE);

    static const int32_t col_dsc[] = {LV_GRID_FR(1), 80, LV_GRID_TEMPLATE_LAST};
    static const int32_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    lv_obj_set_layout(impl_->scr, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(impl_->scr, col_dsc, row_dsc);
    lv_obj_set_style_pad_column(impl_->scr, 2, 0);

    // Left cell: PM2.5 value centered, label at bottom-left
    auto* left = lv_obj_create(impl_->scr);
    lv_obj_remove_style_all(left);
    lv_obj_set_grid_cell(left, LV_GRID_ALIGN_STRETCH, 0, 1,
                               LV_GRID_ALIGN_STRETCH, 0, 1);

    impl_->pm25_label = lv_label_create(left);
    lv_label_set_text(impl_->pm25_label, "--");
    lv_obj_set_style_text_font(impl_->pm25_label, &montserrat_bold_digits, 0);
    lv_obj_set_style_text_color(impl_->pm25_label, lv_color_white(), 0);
    lv_obj_center(impl_->pm25_label);

    impl_->pm25_name = lv_label_create(left);
    lv_label_set_text(impl_->pm25_name, "PM 2.5");
    lv_obj_set_style_text_font(impl_->pm25_name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(impl_->pm25_name, lv_color_white(), 0);
    lv_obj_set_style_text_opa(impl_->pm25_name, LV_OPA_70, 0);
    lv_obj_align(impl_->pm25_name, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    // Right cell: compact secondary readings with per-metric severity colors
    auto* right = lv_obj_create(impl_->scr);
    lv_obj_remove_style_all(right);
    lv_obj_set_grid_cell(right, LV_GRID_ALIGN_END, 1, 1,
                                LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(right, 1, 0);
    lv_obj_set_width(right, 80);

    impl_->pm1  = Impl::MakeSecondaryRow(right, "PM1");
    impl_->pm4  = Impl::MakeSecondaryRow(right, "PM4");
    impl_->pm10 = Impl::MakeSecondaryRow(right, "PM10");
    impl_->temp = Impl::MakeSecondaryRow(right, "\xC2\xB0""C");

    // Status — bottom-left of screen, outside grid
    impl_->status_label = lv_label_create(impl_->scr);
    lv_label_set_text(impl_->status_label, "");
    lv_obj_set_style_text_font(impl_->status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(impl_->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(impl_->status_label, LV_OPA_50, 0);
    lv_obj_set_style_align(impl_->status_label, LV_ALIGN_BOTTOM_LEFT, 0);
    lv_obj_set_style_pad_left(impl_->status_label, 4, 0);
    lv_obj_set_style_pad_bottom(impl_->status_label, 2, 0);
    lv_obj_remove_flag(impl_->status_label, LV_OBJ_FLAG_LAYOUT_1);
    lv_obj_remove_flag(impl_->status_label, LV_OBJ_FLAG_LAYOUT_2);

    lv_obj_invalidate(impl_->scr);
}

Ui::~Ui() {}  // impl_ points to static storage, nothing to free

void Ui::UpdateMeasurements(const Sen55::Measurement& data)
{
    SetLabel(impl_->pm25_label, impl_->pm25_buf, Impl::kBufLen, "%.1f", data.pm2_5);
    Impl::UpdatePanel(impl_->pm1,  data.pm1_0, "%.0f", kPm1Thresholds);
    Impl::UpdatePanel(impl_->pm4,  data.pm4_0, "%.0f", kPm4Thresholds);
    Impl::UpdatePanel(impl_->pm10, data.pm10,  "%.0f", kPm10Thresholds);
    Impl::UpdateTempPanel(impl_->temp, data.temperature);

    auto level = PmSeverity(data.pm2_5, kPm25Thresholds);
    if (level != impl_->last_pm25_severity) {
        lv_obj_set_style_bg_color(impl_->scr, kSeverityColors[level], 0);
        impl_->last_pm25_severity = level;
    }
}

void Ui::SetStatus(const char* text)
{
    lv_label_set_text(impl_->status_label, text);
}
