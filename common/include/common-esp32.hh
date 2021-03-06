#include <driver/gpio.h>
#include <esp_check.h>
#include "common.hh"

#define TAG "COMMON"

namespace esp32{


    esp_err_t ConfigGpioInput(gpio_num_t gpio, gpio_pull_mode_t pullMode){
        gpio_config_t io_conf = {};
        io_conf.mode= GPIO_MODE_INPUT;
        io_conf.pull_down_en = pullMode==GPIO_PULLDOWN_ONLY || pullMode==GPIO_PULLUP_PULLDOWN?GPIO_PULLDOWN_ENABLE:GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = pullMode==GPIO_PULLUP_ONLY || pullMode==GPIO_PULLUP_PULLDOWN?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = 1ULL<<gpio;
        return gpio_config(&io_conf);
    }

    esp_err_t ConfigGpioOutputOD(gpio_num_t gpio, bool pullup){
        gpio_config_t io_conf = {};
        io_conf.mode= GPIO_MODE_OUTPUT_OD;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = pullup?GPIO_PULLUP_ENABLE:GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = 1ULL<<gpio;
        return gpio_config(&io_conf);
    }

    esp_err_t ConfigGpioOutputPP(gpio_num_t gpio){
        gpio_config_t io_conf = {};
        io_conf.mode= GPIO_MODE_OUTPUT;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pin_bit_mask = 1ULL<<gpio;
        return gpio_config(&io_conf);
    }
}
#undef TAG