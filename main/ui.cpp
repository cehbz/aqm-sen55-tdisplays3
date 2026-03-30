#include "ui.hpp"

#include "lvgl.h"

#include <cstdio>

static constexpr size_t kSeverityCount = 4;

static size_t Pm25Severity(float v)
{
    if (v <= 12.f)  return 0;  // Good
    if (v <= 35.4f) return 1;  // Moderate
    if (v <= 55.4f) return 2;  // Unhealthy for sensitive groups
    return 3;                  // Unhealthy
}

static const lv_color_t kSeverityColors[kSeverityCount] = {
    lv_color_hex(0x2E7D32),  // good — green
    lv_color_hex(0xF9A825),  // moderate — amber
    lv_color_hex(0xE65100),  // usg — orange
    lv_color_hex(0xC62828),  // unhealthy — red
};

struct Ui::Impl {
    lv_obj_t* scr{};
    lv_obj_t* value_label{};
    lv_obj_t* status_label{};
    size_t last_severity{0};
};

Ui::Ui() : impl_(std::make_unique<Impl>())
{
    impl_->scr = lv_screen_active();
    lv_obj_set_style_bg_color(impl_->scr, kSeverityColors[0], 0);
    lv_obj_set_style_bg_opa(impl_->scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(impl_->scr, LV_OBJ_FLAG_SCROLLABLE);

    impl_->value_label = lv_label_create(impl_->scr);
    lv_label_set_text(impl_->value_label, "--");
    lv_obj_set_style_text_font(impl_->value_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(impl_->value_label, lv_color_white(), 0);
    lv_obj_center(impl_->value_label);

    impl_->status_label = lv_label_create(impl_->scr);
    lv_label_set_text(impl_->status_label, "");
    lv_obj_set_style_text_font(impl_->status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(impl_->status_label, lv_color_white(), 0);
    lv_obj_set_style_text_opa(impl_->status_label, LV_OPA_50, 0);
    lv_obj_align(impl_->status_label, LV_ALIGN_BOTTOM_RIGHT, -4, -4);

    lv_obj_invalidate(impl_->scr);
}

Ui::~Ui() = default;

void Ui::UpdateMeasurements(const Sen55::Measurement& data)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f", data.pm2_5);
    lv_label_set_text(impl_->value_label, buf);

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
