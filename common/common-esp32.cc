#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include <nvs_flash.h>
#include <esp_check.h>
#include <esp_timer.h>
#include "common-esp32.hh"

int64_t millis(){
	return esp_timer_get_time() / 1000;
}

constexpr int64_t ms2ticks(int64_t ms)
{
    return (ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
}
void delayAtLeastMs(int64_t ms)
{
    vTaskDelay(ms2ticks(ms));
}

void delayMs(int64_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

esp_err_t nvs_flash_init_and_erase_lazily(const char *partition_label)
{
    esp_err_t ret = nvs_flash_init_partition(partition_label);
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase_partition(partition_label));
        ret=nvs_flash_init_partition(partition_label);
    }
    return ret;
}

esp_err_t ConfigGpioInput(gpio_num_t gpio, gpio_pull_mode_t pullMode)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = pullMode == GPIO_PULLDOWN_ONLY || pullMode == GPIO_PULLUP_PULLDOWN ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = pullMode == GPIO_PULLUP_ONLY || pullMode == GPIO_PULLUP_PULLDOWN ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 1ULL << gpio;
    return gpio_config(&io_conf);
}

esp_err_t ConfigGpioOutputOD(gpio_num_t gpio, bool pullup)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT_OD;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 1ULL << gpio;
    return gpio_config(&io_conf);
}

esp_err_t ConfigGpioOutputPP(gpio_num_t gpio)
{
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 1ULL << gpio;
    return gpio_config(&io_conf);
}