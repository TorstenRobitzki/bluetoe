#include "spl/temperature.hpp"

#include <nrf.h>

static std::uint32_t sensor_value = 0;

void spl::details::nrf52_temperature_base::init()
{
    NVIC_SetPriority( TEMP_IRQn, 3 );
    NVIC_ClearPendingIRQ( TEMP_IRQn );
    NVIC_EnableIRQ( TEMP_IRQn );
    NRF_TEMP->INTENSET    = TEMP_INTENSET_DATARDY_Set;
    NRF_TEMP->TASKS_START = 1;
}

std::uint32_t spl::details::nrf52_temperature_base::value()
{
    return NRF_TEMP->TEMP;
}

extern "C" void TEMP_IRQHandler(void)
{
    NRF_TEMP->EVENTS_DATARDY = 0;
    NRF_TEMP->TASKS_START = 1;
}
