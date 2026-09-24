#include <bluetoe/nrf52_radio.hpp>

#include <nrf.h>

#include <cassert>

namespace bluetoe
{
    namespace nrf52_details
    {
        interrupt_entries interrupts = { nullptr, nullptr, nullptr, nullptr };

        void packet_counter::increment()
        {
            if ( ++low == 0 )
                high = ( high + 1 ) & 0x7f;
        }
    }
}

// the radio's constructor fills the table before it enables the interrupts
extern "C" void RADIO_IRQHandler()
{
    assert( bluetoe::nrf52_details::interrupts.radio );
    bluetoe::nrf52_details::interrupts.radio();
}

extern "C" void RTC0_IRQHandler()
{
    assert( bluetoe::nrf52_details::interrupts.rtc );
    bluetoe::nrf52_details::interrupts.rtc();
}

extern "C" void POWER_CLOCK_IRQHandler()
{
    assert( bluetoe::nrf52_details::interrupts.clock );
    bluetoe::nrf52_details::interrupts.clock();
}
