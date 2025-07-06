extern "C"
{
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"
#include "driver/gpio.h"
#include "sync_example.h"
#include "sync_pwm.h"
}

class Test
{
private:
    int a;

public:
    Test(int a) : a(a) {}
    int getA() const { return a; }
    void setA(int a) { this->a = a; }
};

extern "C" void app_main(void)
{
    example();
}
