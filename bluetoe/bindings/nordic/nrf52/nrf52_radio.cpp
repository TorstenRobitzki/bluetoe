#include <bluetoe/nrf52_radio.hpp>

#include <nrf.h>

namespace bluetoe
{
    namespace nrf52_details
    {
        interrupt_entries interrupts = { nullptr, nullptr, nullptr };

        radio_lock_guard::radio_lock_guard()
            : primask_( __get_PRIMASK() )
        {
            __disable_irq();
        }

        radio_lock_guard::~radio_lock_guard()
        {
            __set_PRIMASK( primask_ );
        }

        void packet_counter::increment()
        {
            if ( ++low == 0 )
                high = ( high + 1 ) & 0x7f;
        }
    }
}

extern "C" void RADIO_IRQHandler()
{
    if ( bluetoe::nrf52_details::interrupts.radio )
        bluetoe::nrf52_details::interrupts.radio();
}

extern "C" void TIMER1_IRQHandler()
{
    if ( bluetoe::nrf52_details::interrupts.timer )
        bluetoe::nrf52_details::interrupts.timer();
}
