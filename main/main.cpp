#include <iostream>
#include <stdbool.h>

#include "esp_log.h"

#include "config.h"
#include "Timer.h"
#include "LimitSwitch.h"
#include "Motor.h"
#include "Axis.h"
#include "esp_rest_main.h"

static const char *TAG = "MAIN";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Iniciando aplicación...");

    // Configuracion del sistema
    // Instalar el servicio de interrupciones GPIO
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Inicializar el servidor REST
    init_server();

    Motor motor_q1(Q1_DIR, Q1_STP, Q1_ENA);
    Motor motor_q2(Q2_DIR, Q2_STP, Q2_ENA);
    Motor motor_q3(Q3_DIR, Q3_STP, Q3_ENA);
    Motor motor_q4(Q4_DIR, Q4_STP, Q4_ENA);

    LimitSwitch ls1(Q1_LSW);
    LimitSwitch ls2(Q2_LSW);
    LimitSwitch ls3(Q3_LSW);
    LimitSwitch ls4(Q4_LSW);

    Axis axis1(motor_q1, ls1);
    Axis axis2(motor_q2, ls2);
    Axis axis3(motor_q3, ls3);
    Axis axis4(motor_q4, ls4);

}
