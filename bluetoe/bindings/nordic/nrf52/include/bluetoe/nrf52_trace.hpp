#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_TRACE_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_TRACE_HPP

/**
 * @file nrf52_trace.hpp
 *
 * The inner workings of the radio on pins, for a logic analyser: with
 * BLUETOE_NRF52_RADIO_DEBUG defined, which the platform does for a Debug build, the
 * hardware's events reach six pins through PPI and GPIOTE, and the interrupts and the
 * clock switches mark themselves in software; without it, every function here is empty
 * and costs nothing.
 *
 * The pins are free on the nRF52840-DK and the nRF52-DK alike, on their analog headers:
 *
 * | pin   | high while                                                          |
 * |-------|---------------------------------------------------------------------|
 * | P0.26 | the high frequency crystal runs, from HFCLKSTARTED to its stop      |
 * | P0.27 | the RADIO is active, from READY to DISABLED                          |
 * | P0.28 | a packet is on air, from ADDRESS to END                              |
 * | P0.29 | the CCM works, from the end of its key stream to the end of the crypt |
 * | P0.30 | the radio's interrupts run, any of them                              |
 * | P0.31 | TIMER0 runs, from the RTC's compare that starts it to its release    |
 */

#include <nrf.h>

#include <cstdint>

namespace bluetoe
{
    namespace nrf52_details
    {
        namespace trace
        {
#if defined( BLUETOE_NRF52_RADIO_DEBUG )
            constexpr std::uint32_t pin_hfxo        = 26;
            constexpr std::uint32_t pin_radio       = 27;
            constexpr std::uint32_t pin_packet      = 28;
            constexpr std::uint32_t pin_ccm         = 29;
            constexpr std::uint32_t pin_interrupt   = 30;
            constexpr std::uint32_t pin_timer       = 31;

            // one GPIOTE channel per pin, but the interrupt pin, which software drives
            constexpr std::size_t   gpiote_hfxo     = 0;
            constexpr std::size_t   gpiote_radio    = 1;
            constexpr std::size_t   gpiote_packet   = 2;
            constexpr std::size_t   gpiote_ccm      = 3;
            constexpr std::size_t   gpiote_timer    = 4;

            // the programmable PPI channels after the radio's own
            constexpr std::size_t   ppi_first       = 3;

            inline void assign( std::size_t channel, volatile std::uint32_t& event, volatile std::uint32_t& task )
            {
                NRF_PPI->CH[ channel ].EEP = reinterpret_cast< std::uint32_t >( &event );
                NRF_PPI->CH[ channel ].TEP = reinterpret_cast< std::uint32_t >( &task );
                NRF_PPI->CHENSET           = 1u << channel;
            }

            inline void output( std::size_t gpiote, std::uint32_t pin )
            {
                NRF_GPIOTE->CONFIG[ gpiote ] =
                      ( GPIOTE_CONFIG_MODE_Task << GPIOTE_CONFIG_MODE_Pos )
                    | ( pin << GPIOTE_CONFIG_PSEL_Pos )
                    | ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos )
                    | ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );
            }

            /**
             * @brief the pins as outputs and the hardware's events routed to them
             */
            inline void init()
            {
                NRF_P0->OUTCLR = 1u << pin_interrupt;
                NRF_P0->PIN_CNF[ pin_interrupt ] =
                      ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
                    | ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos );

                output( gpiote_hfxo,   pin_hfxo );
                output( gpiote_radio,  pin_radio );
                output( gpiote_packet, pin_packet );
                output( gpiote_ccm,    pin_ccm );
                output( gpiote_timer,  pin_timer );

                std::size_t channel = ppi_first;
                assign( channel++, NRF_CLOCK->EVENTS_HFCLKSTARTED,  NRF_GPIOTE->TASKS_SET[ gpiote_hfxo ] );
                assign( channel++, NRF_RADIO->EVENTS_READY,         NRF_GPIOTE->TASKS_SET[ gpiote_radio ] );
                assign( channel++, NRF_RADIO->EVENTS_DISABLED,      NRF_GPIOTE->TASKS_CLR[ gpiote_radio ] );
                assign( channel++, NRF_RADIO->EVENTS_ADDRESS,       NRF_GPIOTE->TASKS_SET[ gpiote_packet ] );
                assign( channel++, NRF_RADIO->EVENTS_END,           NRF_GPIOTE->TASKS_CLR[ gpiote_packet ] );
                assign( channel++, NRF_CCM->EVENTS_ENDKSGEN,        NRF_GPIOTE->TASKS_SET[ gpiote_ccm ] );
                assign( channel++, NRF_CCM->EVENTS_ENDCRYPT,        NRF_GPIOTE->TASKS_CLR[ gpiote_ccm ] );
                assign( channel++, NRF_RTC0->EVENTS_COMPARE[ 1 ],   NRF_GPIOTE->TASKS_SET[ gpiote_timer ] );
            }

            inline void hfxo_stopped()
            {
                NRF_GPIOTE->TASKS_CLR[ gpiote_hfxo ] = 1;
            }

            inline void timer_released()
            {
                NRF_GPIOTE->TASKS_CLR[ gpiote_timer ] = 1;
            }

            inline void interrupt_entered()
            {
                NRF_P0->OUTSET = 1u << pin_interrupt;
            }

            inline void interrupt_left()
            {
                NRF_P0->OUTCLR = 1u << pin_interrupt;
            }
#else
            inline void init() {}
            inline void hfxo_stopped() {}
            inline void timer_released() {}
            inline void interrupt_entered() {}
            inline void interrupt_left() {}
#endif

            /**
             * @brief marks an interrupt on the interrupt pin for as long as the object lives
             */
            struct in_interrupt
            {
                in_interrupt()
                {
                    interrupt_entered();
                }

                ~in_interrupt()
                {
                    interrupt_left();
                }

                in_interrupt( const in_interrupt& ) = delete;
                in_interrupt& operator=( const in_interrupt& ) = delete;
            };
        }
    }
}

#endif
