#pragma once

#include "esp_err.h"

#include <cstdint>

namespace tdisplay_s3 {

// Landscape orientation (rotation applied via lvgl_port_display_cfg_t.rotation).
constexpr uint32_t kHRes = 320;
constexpr uint32_t kVRes = 170;

/**
 * Initialise the ST7789V i80 LCD panel and LVGL port in landscape.
 * After this returns the LVGL task is running and the display is ready.
 */
[[nodiscard]] esp_err_t Init();

} // namespace tdisplay_s3
