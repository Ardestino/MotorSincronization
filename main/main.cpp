#include <iostream>
#include <stdbool.h>

#include "esp_log.h"

#include "config.h"
#include "Timer.h"
#include "LimitSwitch.h"

static const char *TAG = "MOTOR";

void setup_gpio()
{
    ESP_LOGI(TAG, "Initialize Limit Switch GPIOs");
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << Q1_LSW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE, // Interrupción en flanco descendente
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Instalar el servicio de interrupciones GPIO
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Añadir manejadores de interrupción
    ESP_ERROR_CHECK(gpio_isr_handler_add(Q1_LSW, ls1_isr_handler, (void *)Q1_LSW));

    // Init Motor Ports
    ESP_LOGI(TAG, "Initialize MCPWM GPIOs");
    gpio_config_t gen_gpio_conf = {
        .pin_bit_mask = BIT(Q1_STP) | BIT(Q1_DIR) | BIT(Q1_ENA),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gen_gpio_conf));

    // Inicializar pines DIR y ENA en estado bajo
    ESP_ERROR_CHECK(gpio_set_level(Q1_DIR, 0));
    ESP_ERROR_CHECK(gpio_set_level(Q1_ENA, 1));
    ESP_LOGI(TAG, "DIR and ENA pins initialized to LOW state");
}

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
    setup_gpio();

    // start_pwm();

    count_steps();

    // // Contar pasos entre limit switches
    // int total_steps = count_steps();
    // ESP_LOGI(TAG, "Pasos totales entre LS1 y LS2: %d", total_steps);

    // std::cout << "Hello, World!" << std::endl;
    // example();
}
