#pragma once

#define DEBOUNCE_TIME_US 50000

// Declaración forward de la función ISR (SIN IRAM_ATTR)
void ls1_isr_handler(void *arg);

class LimitSwitch
{
private:
    const char *name;
    gpio_num_t pin;
    volatile bool ls_triggered;
    volatile int64_t last_trigger_time;

    // Declarar la función ISR como friend (SIN IRAM_ATTR)
    friend void ls1_isr_handler(void *arg);

public:
    LimitSwitch(const char *name, gpio_num_t pin) 
        : name(name), pin(pin), ls_triggered(false), last_trigger_time(0)
    {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE, // Habilitar pull-up
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        ESP_ERROR_CHECK(gpio_isr_handler_add(pin, ls1_isr_handler, (void *)this));
    }

    bool checkDebounce(int64_t current_time)
    {
        if (current_time - last_trigger_time > DEBOUNCE_TIME_US)
        {
            last_trigger_time = current_time;
            return true;
        }
        return false;
    }

    const char *getName() const { return name; }
    gpio_num_t getPin() const { return pin; }
    bool triggered() { return ls_triggered; }
    void triggered(bool value) { ls_triggered = value; }
};

void IRAM_ATTR ls1_isr_handler(void *arg)
{
    int64_t current_time = esp_timer_get_time();
    LimitSwitch* limit_switch = (LimitSwitch*)arg;
    
    // Acceso directo a miembros privados gracias a friend
    if (limit_switch->checkDebounce(current_time))
    {
        int pin_level = gpio_get_level(limit_switch->pin); // Acceso directo al miembro privado
        
        if (pin_level == 0) // Flanco descendente
        {
            bool new_state = !limit_switch->ls_triggered; // Acceso directo al miembro privado
            limit_switch->ls_triggered = new_state; // Acceso directo al miembro privado
            
            esp_rom_printf("LS %s triggered %s at time: %lld\n", 
                          limit_switch->name, // Acceso directo al miembro privado
                          new_state ? "true" : "false", 
                          current_time);
        }
    }
}