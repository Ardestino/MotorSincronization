#pragma once
#include "config.h"

#define DEBOUNCE_TIME_US 50000 // 100ms debounce time in microseconds
static volatile bool ls_triggered = false;
static volatile int64_t last_ls1_time = 0; // Último tiempo de interrupción LS1

// ISR para LS1
static void IRAM_ATTR ls1_isr_handler(void *arg)
{
    int64_t current_time = esp_timer_get_time();

    // Verificar el estado actual del pin para confirmar el flanco
    int pin_level = gpio_get_level(Q1_LSW);

    // Solo procesar si es realmente un flanco ascendente (pin en HIGH)
    if (pin_level == 0 && (current_time - last_ls1_time > DEBOUNCE_TIME_US))
    {
        ls_triggered = !ls_triggered;
        last_ls1_time = current_time;

        // Log seguro para ISR
        esp_rom_printf("LS %d triggered %s at time: %lld\n", pin_level, ls_triggered ? "true" : "false", current_time);
    }
}

class LimitSwitch
{
private:
    gpio_num_t pin;
    bool state;

public:
    LimitSwitch(gpio_num_t pin) : pin(pin), state(false)
    {
        // Configurar el GPIO del Limit Switch
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE, // Interrupción en flanco descendente
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        // Añadir manejador de interrupción
        ESP_ERROR_CHECK(gpio_isr_handler_add(pin, ls1_isr_handler, (void *)pin));
    }
    ~LimitSwitch() {}
};