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

    // Configurar el timer
    Timer timer1(Q1_STP);
    Timer timer2(Q2_STP);
    Timer timer3(Q3_STP);
    // Timer timer4(Q4_STP);

    // Iniciar el timer
    timer1.start();
    timer2.start();
    timer3.start();
    // timer4.start();

    // Inicializar el servidor REST
    init_server();

    Motor motor_q1(Q1_DIR, Q1_STP, Q1_ENA);
    Motor motor_q2(Q2_DIR, Q2_STP, Q2_ENA);
    Motor motor_q3(Q3_DIR, Q3_STP, Q3_ENA);
    Motor motor_q4(Q4_DIR, Q4_STP, Q4_ENA);

    LimitSwitch ls1("q1",Q1_LSW);
    LimitSwitch ls2("q2",Q2_LSW);
    LimitSwitch ls3("q3",Q3_LSW);
    LimitSwitch ls4("q4",Q4_LSW);

    Axis axis1("axis_q1",motor_q1, ls1);
    Axis axis2("axis_q2",motor_q2, ls2);
    Axis axis3("axis_q3",motor_q3, ls3);
    Axis axis4("axis_q4",motor_q4, ls4);

    axis1.auto_home();
    axis2.auto_home();
    axis3.auto_home();
    axis4.auto_home();
}
