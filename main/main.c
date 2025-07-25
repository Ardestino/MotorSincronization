#include "driver/mcpwm_prelude.h"
#include "sync_example.h"
#include "sync_pwm.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define DIR_PIN 21
#define STEP_PIN 22
#define EN_PIN 23
#define LS1_PIN 18
#define LS2_PIN 19

static const char *TAG = "MOTOR";

void setup_gpio()
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << DIR_PIN) | (1ULL << STEP_PIN) | (1ULL << EN_PIN);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LS1_PIN) | (1ULL << LS2_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    gpio_set_level(EN_PIN, 0); // Enable driver
}

void step_motor(int steps, bool dir)
{
    gpio_set_level(DIR_PIN, dir);
    for (int i = 0; i < steps; ++i)
    {
        if (gpio_get_level(LS1_PIN) == 0 || gpio_get_level(LS2_PIN) == 0)
            break; // Stop if any limit switch is pressed

        gpio_set_level(STEP_PIN, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        gpio_set_level(STEP_PIN, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

int count_steps_ls1_to_ls2()
{
    int step_count = 0;
    
    // Primero ir hacia LS1
    ESP_LOGI(TAG, "Buscando LS1...");
    gpio_set_level(DIR_PIN, false); // Dirección hacia LS1
    
    while (gpio_get_level(LS1_PIN) != 0) {
        gpio_set_level(STEP_PIN, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        gpio_set_level(STEP_PIN, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    
    ESP_LOGI(TAG, "LS1 alcanzado, iniciando conteo hacia LS2...");
    
    // Ahora contar pasos desde LS1 hacia LS2
    gpio_set_level(DIR_PIN, true); // Dirección hacia LS2
    vTaskDelay(50 / portTICK_PERIOD_MS); // Pequeña pausa para salir de LS1
    
    while (gpio_get_level(LS2_PIN) != 0) {
        gpio_set_level(STEP_PIN, 1);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        gpio_set_level(STEP_PIN, 0);
        vTaskDelay(1 / portTICK_PERIOD_MS);
        step_count++;
    }
    
    ESP_LOGI(TAG, "LS2 alcanzado. Total de pasos: %d", step_count);
    return step_count;
}

void app_main(void)
{
    setup_gpio();
    
    // Contar pasos entre limit switches
    int total_steps = count_steps_ls1_to_ls2();
    ESP_LOGI(TAG, "Pasos totales entre LS1 y LS2: %d", total_steps);

    while (true)
    {
        step_motor(200, true); // Move 200 steps forward
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        step_motor(200, false); // Move 200 steps backward
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}