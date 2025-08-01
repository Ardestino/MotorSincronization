#pragma once
#include "LimitSwitch.h"
#include "Motor.h"

class Axis
{
private:
    const char *axis_name;
    Motor motor;
    LimitSwitch limitSwitch;
    int pulse_count = 0;

public:
    Axis(const char* name, Motor motor, LimitSwitch limitSwitch) : axis_name(name), motor(motor), limitSwitch(limitSwitch) {}
    ~Axis() {}
    void auto_home()
    {
        // Primero ir hacia LS1
        ESP_LOGI(axis_name, "Buscando LS1...");
        motor.set_direction(false);
        motor.set_enable(true);

        // Esperar hasta que se active el limit switch
        while (!limitSwitch.triggered()){
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        motor.set_enable(false);
        ESP_LOGI(axis_name, "LS1 alcanzado por interrupción");
        
        // Ahora contar pasos desde LS1 hacia LS2
        pulse_count = 0;
        motor.set_direction(true); // Invertir dirección para ir hacia LS2
        motor.set_enable(true); // Habilitar el motor
        
        // Esperar a que se suelte el push button
        while (limitSwitch.triggered())
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        ESP_LOGI(axis_name, "Se solto LS1 correctamente, comenzando conteo hacia LS2...");

        // Esperar a que llegue a LS2
        while (!limitSwitch.triggered()){
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        motor.set_enable(false);
        ESP_LOGI(axis_name, "LS2 alcanzado por interrupción. Total de pasos: %d", pulse_count);
        ESP_LOGI(axis_name, "Homing completado. Motor deshabilitado.");
    }
};