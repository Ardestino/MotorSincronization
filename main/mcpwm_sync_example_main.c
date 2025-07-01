#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "sync_pwm.h"

const static char *TAG_MAIN = "MAIN";

#define EXAMPLE_TIMER_RESOLUTION_HZ  1000000  // 1MHz, 1us per tick

#define EXAMPLE_TIMER_PERIOD0        3000     // 1000 ticks, 1ms
#define EXAMPLE_TIMER_PERIOD1        1000     // 1000 ticks, 3ms
#define EXAMPLE_TIMER_PERIOD2        200      //  200 ticks, 200us

#define EXAMPLE_TIMER_UPPERIOD0       (EXAMPLE_TIMER_PERIOD0 / 2) // 50% duty cycle
#define EXAMPLE_TIMER_UPPERIOD1       (EXAMPLE_TIMER_PERIOD1 / 2) // 50% duty cycle
#define EXAMPLE_TIMER_UPPERIOD2       (EXAMPLE_TIMER_PERIOD2 / 2) // 50% duty cycle

#define EXAMPLE_GEN_STP0           23 // Verde
#define EXAMPLE_GEN_DIR0           22 // Azul
#define EXAMPLE_GEN_ENA0           21 // Amarillo

#define EXAMPLE_GEN_STP1           19
#define EXAMPLE_GEN_DIR1           18
#define EXAMPLE_GEN_ENA1           5

#define EXAMPLE_GEN_STP2           17
#define EXAMPLE_GEN_DIR2           16
#define EXAMPLE_GEN_ENA2           4

void app_main(void)
{
    // Init Ports
    ESP_LOGI(TAG_MAIN, "Initialize MCPWM GPIOs");
    gpio_config_t gen_gpio_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT(EXAMPLE_GEN_STP0) | BIT(EXAMPLE_GEN_STP1) | BIT(EXAMPLE_GEN_STP2) |
                        BIT(EXAMPLE_GEN_DIR0) | BIT(EXAMPLE_GEN_DIR1) | BIT(EXAMPLE_GEN_DIR2) |
                        BIT(EXAMPLE_GEN_ENA0) | BIT(EXAMPLE_GEN_ENA1) | BIT(EXAMPLE_GEN_ENA2) ,
    };
    ESP_ERROR_CHECK(gpio_config(&gen_gpio_conf));

    ESP_LOGI(TAG_MAIN, "Create timers");
    mcpwm_timer_handle_t timers[3];
    const int time_period[3] = {EXAMPLE_TIMER_PERIOD0, EXAMPLE_TIMER_PERIOD1, EXAMPLE_TIMER_PERIOD2};
    mcpwm_timer_config_t timer_config = {
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .group_id = 0,
        .resolution_hz = EXAMPLE_TIMER_RESOLUTION_HZ,
        .period_ticks = EXAMPLE_TIMER_PERIOD0,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    for (int i = 0; i < 3; i++) {
        timer_config.period_ticks = time_period[i];
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timers[i]));
    }

    ESP_LOGI(TAG_MAIN, "Create operators");
    mcpwm_oper_handle_t operators[3];
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator should be in the same group of the above timers
    };
    for (int i = 0; i < 3; ++i) {
        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operators[i]));
    }

    ESP_LOGI(TAG_MAIN, "Connect timers and operators with each other");
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operators[i], timers[i]));
    }

    ESP_LOGI(TAG_MAIN, "Create comparators");
    mcpwm_cmpr_handle_t comparators[3];
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    const int compare_periods[3] = {EXAMPLE_TIMER_UPPERIOD0, EXAMPLE_TIMER_UPPERIOD1, EXAMPLE_TIMER_UPPERIOD2};
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_new_comparator(operators[i], &compare_config, &comparators[i]));
        // init compare for each comparator
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparators[i], compare_periods[i]));
    }

    ESP_LOGI(TAG_MAIN, "Create generators");
    mcpwm_gen_handle_t generators[3];
    const int gen_gpios[3] = {EXAMPLE_GEN_STP0, EXAMPLE_GEN_STP1, EXAMPLE_GEN_STP2};
    mcpwm_generator_config_t gen_config = {};
    for (int i = 0; i < 3; i++) {
        gen_config.gen_gpio_num = gen_gpios[i];
        ESP_ERROR_CHECK(mcpwm_new_generator(operators[i], &gen_config, &generators[i]));
    }

    ESP_LOGI(TAG_MAIN, "Set generator actions on timer and compare event");
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generators[i],
                                                                  // when the timer value is zero, and is counting up, set output to high
                                                                  MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
        ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generators[i],
                                                                    // when compare event happens, and timer is counting up, set output to low
                                                                    MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparators[i], MCPWM_GEN_ACTION_LOW)));
    }

    ESP_LOGI(TAG_MAIN, "Start timers one by one, so they are not synced");
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_timer_enable(timers[i]));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(timers[i], MCPWM_TIMER_START_NO_STOP));
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // Manually added this "IDLE" phase, which can help us distinguish the wave forms before and after sync
    ESP_LOGI(TAG_MAIN, "Force the output level to low, timer still running");
    for (int i = 0; i < 3; i++) {
        ESP_ERROR_CHECK(mcpwm_generator_set_force_level(generators[i], 0, true));
    }

    ESP_LOGI(TAG_MAIN, "Setup sync strategy");
    example_setup_sync_strategy(timers);

    ESP_LOGI(TAG_MAIN, "Now the output PWMs should in sync");
    for (int i = 0; i < 3; ++i) {
        // remove the force level on the generator, so that we can see the PWM again
        ESP_ERROR_CHECK(mcpwm_generator_set_force_level(generators[i], -1, true));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
}
