#include <iostream>
extern "C"
{
#include "driver/mcpwm_prelude.h"
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
    std::cout << "Hello, World!" << std::endl;
    example();
}
