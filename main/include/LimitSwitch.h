#pragma once

#define DEBOUNCE_TIME_US 50000 // 100ms debounce time in microseconds
static volatile bool ls_triggered = false;
static volatile int64_t last_ls1_time = 0; // Último tiempo de interrupción LS1
static volatile int64_t last_ls2_time = 0; // Último tiempo de interrupción LS2

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
    int pin;
    bool state;
public:
    LimitSwitch(/* args */) {}
    ~LimitSwitch() {}
};