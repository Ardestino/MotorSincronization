#include <iostream>
#include <stdbool.h>

#include "esp_log.h"

#include "config.h"
#include "Timer.h"
#include "LimitSwitch.h"
#include "Motor.h"

static const char *TAG = "MAIN";

void count_steps()
{
    // Primero ir hacia LS1
    ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 0));
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
        ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 1)); // Habilitar el generador

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
        ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 0)); // Habilitar el generador
        while (ls_triggered)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(TAG, "Se solto LS2 correctamente, comenzando conteo hacia LS1...");
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando aplicación...");
    Motor motor_q1(Q1_DIR, Q1_STP, Q1_ENA);
    Motor motor_q2(Q2_DIR, Q2_STP, Q2_ENA);
    Motor motor_q3(Q3_DIR, Q3_STP, Q3_ENA);
    Motor motor_q4(Q4_DIR, Q4_STP, Q4_ENA);

    count_steps();
}
