#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sync_pwm.h"
#define EXAMPLE_SYNC_GPIO 5 // GPIO used for sync source

const static char *TAG = "SYNC_PWM";

void example_setup_sync_strategy(mcpwm_timer_handle_t timers[])
{
    //    +----GPIO----+
    //    |     |      |
    //    |     |      |
    //    v     v      v
    // timer0 timer1 timer2
    gpio_config_t sync_gpio_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT(EXAMPLE_SYNC_GPIO),
    };
    ESP_ERROR_CHECK(gpio_config(&sync_gpio_conf));

    ESP_LOGI(TAG, "Create GPIO sync source");
    mcpwm_sync_handle_t gpio_sync_source = NULL;
    mcpwm_gpio_sync_src_config_t gpio_sync_config = {
        .group_id = 0, // GPIO fault should be in the same group of the above timers
        .gpio_num = EXAMPLE_SYNC_GPIO,
        .flags.pull_down = true,
        .flags.active_neg = false, // by default, a posedge pulse can trigger a sync event
    };
    ESP_ERROR_CHECK(mcpwm_new_gpio_sync_src(&gpio_sync_config, &gpio_sync_source));

    ESP_LOGI(TAG, "Set timers to sync on the GPIO");
    mcpwm_timer_sync_phase_config_t sync_phase_config = {
        .count_value = 0,
        .direction = MCPWM_TIMER_DIRECTION_UP,
        .sync_src = gpio_sync_source,
    };
    for (int i = 0; i < 3; i++)
    {
        ESP_ERROR_CHECK(mcpwm_timer_set_phase_on_sync(timers[i], &sync_phase_config));
    }

    ESP_LOGI(TAG, "Trigger a pulse on the GPIO as a sync event");
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_SYNC_GPIO, 0));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_SYNC_GPIO, 1));
    ESP_ERROR_CHECK(gpio_reset_pin(EXAMPLE_SYNC_GPIO));
}