#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define EXAMPLE_TIMER_RESOLUTION_HZ 1000000 // 1MHz, 1us per tick
#define EXAMPLE_TIMER_PERIOD0 100 // 1000 ticks, 1ms
#define EXAMPLE_TIMER_UPPERIOD0 (EXAMPLE_TIMER_PERIOD0 / 2) // 50% duty cycle
#define EXAMPLE_GEN_STP0 23 // Verde
#define EXAMPLE_GEN_DIR0 22 // Amarillo
#define EXAMPLE_GEN_ENA0 21 // Azul
#define LS1_PIN 19
#define LS2_PIN 18
#define DEBOUNCE_TIME_US 50000 // 50ms debounce time in microseconds

static const char *TAG = "MOTOR";

// Variables para interrupciones
static volatile bool ls1_triggered = false;
static volatile bool ls2_triggered = false;
static volatile int pulse_count = 0; // Contador de pulsos
static volatile int64_t last_ls1_time = 0; // Último tiempo de interrupción LS1
static volatile int64_t last_ls2_time = 0; // Último tiempo de interrupción LS2

// Callback para contar pulsos PWM
static bool IRAM_ATTR on_pwm_compare_event(mcpwm_cmpr_handle_t cmpr, const mcpwm_compare_event_data_t *edata, void *user_ctx)
{
    pulse_count++; // Incrementar contador en cada pulso
    return false; // No hay necesidad de despertar tareas de mayor prioridad
}

// ISR para LS1
static void IRAM_ATTR ls1_isr_handler(void* arg)
{
    int64_t current_time = esp_timer_get_time();
    
    // Implementar debounce
    if (current_time - last_ls1_time > DEBOUNCE_TIME_US) {
        ls1_triggered = true;
        gpio_set_level(EXAMPLE_GEN_DIR0, 1);
        last_ls1_time = current_time;
    }
}

// ISR para LS2
static void IRAM_ATTR ls2_isr_handler(void* arg)
{
    int64_t current_time = esp_timer_get_time();
    
    // Implementar debounce
    if (current_time - last_ls2_time > DEBOUNCE_TIME_US) {
        ls2_triggered = true;
        gpio_set_level(EXAMPLE_GEN_DIR0, 0);
        last_ls2_time = current_time;
    }
}

void setup_gpio()
{
    ESP_LOGI(TAG, "Initialize Limit Switch GPIOs");
    gpio_config_t io_conf = {};
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LS1_PIN) | (1ULL << LS2_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE; // Interrupción en flanco descendente
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Instalar el servicio de interrupciones GPIO
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    
    // Añadir manejadores de interrupción
    ESP_ERROR_CHECK(gpio_isr_handler_add(LS1_PIN, ls1_isr_handler, (void*) LS1_PIN));
    ESP_ERROR_CHECK(gpio_isr_handler_add(LS2_PIN, ls2_isr_handler, (void*) LS2_PIN));

    // Init Motor Ports
    ESP_LOGI(TAG, "Initialize MCPWM GPIOs");
    gpio_config_t gen_gpio_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT(EXAMPLE_GEN_STP0) |
                        BIT(EXAMPLE_GEN_DIR0) |
                        BIT(EXAMPLE_GEN_ENA0),
    };
    ESP_ERROR_CHECK(gpio_config(&gen_gpio_conf));
    
    // Inicializar pines DIR y ENA en estado bajo
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_GEN_DIR0, 0));
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_GEN_ENA0, 1));
    ESP_LOGI(TAG, "DIR and ENA pins initialized to LOW state");
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

    // Registrar callback para contar pulsos
    mcpwm_comparator_event_callbacks_t cbs = {
        .on_reach = on_pwm_compare_event,
    };
    ESP_ERROR_CHECK(mcpwm_comparator_register_event_callbacks(comparator, &cbs, NULL));

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
    // Reset flags
    ls1_triggered = false;
    ls2_triggered = false;
    
    // Primero ir hacia LS1
    ESP_LOGI(TAG, "Buscando LS1...");
    // Dirección hacia LS1
    
    while (!ls1_triggered) { // Esperar hasta que se active la interrupción
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGI(TAG, "LS1 alcanzado por interrupción, iniciando conteo hacia LS2...");
    
    // Ahora contar pasos desde LS1 hacia LS2
    pulse_count = 0;
    ls1_triggered = false;
    ls2_triggered = false;
    
    while (!ls2_triggered) { // Esperar hasta que se active la interrupción
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "LS2 alcanzado por interrupción. Total de pasos: %d", pulse_count);
    return pulse_count; // Retornar el número de pulsos contados
}

void app_main(void)
{
    setup_gpio();

    start_pwm();
    
    // Contar pasos entre limit switches
    int total_steps = count_steps();
    ESP_LOGI(TAG, "Pasos totales entre LS1 y LS2: %d", total_steps);
}