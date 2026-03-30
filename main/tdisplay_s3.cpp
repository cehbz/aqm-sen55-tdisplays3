#include "tdisplay_s3.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lvgl_port.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

// ESP-IDF config structs have many fields we intentionally leave defaulted.
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

namespace tdisplay_s3 {
namespace {

const char* TAG = "tdisplay_s3";

/* ── T-Display S3 pin assignments ───────────────────────────────────── */

constexpr auto kPinPower = GPIO_NUM_15;   // Peripheral power enable
constexpr auto kPinBl    = GPIO_NUM_38;   // Backlight
constexpr auto kPinWr    = GPIO_NUM_8;    // i80 write clock
constexpr auto kPinDc    = GPIO_NUM_7;    // Data/Command
constexpr auto kPinCs    = GPIO_NUM_6;    // Chip select
constexpr auto kPinRst   = GPIO_NUM_5;    // Reset
constexpr auto kPinRd    = GPIO_NUM_9;    // Read strobe (active low, must be held HIGH)

constexpr gpio_num_t kDataPins[8] = {
    GPIO_NUM_39, GPIO_NUM_40, GPIO_NUM_41, GPIO_NUM_42,
    GPIO_NUM_45, GPIO_NUM_46, GPIO_NUM_47, GPIO_NUM_48,
};

constexpr uint32_t kPclkHz  = 20'000'000;  // 20 MHz i80 clock
constexpr uint32_t kBufRows = 40;          // Draw buffer height in rows

/* ── Helpers ────────────────────────────────────────────────────────── */

void SetGpioHigh(gpio_num_t pin)
{
    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(pin, 1);
}

esp_err_t InitLcd(esp_lcd_panel_io_handle_t& out_io,
                  esp_lcd_panel_handle_t& out_panel)
{
    esp_lcd_i80_bus_config_t bus_cfg = {
        .dc_gpio_num = kPinDc,
        .wr_gpio_num = kPinWr,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            kDataPins[0], kDataPins[1], kDataPins[2], kDataPins[3],
            kDataPins[4], kDataPins[5], kDataPins[6], kDataPins[7],
        },
        .bus_width = 8,
        .max_transfer_bytes = kHRes * kBufRows * sizeof(uint16_t),
        .dma_burst_size = 64,
    };
    esp_lcd_i80_bus_handle_t i80_bus{};
    ESP_RETURN_ON_ERROR(esp_lcd_new_i80_bus(&bus_cfg, &i80_bus),
                        TAG, "i80 bus create failed");

    esp_lcd_panel_io_i80_config_t io_cfg = {
        .cs_gpio_num = kPinCs,
        .pclk_hz = kPclkHz,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .dc_levels = {
            .dc_cmd_level = 0,
            .dc_data_level = 1,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i80(i80_bus, &io_cfg, &out_io),
                        TAG, "Panel IO create failed");

    esp_lcd_panel_dev_config_t panel_cfg = {
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .reset_gpio_num = kPinRst,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(out_io, &panel_cfg, &out_panel),
                        TAG, "ST7789 panel create failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(out_panel), TAG, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(out_panel), TAG, "Panel init failed");

    // ST7789V requires color inversion for correct colours
    esp_lcd_panel_invert_color(out_panel, true);

    // Gap: after swap_xy the 35px offset moves from X to Y
    esp_lcd_panel_set_gap(out_panel, 0, 35);

    // ST7789 init sends sleep-out but not DISPON — display stays off without this
    esp_lcd_panel_disp_on_off(out_panel, true);

    ESP_LOGI(TAG, "ST7789V panel initialised (%lux%lu landscape)", kHRes, kVRes);
    return ESP_OK;
}

} // anonymous namespace

esp_err_t Init()
{
    // GPIO 15 must be HIGH before any display or ADC access
    SetGpioHigh(kPinPower);
    // RD must be HIGH (inactive) or the ST7789V drives the data bus, causing contention
    SetGpioHigh(kPinRd);
    vTaskDelay(pdMS_TO_TICKS(20));

    esp_lcd_panel_io_handle_t io_handle{};
    esp_lcd_panel_handle_t lcd_panel{};
    ESP_RETURN_ON_ERROR(InitLcd(io_handle, lcd_panel), TAG, "LCD init failed");

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port init failed");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = lcd_panel,
        .buffer_size = kHRes * kBufRows,
        .double_buffer = true,
        .hres = kHRes,
        .vres = kVRes,
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
            .swap_bytes = true,
        },
    };
    auto* disp = lvgl_port_add_disp(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "Failed to add display to LVGL port");
        return ESP_FAIL;
    }

    SetGpioHigh(kPinBl);
    ESP_LOGI(TAG, "Display initialised (%lux%lu)", kHRes, kVRes);
    return ESP_OK;
}

} // namespace tdisplay_s3
