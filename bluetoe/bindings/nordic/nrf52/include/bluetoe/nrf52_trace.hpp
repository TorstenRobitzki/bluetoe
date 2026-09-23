#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_TRACE_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_TRACE_HPP

/**
 * @file nrf52_trace.hpp
 *
 * The inner workings of the radio on pins, for a logic analyser: with
 * BLUETOE_NRF52_RADIO_DEBUG defined, which the platform's CMake option of that name does,
 * on by default in a Debug build, the hardware's events reach the pins through PPI and
 * GPIOTE, and the interrupts, the clock switches and run() mark themselves in software;
 * without it, every function here is empty and costs nothing. A Release build can carry
 * the traces as well: the hardware drives most of them, so the timing stays what is
 * measured.
 *
 * On a part with a second port, the nRF52840 and nRF52833, the pins are P1.01 to P1.07,
 * which the development kit has on one header; on the others P0.25 to P0.31, on the
 * analog header.
 *
 * | nRF52840, nRF52833 | other parts | high while                                                            |
 * |--------------------|-------------|-----------------------------------------------------------------------|
 * | P1.01              | P0.25       | the high frequency crystal runs, from HFCLKSTARTED to its stop        |
 * | P1.02              | P0.26       | the RADIO is active, from READY to DISABLED                            |
 * | P1.03              | P0.27       | a packet is on air, from ADDRESS to END                                |
 * | P1.04              | P0.28       | the CCM works, from the end of its key stream to the end of the crypt |
 * | P1.05              | P0.29       | the radio's interrupts run, any of them                                |
 * | P1.06              | P0.30       | TIMER0 runs, from the RTC's compare that starts it to its release      |
 * | P1.07              | P0.31       | the application runs: run() was left and not entered again             |
 */

#include <nrf.h>

#include <cstddef>
#include <cstdint>

namespace bluetoe
{
    namespace nrf52_details
    {
        namespace trace
        {
#if defined( BLUETOE_NRF52_RADIO_DEBUG )
            // pins are numbered across the ports, 32 and up on the second
#   if defined( NRF_P1 )
            constexpr std::uint32_t first_pin = 32 + 1;
#   else
            constexpr std::uint32_t first_pin = 25;
#   endif
            constexpr std::uint32_t pin_hfxo        = first_pin + 0;
            constexpr std::uint32_t pin_radio       = first_pin + 1;
            constexpr std::uint32_t pin_packet      = first_pin + 2;
            constexpr std::uint32_t pin_ccm         = first_pin + 3;
            constexpr std::uint32_t pin_interrupt   = first_pin + 4;
            constexpr std::uint32_t pin_timer       = first_pin + 5;
            constexpr std::uint32_t pin_application = first_pin + 6;

            // one GPIOTE channel per pin the hardware drives; software drives the other two
            constexpr std::size_t   gpiote_hfxo     = 0;
            constexpr std::size_t   gpiote_radio    = 1;
            constexpr std::size_t   gpiote_packet   = 2;
            constexpr std::size_t   gpiote_ccm      = 3;
            constexpr std::size_t   gpiote_timer    = 4;

            // the programmable PPI channels after the radio's own
            constexpr std::size_t   ppi_first       = 3;

            inline NRF_GPIO_Type& port_of( std::uint32_t pin )
            {
#   if defined( NRF_P1 )
                return pin < 32 ? *NRF_P0 : *NRF_P1;
#   else
                return *NRF_P0;
#   endif
            }

            inline std::uint32_t bit_of( std::uint32_t pin )
            {
                return 1u << ( pin & 31 );
            }

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
                    | ( ( pin & 31 ) << GPIOTE_CONFIG_PSEL_Pos )
#   if defined( GPIOTE_CONFIG_PORT_Pos )
                    | ( ( pin >> 5 ) << GPIOTE_CONFIG_PORT_Pos )
#   endif
                    | ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos )
                    | ( GPIOTE_CONFIG_OUTINIT_Low << GPIOTE_CONFIG_OUTINIT_Pos );
            }

            inline void software_output( std::uint32_t pin )
            {
                port_of( pin ).OUTCLR = bit_of( pin );
                port_of( pin ).PIN_CNF[ pin & 31 ] =
                      ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
                    | ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos );
            }

            /**
             * @brief the pins as outputs and the hardware's events routed to them
             */
            inline void init()
            {
                software_output( pin_interrupt );
                software_output( pin_application );

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
                port_of( pin_interrupt ).OUTSET = bit_of( pin_interrupt );
            }

            inline void interrupt_left()
            {
                port_of( pin_interrupt ).OUTCLR = bit_of( pin_interrupt );
            }

            inline void run_entered()
            {
                port_of( pin_application ).OUTCLR = bit_of( pin_application );
            }

            inline void run_left()
            {
                port_of( pin_application ).OUTSET = bit_of( pin_application );
            }
#else
            inline void init() {}
            inline void hfxo_stopped() {}
            inline void timer_released() {}
            inline void interrupt_entered() {}
            inline void interrupt_left() {}
            inline void run_entered() {}
            inline void run_left() {}
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
