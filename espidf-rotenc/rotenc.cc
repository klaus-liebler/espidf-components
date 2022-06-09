#include "include/rotenc.hh"
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <esp_compiler.h>




esp_err_t cRotaryEncoder::Start()
{
    return pcnt_unit_start(pcnt_unit);
}

esp_err_t cRotaryEncoder::Stop()
{
    return pcnt_unit_stop(this->pcnt_unit);
}

esp_err_t cRotaryEncoder::GetValue(int *value)
{
    return pcnt_unit_get_count(this->pcnt_unit, value);
}


cRotaryEncoder::cRotaryEncoder(gpio_num_t phase_a_gpio_num, gpio_num_t phase_b_gpio_num, int minCount, int maxCount)
	:pcnt_unit(NULL), phase_a_gpio_num(phase_a_gpio_num), phase_b_gpio_num(phase_b_gpio_num), minCount(minCount), maxCount(maxCount){

}

esp_err_t cRotaryEncoder::Init()
{
	//Set up the IO state of hte pin
	gpio_reset_pin(phase_a_gpio_num);
	gpio_reset_pin(phase_b_gpio_num);
	gpio_set_direction(phase_a_gpio_num, GPIO_MODE_INPUT);
	gpio_set_direction(phase_b_gpio_num, GPIO_MODE_INPUT);
	gpio_pullup_en(phase_a_gpio_num);
	gpio_pullup_en(phase_b_gpio_num);

    pcnt_unit_config_t unit_config={};
    unit_config.high_limit = maxCount;
    unit_config.low_limit=minCount;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &this->pcnt_unit));

    pcnt_glitch_filter_config_t filter_config = {};
    filter_config.max_glitch_ns = 1000;
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {};
    chan_a_config.edge_gpio_num = phase_a_gpio_num;
    chan_a_config.level_gpio_num = phase_b_gpio_num;
    
    pcnt_channel_handle_t pcnt_chan_a=NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {};
    chan_b_config.edge_gpio_num = phase_b_gpio_num;
    chan_b_config.level_gpio_num = phase_a_gpio_num;
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
	ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    return ESP_OK;
}