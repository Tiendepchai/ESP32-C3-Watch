#include "power_manager.h"

#include <string.h>
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = BOARD_TAG_POWER;

typedef struct {
    power_manager_config_t cfg;
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;
    bool cali_ready;
    SemaphoreHandle_t lock;
    power_state_t state;
    TaskHandle_t task;
} power_manager_ctx_t;

static power_manager_ctx_t s_pm;

static esp_err_t power_manager_init_adc(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BOARD_BATTERY_ADC_UNIT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_pm.adc_handle), TAG, "adc unit init failed");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_pm.adc_handle, BOARD_BATTERY_ADC_CHANNEL, &chan_cfg),
                        TAG, "adc channel config failed");

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BOARD_BATTERY_ADC_UNIT,
        .chan = BOARD_BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t rc = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_pm.cali_handle);
    if (rc == ESP_OK) {
        s_pm.cali_ready = true;
    } else {
        ESP_LOGW(TAG, "ADC calibration unavailable, falling back to raw->mv approx (%s)", esp_err_to_name(rc));
        s_pm.cali_ready = false;
    }
    return ESP_OK;
}

static esp_err_t power_manager_init_stat_pins(void)
{
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << BOARD_TP4056_CHRG_GPIO) | (1ULL << BOARD_TP4056_STDBY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_cfg);
}

static uint16_t power_manager_sample_battery_mv(void)
{
    int raw_sum = 0;
    int mv_sum = 0;
    int valid_samples = 0;

    for (int i = 0; i < BOARD_BATTERY_SAMPLE_COUNT; ++i) {
        int raw = 0;
        if (adc_oneshot_read(s_pm.adc_handle, BOARD_BATTERY_ADC_CHANNEL, &raw) != ESP_OK) {
            continue;
        }
        raw_sum += raw;
        if (s_pm.cali_ready) {
            int mv = 0;
            if (adc_cali_raw_to_voltage(s_pm.cali_handle, raw, &mv) == ESP_OK) {
                mv_sum += mv;
                valid_samples++;
            }
        }
    }

    int adc_mv;
    if (valid_samples > 0) {
        adc_mv = mv_sum / valid_samples;
    } else {
        /* Fallback: 12-bit ADC, ATTEN_DB_12 ≈ 0..3100mV. */
        int avg_raw = raw_sum / BOARD_BATTERY_SAMPLE_COUNT;
        adc_mv = (avg_raw * 3100) / 4095;
    }

    int battery_mv = (adc_mv * BOARD_BATTERY_DIVIDER_NUM) / BOARD_BATTERY_DIVIDER_DEN;
    if (battery_mv < 0) battery_mv = 0;
    if (battery_mv > 65535) battery_mv = 65535;
    return (uint16_t)battery_mv;
}

static uint8_t power_manager_voltage_to_percent(uint16_t mv)
{
    /* Piecewise linear LiPo discharge curve, single-cell 3.7V nominal. */
    static const struct { uint16_t mv; uint8_t pct; } curve[] = {
        { 4200, 100 },
        { 4100,  90 },
        { 4000,  80 },
        { 3900,  65 },
        { 3800,  45 },
        { 3700,  25 },
        { 3600,  12 },
        { 3500,   5 },
        { 3400,   2 },
        { BOARD_POWER_BATTERY_EMPTY_MV, 0 },
    };

    if (mv >= curve[0].mv) return 100;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (mv >= curve[i].mv) {
            uint16_t mv_hi = curve[i - 1U].mv;
            uint16_t mv_lo = curve[i].mv;
            uint8_t pct_hi = curve[i - 1U].pct;
            uint8_t pct_lo = curve[i].pct;
            uint32_t span_mv = (uint32_t)(mv_hi - mv_lo);
            uint32_t span_pct = (uint32_t)(pct_hi - pct_lo);
            uint32_t pct = pct_lo + (((uint32_t)(mv - mv_lo) * span_pct) / span_mv);
            if (pct > 100U) pct = 100U;
            return (uint8_t)pct;
        }
    }
    return 0;
}

static power_source_t power_manager_read_source(void)
{
    int chrg = gpio_get_level(BOARD_TP4056_CHRG_GPIO);
    int stdby = gpio_get_level(BOARD_TP4056_STDBY_GPIO);

    if (chrg == 0 && stdby != 0) {
        return POWER_SOURCE_CHARGING;
    }
    if (chrg != 0 && stdby == 0) {
        return POWER_SOURCE_CHARGED;
    }
    if (chrg != 0 && stdby != 0) {
        return POWER_SOURCE_BATTERY;
    }
    return POWER_SOURCE_UNKNOWN;
}

static bool power_state_significantly_changed(const power_state_t *prev, const power_state_t *next)
{
    if (prev->source != next->source) return true;
    if (prev->low_battery != next->low_battery) return true;
    if (prev->critical_battery != next->critical_battery) return true;
    int delta = (int)next->battery_pct - (int)prev->battery_pct;
    if (delta < 0) delta = -delta;
    return delta >= 2;
}

static void power_manager_task(void *arg)
{
    power_state_t prev = {0};
    bool first = true;

    while (true) {
        power_state_t next = {0};
        next.battery_mv = power_manager_sample_battery_mv();
        next.battery_pct = power_manager_voltage_to_percent(next.battery_mv);
        next.source = power_manager_read_source();
        next.low_battery = (next.battery_pct < BOARD_POWER_BATTERY_LOW_PCT) &&
                           (next.source == POWER_SOURCE_BATTERY);
        next.critical_battery = (next.battery_pct < BOARD_POWER_BATTERY_CRITICAL_PCT) &&
                                (next.source == POWER_SOURCE_BATTERY);

        xSemaphoreTake(s_pm.lock, portMAX_DELAY);
        s_pm.state = next;
        xSemaphoreGive(s_pm.lock);

        if (first || power_state_significantly_changed(&prev, &next)) {
            ESP_LOGI(TAG, "battery=%umV (%u%%) source=%s%s%s",
                     (unsigned)next.battery_mv, (unsigned)next.battery_pct,
                     power_source_to_string(next.source),
                     next.low_battery ? " LOW" : "",
                     next.critical_battery ? " CRITICAL" : "");
            if (s_pm.cfg.state_cb != NULL) {
                s_pm.cfg.state_cb(&next, s_pm.cfg.user_ctx);
            }
            prev = next;
            first = false;
        }

        vTaskDelay(pdMS_TO_TICKS(BOARD_POWER_SAMPLE_MS));
    }
}

esp_err_t power_manager_init(const power_manager_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is null");
    if (s_pm.task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&s_pm, 0, sizeof(s_pm));
    s_pm.cfg = *config;
    s_pm.state.source = POWER_SOURCE_UNKNOWN;
    s_pm.lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_pm.lock != NULL, ESP_ERR_NO_MEM, TAG, "lock alloc failed");

    ESP_RETURN_ON_ERROR(power_manager_init_adc(), TAG, "ADC init failed");
    ESP_RETURN_ON_ERROR(power_manager_init_stat_pins(), TAG, "STAT pins init failed");

    BaseType_t ok = xTaskCreate(power_manager_task, "pwr_mgr", 3072, NULL, 4, &s_pm.task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task create failed");

    ESP_LOGI(TAG, "power manager started, ADC GPIO=%d CHRG=%d STDBY=%d period=%dms",
             BOARD_BATTERY_ADC_GPIO, BOARD_TP4056_CHRG_GPIO, BOARD_TP4056_STDBY_GPIO,
             BOARD_POWER_SAMPLE_MS);
    return ESP_OK;
}

void power_manager_get_state(power_state_t *out_state)
{
    if (out_state == NULL) return;
    if (s_pm.lock == NULL) {
        memset(out_state, 0, sizeof(*out_state));
        out_state->source = POWER_SOURCE_UNKNOWN;
        return;
    }
    xSemaphoreTake(s_pm.lock, portMAX_DELAY);
    *out_state = s_pm.state;
    xSemaphoreGive(s_pm.lock);
}

const char *power_source_to_string(power_source_t source)
{
    switch (source) {
    case POWER_SOURCE_BATTERY:  return "battery";
    case POWER_SOURCE_CHARGING: return "charging";
    case POWER_SOURCE_CHARGED:  return "charged";
    default:                    return "unknown";
    }
}
