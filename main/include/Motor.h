#pragma once
#include "esp_log.h"
#include "driver/gpio.h"

static const char *MOTOR_TAG = "MOTOR";

class Motor
{
private:
    gpio_num_t dir_pin;
    gpio_num_t stp_pin;
    gpio_num_t ena_pin;

public:
    Motor(gpio_num_t dir, gpio_num_t stp, gpio_num_t ena)
        : dir_pin(dir), stp_pin(stp), ena_pin(ena)
    {
        ESP_LOGI(MOTOR_TAG, "Initialize MCPWM GPIOs");

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << dir_pin) | (1ULL << stp_pin) | (1ULL << ena_pin),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));

        // Inicializar pines DIR y ENA en estado bajo
        ESP_ERROR_CHECK(gpio_set_level(dir_pin, 0));
        ESP_ERROR_CHECK(gpio_set_level(ena_pin, 1));
        ESP_LOGI(MOTOR_TAG, "DIR and ENA pins initialized to LOW state");
    }
    ~Motor() {}
    void set_direction(bool direction)
    {
        ESP_ERROR_CHECK(gpio_set_level(dir_pin, direction ? 1 : 0));
        ESP_LOGI(MOTOR_TAG, "Motor direction set to %s", direction ? "FORWARD" : "REVERSE");
    }
    void set_enable(bool enable)
    {
        ESP_ERROR_CHECK(gpio_set_level(ena_pin, enable ? 0 : 1)); // LOW to enable, HIGH to disable
        ESP_LOGI(MOTOR_TAG, "Motor %s", enable ? "enabled" : "disabled");
    }
};