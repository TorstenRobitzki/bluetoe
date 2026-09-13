#include "tester/platform.hpp"

#include <nrf.h>

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    namespace {

        /*
         * P0.03: free on the nRF52840-DK and the nRF52-DK alike, on the analog header of
         * both.
         */
        constexpr std::uint32_t pin_reset = 3;

        /*
         * The CPU runs at 64 MHz whatever clock source drives it, so SysTick counts
         * milliseconds without any setup of the clocks.
         */
        constexpr std::uint32_t cpu_ticks_per_ms = 64000;
        constexpr std::uint32_t reset_hold_ms    = 5;

        void wait_ms( std::uint32_t ms )
        {
            SysTick->LOAD = cpu_ticks_per_ms - 1;
            SysTick->VAL  = 0;
            SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

            for ( ; ms; --ms )
                while ( !( SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk ) )
                    ;

            SysTick->CTRL = 0;
        }
    }

    platform::platform()
    {
        // released before the pin becomes an output, so that it never drives low by accident
        NRF_P0->OUTSET = 1u << pin_reset;
        NRF_P0->PIN_CNF[ pin_reset ] =
              ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
            | ( GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos )
            | ( GPIO_PIN_CNF_DRIVE_S0D1 << GPIO_PIN_CNF_DRIVE_Pos );
    }

    void platform::reset_device_under_test()
    {
        NRF_P0->OUTCLR = 1u << pin_reset;
        wait_ms( reset_hold_ms );
        NRF_P0->OUTSET = 1u << pin_reset;
    }

    void platform::run()
    {
        __WFE();
    }

    void platform::wake_up()
    {
        __SEV();
    }
}
}
