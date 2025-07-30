#include <iostream>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/mcpwm_prelude.h"
#include "sync_example.h"
#include "sync_pwm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h" // Añadir este include

#define EXAMPLE_TIMER_RESOLUTION_HZ 1000000                 // 1MHz, 1us per tick
#define EXAMPLE_TIMER_PERIOD0 100                           // 1000 ticks, 1ms
#define EXAMPLE_TIMER_UPPERIOD0 (EXAMPLE_TIMER_PERIOD0 / 2) // 50% duty cycle
const gpio_num_t EXAMPLE_GEN_STP0 = GPIO_NUM_23;            // Verde
const gpio_num_t EXAMPLE_GEN_DIR0 = GPIO_NUM_22;            // Amarillo
const gpio_num_t EXAMPLE_GEN_ENA0 = GPIO_NUM_21;            // Azul
const gpio_num_t LS1_PIN = GPIO_NUM_19;
#define DEBOUNCE_TIME_US 50000 // 100ms debounce time in microseconds

static const char *TAG = "MOTOR";

// Variables para interrupciones
static volatile bool ls_triggered = false;
static volatile int pulse_count = 0;       // Contador de pulsos
static volatile int64_t last_ls1_time = 0; // Último tiempo de interrupción LS1
static volatile int64_t last_ls2_time = 0; // Último tiempo de interrupción LS2

// Callback para contar pulsos PWM
static bool IRAM_ATTR on_pwm_compare_event(mcpwm_cmpr_handle_t cmpr, const mcpwm_compare_event_data_t *edata, void *user_ctx)
{
    pulse_count = pulse_count + 1; // Incrementar contador en cada pulso (evita '++' en volatile)
    return false;                  // No hay necesidad de despertar tareas de mayor prioridad
}

// ISR para LS1
static void IRAM_ATTR ls1_isr_handler(void *arg)
{
    int64_t current_time = esp_timer_get_time();

    // Verificar el estado actual del pin para confirmar el flanco
    int pin_level = gpio_get_level(LS1_PIN);

    // Solo procesar si es realmente un flanco ascendente (pin en HIGH)
    if (pin_level == 0 && (current_time - last_ls1_time > DEBOUNCE_TIME_US))
    {
        ls_triggered = !ls_triggered;
        last_ls1_time = current_time;

        // Log seguro para ISR
        esp_rom_printf("LS %d triggered %s at time: %lld\n", pin_level, ls_triggered ? "true" : "false", current_time);
    }
}

void setup_gpio()
{
    ESP_LOGI(TAG, "Initialize Limit Switch GPIOs");
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LS1_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Interrupción en flanco descendente
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Instalar el servicio de interrupciones GPIO
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Añadir manejadores de interrupción
    ESP_ERROR_CHECK(gpio_isr_handler_add(LS1_PIN, ls1_isr_handler, (void *)LS1_PIN));

    // Init Motor Ports
    ESP_LOGI(TAG, "Initialize MCPWM GPIOs");
    gpio_config_t gen_gpio_conf = {
        .pin_bit_mask = BIT(EXAMPLE_GEN_STP0) | BIT(EXAMPLE_GEN_DIR0) | BIT(EXAMPLE_GEN_ENA0),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
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
        .intr_priority = 0, // Use default priority
        .flags = {
            .update_period_on_empty = true, // Update period when timer counts to zero
            .update_period_on_sync = false, // Do not update period on sync event
            .allow_pd = false,              // Do not allow power down
        }};
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    ESP_LOGI(TAG, "Create operator");
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

    ESP_LOGI(TAG, "Connect timer and operator with each other");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(op, timer));

    ESP_LOGI(TAG, "Create comparator");
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

    ESP_LOGI(TAG, "Create generator");
    mcpwm_gen_handle_t generator;
    mcpwm_generator_config_t gen_config = {};
    gen_config.gen_gpio_num = EXAMPLE_GEN_STP0;
    ESP_ERROR_CHECK(mcpwm_new_generator(op, &gen_config, &generator));

    ESP_LOGI(TAG, "Set generator actions on timer and compare event");
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

    ESP_LOGI(TAG, "Start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));
    vTaskDelay(pdMS_TO_TICKS(100));
}

void count_steps()
{
    // Primero ir hacia LS1
    ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_GEN_DIR0, 0));
    ESP_LOGI(TAG, "Buscando LS1...");
    while (true)
    {

        while (!ls_triggered)
        { // Esperar hasta que se active la interrupción
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "LS1 alcanzado por interrupción, iniciando conteo hacia LS2...");

        // Ahora contar pasos desde LS1 hacia LS2
        pulse_count = 0;
        ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_GEN_DIR0, 1)); // Habilitar el generador

        // Invertir direccion y esperar a que se suelte el push button
        while (ls_triggered)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Se solto LS1 correctamente, comenzando conteo hacia LS2...");

        while (!ls_triggered)
        { // Esperar hasta que se active la interrupción
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "LS2 alcanzado por interrupción. Total de pasos: %d", pulse_count);

        // Invertir direccion y esperar a que se suelte el push button
        ESP_ERROR_CHECK(gpio_set_level(EXAMPLE_GEN_DIR0, 0)); // Habilitar el generador
        while (ls_triggered)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Se solto LS2 correctamente, comenzando conteo hacia LS1...");
    }
}

extern "C" void app_main(void)
{
    setup_gpio();

    start_pwm();

    count_steps();

    // // Contar pasos entre limit switches
    // int total_steps = count_steps();
    // ESP_LOGI(TAG, "Pasos totales entre LS1 y LS2: %d", total_steps);

    // std::cout << "Hello, World!" << std::endl;
    // example();
}
