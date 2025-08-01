#pragma once
#include "LimitSwitch.h"
#include "Motor.h"

class Axis
{
private:
    Motor motor;
    LimitSwitch limitSwitch; // Assuming LimitSwitch is defined elsewhere

public:
    Axis(Motor motor, LimitSwitch limitSwitch) : motor(motor), limitSwitch(limitSwitch) {}
    ~Axis() {}
    void count_steps()
    {
        // // Primero ir hacia LS1
        // ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 0));
        // ESP_LOGI(TAG, "Buscando LS1...");
        // while (true)
        // {

        //     while (!ls_triggered)
        //     { // Esperar hasta que se active la interrupción
        //         vTaskDelay(pdMS_TO_TICKS(10));
        //     }
        //     ESP_LOGI(TAG, "LS1 alcanzado por interrupción, iniciando conteo hacia LS2...");

        //     // Ahora contar pasos desde LS1 hacia LS2
        //     pulse_count = 0;
        //     ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 1)); // Habilitar el generador

        //     // Invertir direccion y esperar a que se suelte el push button
        //     while (ls_triggered)
        //     {
        //         vTaskDelay(pdMS_TO_TICKS(10));
        //     }
        //     ESP_LOGI(TAG, "Se solto LS1 correctamente, comenzando conteo hacia LS2...");

        //     while (!ls_triggered)
        //     { // Esperar hasta que se active la interrupción
        //         vTaskDelay(pdMS_TO_TICKS(10));
        //     }
        //     ESP_LOGI(TAG, "LS2 alcanzado por interrupción. Total de pasos: %d", pulse_count);

        //     // Invertir direccion y esperar a que se suelte el push button
        //     ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 0)); // Habilitar el generador
        //     while (ls_triggered)
        //     {
        //         vTaskDelay(pdMS_TO_TICKS(10));
        //     }
        //     ESP_LOGI(TAG, "Se solto LS2 correctamente, comenzando conteo hacia LS1...");
        // }
    }
};