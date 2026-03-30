#include "ui.hpp"
#include "montserrat_bold_digits.h"

#include "lvgl.h"

#include <cstdio>

static void SetLabel(lv_obj_t* label, const char* fmt, float val)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), fmt, val);
    lv_label_set_text(label, buf);
}

static constexpr size_t kSeverityCount = 4;

static size_t Pm25Severity(float v)
{
    if (v <= 12.f)  return 0;
    if (v <= 35.4f) return 1;
    if (v <= 55.4f) return 2;
    return 3;
}

static const lv_color_t kSeverityColors[kSeverityCount] = {
    lv_color_hex(0x2E7D32),  // good — green
    lv_color_hex(0xF9A825),  // moderate — amber
    lv_color_hex(0xE65100),  // unhealthy for sensitive groups — orange
    lv_color_hex(0xC62828),  // unhealthy — red
};

constexpr int32_t kPm25FontSize = 96;

struct Ui::Impl {
    lv_font_t* big_font{};
    lv_obj_t* scr{};
    lv_obj_t* pm25_label{};
    lv_obj_t* pm25_name{};
    lv_obj_t* pm1_label{};
    lv_obj_t* pm4_label{};
    lv_obj_t* pm10_label{};
    lv_obj_t* temp_label{};
    lv_obj_t* status_label{};
    size_t last_severity{SIZE_MAX};  // sentinel forces first update to set bg color

    // Creates a row: value (left, Montserrat 20) | name (right, Montserrat 14 muted)
    static lv_obj_t* MakeSecondaryRow(lv_obj_t* parent, const char* name)
    {
        auto* row = lv_obj_create(parent);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);

        auto* val_lbl = lv_label_create(row);
        lv_label_set_text(val_lbl, "--");
        lv_obj_set_style_text_font(val_lbl, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(val_lbl, lv_color_white(), 0);

        auto* name_lbl = lv_label_create(row);
        lv_label_set_text(name_lbl, name);
        lv_obj_set_style_text_font(name_lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(name_lbl, lv_color_white(), 0);
        lv_obj_set_style_text_opa(name_lbl, LV_OPA_70, 0);

        return val_lbl;
    }
};

Ui::Ui() : impl_(std::make_unique<Impl>())
{
    impl_->big_font = lv_tiny_ttf_create_data(montserrat_bold_digits_ttf,
                                               montserrat_bold_digits_ttf_len,
                                               kPm25FontSize);

    impl_->scr = lv_screen_active();
    lv_obj_set_style_bg_color(impl_->scr, kSeverityColors[0], 0);
    lv_obj_set_style_bg_opa(impl_->scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(impl_->scr, 4, 0);
    lv_obj_clear_flag(impl_->scr, LV_OBJ_FLAG_SCROLLABLE);

    // Grid: left column stretches, right column fixed to fit "999 PM10"
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
    lv_obj_set_style_text_font(impl_->pm25_label, impl_->big_font, 0);
    lv_obj_set_style_text_color(impl_->pm25_label, lv_color_white(), 0);
    lv_obj_center(impl_->pm25_label);

    impl_->pm25_name = lv_label_create(left);
    lv_label_set_text(impl_->pm25_name, "PM 2.5");
    lv_obj_set_style_text_font(impl_->pm25_name, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(impl_->pm25_name, lv_color_white(), 0);
    lv_obj_set_style_text_opa(impl_->pm25_name, LV_OPA_70, 0);
    lv_obj_align(impl_->pm25_name, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    // Right cell: compact secondary readings
    auto* right = lv_obj_create(impl_->scr);
    lv_obj_remove_style_all(right);
    lv_obj_set_grid_cell(right, LV_GRID_ALIGN_END, 1, 1,
                                LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_row(right, 2, 0);
    lv_obj_set_width(right, 80);

    impl_->pm1_label  = Impl::MakeSecondaryRow(right, "PM1");
    impl_->pm4_label  = Impl::MakeSecondaryRow(right, "PM4");
    impl_->pm10_label = Impl::MakeSecondaryRow(right, "PM10");
    impl_->temp_label = Impl::MakeSecondaryRow(right, "\xC2\xB0""C");

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

Ui::~Ui()
{
    if (impl_->big_font) {
        lv_tiny_ttf_destroy(impl_->big_font);
    }
}

void Ui::UpdateMeasurements(const Sen55::Measurement& data)
{
    SetLabel(impl_->pm25_label, "%.1f", data.pm2_5);
    SetLabel(impl_->pm1_label,  "%.0f", data.pm1_0);
    SetLabel(impl_->pm4_label,  "%.0f", data.pm4_0);
    SetLabel(impl_->pm10_label, "%.0f", data.pm10);
    SetLabel(impl_->temp_label, "%.0f", data.temperature);

    auto level = Pm25Severity(data.pm2_5);
    if (level != impl_->last_severity) {
        lv_obj_set_style_bg_color(impl_->scr, kSeverityColors[level], 0);
        impl_->last_severity = level;
    }
}

void Ui::SetStatus(std::string_view text)
{
    lv_label_set_text_fmt(impl_->status_label, "%.*s",
                          static_cast<int>(text.size()), text.data());
}
