#include "hal/io.hpp"
#include <iostream>

void init_led()
{
    std::cout << "init_led()" << std::endl;
}

void set_led( bool state )
{
    std::cout << "LED: " << ( state ? "on" : "off" ) << std::endl;
}

