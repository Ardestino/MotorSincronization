#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/mcpwm_prelude.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define EXAMPLE_TIMER_RESOLUTION_HZ 1000000                 // 1MHz, 1us per tick
#define EXAMPLE_TIMER_PERIOD0 300                           // 1000 ticks, 1ms
#define EXAMPLE_TIMER_UPPERIOD0 (EXAMPLE_TIMER_PERIOD0 / 2) // 50% duty cycle
const gpio_num_t EXAMPLE_GEN_STP0 = GPIO_NUM_27;            // Verde

static const char *TAG_TIMER = "TIMER";

// Callback para contar pulsos PWM
static volatile int pulse_count = 0; // Contador de pulsos
static bool IRAM_ATTR on_pwm_compare_event(mcpwm_cmpr_handle_t cmpr, const mcpwm_compare_event_data_t *edata, void *user_ctx)
{
    pulse_count = pulse_count + 1; // Incrementar contador en cada pulso (evita '++' en volatile)
    return false;                  // No hay necesidad de despertar tareas de mayor prioridad
}

class Timer
{
private:

public:
    Timer()
    {
        ESP_LOGI(TAG_TIMER, "Create timer");
        mcpwm_timer_handle_t timer;
        mcpwm_timer_config_t timer_config = {
            .group_id = 0,
            .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
            .resolution_hz = EXAMPLE_TIMER_RESOLUTION_HZ,
            .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
            .period_ticks = EXAMPLE_TIMER_PERIOD0,
            .intr_priority = 0, // Use default priority
            .flags = {
                .update_period_on_empty = true, // Update period when timer counts to zero
                .update_period_on_sync = false, // Do not update period on sync event
                .allow_pd = false,              // Do not allow power down
            }};
        ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

        ESP_LOGI(TAG_TIMER, "Create operator");
        mcpwm_oper_handle_t op;
        mcpwm_operator_config_t operator_config = {
            .group_id = 0,      // operator should be in the same group of the above timers
            .intr_priority = 0, // Use default priority
            .flags = {
                .update_gen_action_on_tez = true,   // Update generator action when timer counts to
                .update_gen_action_on_tep = true,   // Update generator action when timer counts to peak
                .update_gen_action_on_sync = false, // Do not update generator action on sync event
                .update_dead_time_on_tez = false,   // Do not update dead time when timer counts to zero
                .update_dead_time_on_tep = false,   // Do not update dead time when timer counts to peak
                .update_dead_time_on_sync = false,  // Do not update dead time on sync event
            }};
        ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &op));

        ESP_LOGI(TAG_TIMER, "Connect timer and operator with each other");
        ESP_ERROR_CHECK(mcpwm_operator_connect_timer(op, timer));

        ESP_LOGI(TAG_TIMER, "Create comparator");
        mcpwm_cmpr_handle_t comparator;
        mcpwm_comparator_config_t compare_config = {
            .intr_priority = 0, // Use default priority
            .flags = {
                .update_cmp_on_tez = true,
                .update_cmp_on_tep = false,
                .update_cmp_on_sync = false,
            }};
        const int compare_period = EXAMPLE_TIMER_UPPERIOD0;
        ESP_ERROR_CHECK(mcpwm_new_comparator(op, &compare_config, &comparator));
        // init compare for each comparator
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, compare_period));

        // Registrar callback para contar pulsos
        mcpwm_comparator_event_callbacks_t cbs = {
            .on_reach = on_pwm_compare_event,
        };
        ESP_ERROR_CHECK(mcpwm_comparator_register_event_callbacks(comparator, &cbs, NULL));

        ESP_LOGI(TAG_TIMER, "Create generator");
        mcpwm_gen_handle_t generator;
        mcpwm_generator_config_t gen_config = {};
        gen_config.gen_gpio_num = EXAMPLE_GEN_STP0;
        ESP_ERROR_CHECK(mcpwm_new_generator(op, &gen_config, &generator));

        ESP_LOGI(TAG_TIMER, "Set generator actions on timer and compare event");
        ESP_ERROR_CHECK(
            mcpwm_generator_set_action_on_timer_event(
                generator,
                // when the timer value is zero, and is counting up, set output to high
                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
        ESP_ERROR_CHECK(
            mcpwm_generator_set_action_on_compare_event(
                generator,
                // when compare event happens, and timer is counting up, set output to low
                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

        ESP_LOGI(TAG_TIMER, "Start timer");
        ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
        ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
};