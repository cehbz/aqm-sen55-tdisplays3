#include "tdisplay_s3.hpp"
#include "sen55.hpp"
#include "ui.hpp"
#include "device_id.hpp"
#include "wifi.hpp"
#include "mqtt.hpp"
#include "ha_discovery.hpp"

#include "credentials.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif_sntp.h"
#include "driver/i2c_master.h"

#include <cstdio>
#include <ctime>

namespace {

const char *TAG = "main";
constexpr auto kSen55Sda = GPIO_NUM_17;
constexpr auto kSen55Scl = GPIO_NUM_18;

void publish_sen55(const Sen55::Measurement &m)
{
    if (!mqtt_is_connected()) return;

    const char *id = device_id_get();
    char topic[64];
    char value[16];

    struct { float val; const char *fmt; } fields[kSen55EntityCount] = {
        {m.pm1_0,       "%.1f"},
        {m.pm2_5,       "%.1f"},
        {m.pm4_0,       "%.1f"},
        {m.pm10,        "%.1f"},
        {m.temperature, "%.1f"},
        {m.humidity,    "%.1f"},
        {m.voc_index,   "%.0f"},
        {m.nox_index,   "%.0f"},
    };

    for (size_t i = 0; i < kSen55EntityCount; ++i) {
        std::snprintf(topic, sizeof(topic), "aqm/%s/sensor/%s", id, kSen55Entities[i]);
        std::snprintf(value, sizeof(value), fields[i].fmt, fields[i].val);
        mqtt_publish(topic, value);
    }
}

void on_mqtt_connect()
{
    ha_discovery_publish_sen55(device_id_get());
}

void on_mqtt_data(const char * /*topic*/, int /*topic_len*/,
                  const char * /*data*/, int /*data_len*/)
{
}

} // namespace

extern "C" void app_main()
{
    ESP_ERROR_CHECK(tdisplay_s3::Init());

    device_id_init();
    ESP_LOGI(TAG, "Device ID: %s", device_id_get());

    i2c_master_bus_config_t bus_cfg{};
    bus_cfg.i2c_port = I2C_NUM_1;
    bus_cfg.sda_io_num = kSen55Sda;
    bus_cfg.scl_io_num = kSen55Scl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    i2c_master_bus_handle_t sen55_bus{};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &sen55_bus));

    static Ui *ui = nullptr;
    if (lvgl_port_lock(0)) {
        static auto ui_obj = Ui();
        ui = &ui_obj;
        lvgl_port_unlock();
    }
    if (!ui) {
        ESP_LOGE(TAG, "LVGL port lock failed during UI init");
        abort();
    }

    static auto sensor = Sen55(sen55_bus, [](const Sen55::Measurement &m) {
        auto now = std::time(nullptr);
        struct tm tm_buf{};
        auto *tm = localtime_r(&now, &tm_buf);
        char ts[32];
        std::snprintf(ts, sizeof(ts), "Updated %02d:%02d:%02d",
                      tm->tm_hour, tm->tm_min, tm->tm_sec);

        if (lvgl_port_lock(0)) {
            ui->UpdateMeasurements(m);
            ui->SetStatus(ts);
            lvgl_port_unlock();
        }

        publish_sen55(m);
    });

    wifi_init();
    if (wifi_wait_connected(15000)) {
        ESP_LOGI(TAG, "WiFi connected");

        setenv("TZ", TIMEZONE, 1);
        tzset();
        esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        esp_err_t sntp_err = esp_netif_sntp_init(&sntp_cfg);
        if (sntp_err == ESP_OK) {
            ESP_LOGI(TAG, "SNTP started, TZ=%s", TIMEZONE);
        } else {
            ESP_LOGE(TAG, "SNTP init failed: %s", esp_err_to_name(sntp_err));
        }
    } else {
        ESP_LOGW(TAG, "WiFi not connected yet — will auto-reconnect");
    }

    mqtt_init(device_id_get(), on_mqtt_connect, on_mqtt_data);
    ESP_LOGI(TAG, "Air quality monitor running (gateway planned)");
}
