#include "include/rotenc.hh"
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include <esp_compiler.h>
#include <limits.h>




esp_err_t cRotaryEncoder::Start()
{
    return pcnt_counter_resume(this->pcnt_unit);
    //return pcnt_unit_start(this->pcnt_unit);
}

esp_err_t cRotaryEncoder::Stop()
{
    return pcnt_counter_pause(this->pcnt_unit);
    //return pcnt_unit_stop(this->pcnt_unit);
}

esp_err_t cRotaryEncoder::GetValue(int *value)
{
    int16_t tmp;
    pcnt_get_counter_value(this->pcnt_unit, &tmp);
    *value=tmp;
    return ESP_OK;
    //return pcnt_unit_get_count(this->pcnt_unit, value);
}


cRotaryEncoder::cRotaryEncoder(gpio_num_t phase_a_gpio_num, gpio_num_t phase_b_gpio_num, int16_t minCount, int16_t maxCount)
	:/*pcnt_unit(NULL)*/pcnt_unit(PCNT_UNIT_0), phase_a_gpio_num(phase_a_gpio_num), phase_b_gpio_num(phase_b_gpio_num), minCount(minCount), maxCount(maxCount){

}

esp_err_t cRotaryEncoder::Init()
{
	gpio_reset_pin(phase_a_gpio_num);
	gpio_reset_pin(phase_b_gpio_num);
	gpio_set_direction(phase_a_gpio_num, GPIO_MODE_INPUT);
	gpio_set_direction(phase_b_gpio_num, GPIO_MODE_INPUT);
	gpio_pullup_en(phase_a_gpio_num);
	gpio_pullup_en(phase_b_gpio_num);
/*
    pcnt_unit_config_t unit_config={};
    unit_config.high_limit = maxCount;
    unit_config.low_limit  = minCount;
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
*/
    pcnt_config_t dev_config = {};
    dev_config.pulse_gpio_num = phase_a_gpio_num;
    dev_config.ctrl_gpio_num = phase_b_gpio_num;
    dev_config.channel = PCNT_CHANNEL_0;
    dev_config.unit = pcnt_unit;
    dev_config.pos_mode = PCNT_COUNT_DEC;
    dev_config.neg_mode = PCNT_COUNT_INC;
    dev_config.lctrl_mode = PCNT_MODE_REVERSE;
    dev_config.hctrl_mode = PCNT_MODE_KEEP;
    dev_config.counter_h_lim = maxCount;
    dev_config.counter_l_lim = minCount;
    ESP_ERROR_CHECK(pcnt_unit_config(&dev_config));

    dev_config.pulse_gpio_num = phase_b_gpio_num;
    dev_config.ctrl_gpio_num = phase_a_gpio_num;
    dev_config.channel = PCNT_CHANNEL_1;
    dev_config.pos_mode = PCNT_COUNT_INC;
    dev_config.neg_mode = PCNT_COUNT_DEC;
    ESP_ERROR_CHECK(pcnt_unit_config(&dev_config));

    // PCNT pause and reset value
    ESP_ERROR_CHECK(pcnt_counter_pause(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_counter_clear(pcnt_unit));

    /* Configure and enable the input filter */
    int max_glitch_us=1;
    ESP_ERROR_CHECK(pcnt_set_filter_value(pcnt_unit, max_glitch_us * 80));
    ESP_ERROR_CHECK(pcnt_filter_enable(pcnt_unit));
    
    /*
    pcnt_glitch_filter_config_t filter_config = {};
    filter_config.max_glitch_ns = 1000;
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    pcnt_chan_config_t chan_a_config = {};
    chan_a_config.edge_gpio_num = phase_a_gpio_num;
    chan_a_config.level_gpio_num = phase_b_gpio_num;
    chan_a_config.flags.invert_edge_input=0;
    chan_a_config.flags.invert_level_input=0;
    chan_a_config.flags.io_loop_back=0;
    pcnt_channel_handle_t pcnt_chan_a=NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    
    pcnt_chan_config_t chan_b_config = {};
    chan_b_config.edge_gpio_num = phase_b_gpio_num;
    chan_b_config.level_gpio_num = phase_a_gpio_num;
    chan_b_config.flags.invert_edge_input=0;
    chan_b_config.flags.invert_level_input=0;
    chan_b_config.flags.io_loop_back=0;
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
	ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    */
    return ESP_OK;
}