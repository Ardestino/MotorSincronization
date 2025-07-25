#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXAMPLE_TIMER_RESOLUTION_HZ 1000000 // 1MHz, 1us per tick
#define EXAMPLE_TIMER_PERIOD0 3000 // 1000 ticks, 1ms
#define EXAMPLE_TIMER_UPPERIOD0 (EXAMPLE_TIMER_PERIOD0 / 2) // 50% duty cycle
#define EXAMPLE_GEN_STP0 22 // Verde
#define EXAMPLE_GEN_DIR0 21 // Amarillo
#define EXAMPLE_GEN_ENA0 23 // Azul
#define LS1_PIN 18
#define LS2_PIN 19

static const char *TAG = "MOTOR";

void setup_gpio()
{
    ESP_LOGI(TAG, "Initialize Limit Switch GPIOs");
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LS1_PIN) | (1ULL << LS2_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));


    // Init Motor Ports
    ESP_LOGI(TAG, "Initialize MCPWM GPIOs");
    gpio_config_t gen_gpio_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT(EXAMPLE_GEN_STP0) |
                        BIT(EXAMPLE_GEN_DIR0) |
                        BIT(EXAMPLE_GEN_ENA0),
    };
    ESP_ERROR_CHECK(gpio_config(&gen_gpio_conf));
}

void start_pwm()
{
    ESP_LOGI(TAG, "Create timer");
    mcpwm_timer_handle_t timer;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = EXAMPLE_TIMER_RESOLUTION_HZ,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
        .period_ticks = EXAMPLE_TIMER_PERIOD0,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    ESP_LOGI(TAG, "Create operator");
    mcpwm_oper_handle_t operator;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator should be in the same group of the above timers
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &operator));

    ESP_LOGI(TAG, "Connect timer and operator with each other");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(operator, timer));

    ESP_LOGI(TAG, "Create comparator");
    mcpwm_cmpr_handle_t comparator;
    mcpwm_comparator_config_t compare_config = {
        .flags.update_cmp_on_tez = true,
    };
    const int compare_period = EXAMPLE_TIMER_UPPERIOD0;
    ESP_ERROR_CHECK(mcpwm_new_comparator(operator, &compare_config, &comparator));
    // init compare for each comparator
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, compare_period));

    ESP_LOGI(TAG, "Create generator");
    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t gen_config = {};
    gen_config.gen_gpio_num = EXAMPLE_GEN_STP0;
    ESP_ERROR_CHECK(mcpwm_new_generator(operator, &gen_config, &generator));

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
    ESP_ERROR_CHECK(
        mcpwm_generator_set_action_on_timer_event(
            generator,
            // when the timer value is zero, and is counting up, set output to high
            MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)
        )
    );
    ESP_ERROR_CHECK(
        mcpwm_generator_set_action_on_compare_event(
            generator,
            // when compare event happens, and timer is counting up, set output to low
            MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)
        )
    );

    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    vTaskDelay(pdMS_TO_TICKS(100));
}

int count_steps()
{
    int step_count = 0;
    
    // Primero ir hacia LS1
    ESP_LOGI(TAG, "Buscando LS1...");
    // gpio_set_level(DIR_PIN, false); // Dirección hacia LS1
    
    while (gpio_get_level(LS1_PIN) == 0) { // Cambiado: mientras NO esté presionado
        // gpio_set_level(STEP_PIN, 1);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
        // gpio_set_level(STEP_PIN, 0);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "LS1 alcanzado, iniciando conteo hacia LS2...");
    
    // Ahora contar pasos desde LS1 hacia LS2
    // gpio_set_level(DIR_PIN, true); // Dirección hacia LS2
    // vTaskDelay(50 / portTICK_PERIOD_MS); // Pequeña pausa para salir de LS1
    
    while (gpio_get_level(LS2_PIN) == 0) { // Cambiado: mientras NO esté presionado
        // gpio_set_level(STEP_PIN, 1);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
        // gpio_set_level(STEP_PIN, 0);
        // vTaskDelay(1 / portTICK_PERIOD_MS);
        // step_count++;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "LS2 alcanzado. Total de pasos: %d", step_count);
    return step_count;
}

void app_main(void)
{
    setup_gpio();

    start_pwm();
    
    // Contar pasos entre limit switches
    int total_steps = count_steps();
    ESP_LOGI(TAG, "Pasos totales entre LS1 y LS2: %d", total_steps);
}