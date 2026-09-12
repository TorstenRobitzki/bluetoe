#include "dut_rigs/nrf52/uart.hpp"

#include <nrf.h>

#include <cassert>

namespace bluetoe {
namespace test_rig {
namespace nrf52 {

    namespace {

        /*
         * The pins the development kits route to the J-Link's virtual COM port: the same four
         * on the nRF52840-DK (PCA10056) and the nRF52-DK (PCA10040).
         */
        constexpr std::uint32_t pin_rxd = 8;
        constexpr std::uint32_t pin_txd = 6;
        constexpr std::uint32_t pin_cts = 7;
        constexpr std::uint32_t pin_rts = 5;

        /*
         * Below the radio's interrupts, as decision 16 requires; the radio uses the highest
         * priorities.
         */
        constexpr std::uint32_t interrupt_priority = 6;

        constexpr std::uint32_t receive_interrupt = UART_INTENSET_RXDRDY_Msk;

        void configure_output( std::uint32_t pin )
        {
            NRF_P0->OUTSET = 1u << pin;
            NRF_P0->PIN_CNF[ pin ] =
                  ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos )
                | ( GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos );
        }

        void configure_input( std::uint32_t pin )
        {
            NRF_P0->PIN_CNF[ pin ] =
                  ( GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos )
                | ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
                | ( GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos );
        }
    }

    namespace details {

        uart_base* uart_base::instance_ = nullptr;

        uart_base::uart_base()
            : transmitting_( false )
        {
            assert( instance_ == nullptr );
            instance_ = this;
        }

        uart_base::~uart_base()
        {
            instance_ = nullptr;
        }

        void uart_base::start()
        {
            configure_output( pin_txd );
            configure_output( pin_rts );
            configure_input( pin_rxd );
            configure_input( pin_cts );

            NRF_UART0->PSEL.TXD = pin_txd;
            NRF_UART0->PSEL.RXD = pin_rxd;
            NRF_UART0->PSEL.RTS = pin_rts;
            NRF_UART0->PSEL.CTS = pin_cts;

            NRF_UART0->BAUDRATE = UART_BAUDRATE_BAUDRATE_Baud115200;
            NRF_UART0->CONFIG   = UART_CONFIG_HWFC_Enabled << UART_CONFIG_HWFC_Pos;
            NRF_UART0->ENABLE   = UART_ENABLE_ENABLE_Enabled << UART_ENABLE_ENABLE_Pos;

            NRF_UART0->EVENTS_RXDRDY = 0;
            NRF_UART0->EVENTS_TXDRDY = 0;
            NRF_UART0->EVENTS_ERROR  = 0;
            NRF_UART0->INTENSET      = UART_INTENSET_TXDRDY_Msk | UART_INTENSET_ERROR_Msk;

            NVIC_SetPriority( UARTE0_UART0_IRQn, interrupt_priority );
            NVIC_ClearPendingIRQ( UARTE0_UART0_IRQn );
            NVIC_EnableIRQ( UARTE0_UART0_IRQn );

            NRF_UART0->TASKS_STARTTX = 1;
            NRF_UART0->TASKS_STARTRX = 1;

            listen();
        }

        void uart_base::listen()
        {
            NRF_UART0->INTENSET = receive_interrupt;
        }

        /*
         * The interrupt decides whether transmission goes on, so the decision to start one
         * is taken with it masked; otherwise a byte pushed between its check and its
         * "idle" verdict would wait for a transmission that never comes.
         */
        void uart_base::transmit_pending()
        {
            NVIC_DisableIRQ( UARTE0_UART0_IRQn );

            std::uint8_t byte = 0;

            if ( !transmitting_ && pop_to_transmit( byte ) )
            {
                transmitting_    = true;
                NRF_UART0->TXD   = byte;
            }

            listen();

            NVIC_EnableIRQ( UARTE0_UART0_IRQn );
        }

        void uart_base::interrupt_handler()
        {
            if ( instance_ )
                instance_->interrupt();
        }

        void uart_base::interrupt()
        {
            bool received = false;

            /*
             * One byte per event. A byte the rig has no room for stays in the UART's FIFO,
             * and the port stops listening; the hardware then holds the host off through
             * RTS once the FIFO is full, and transmit_pending() listens again.
             */
            while ( NRF_UART0->EVENTS_RXDRDY )
            {
                if ( !has_room() )
                {
                    NRF_UART0->INTENCLR = receive_interrupt;
                    break;
                }

                NRF_UART0->EVENTS_RXDRDY = 0;
                push_received( static_cast< std::uint8_t >( NRF_UART0->RXD ) );
                received = true;
            }

            if ( NRF_UART0->EVENTS_TXDRDY )
            {
                NRF_UART0->EVENTS_TXDRDY = 0;

                std::uint8_t byte = 0;

                if ( pop_to_transmit( byte ) )
                    NRF_UART0->TXD = byte;
                else
                    transmitting_ = false;
            }

            if ( NRF_UART0->EVENTS_ERROR )
            {
                // a framing or overrun error corrupts a frame, which its CRC catches; nothing to do here
                NRF_UART0->EVENTS_ERROR = 0;
                NRF_UART0->ERRORSRC     = NRF_UART0->ERRORSRC;
            }

            if ( received )
                wake();
        }
    }
}
}
}

extern "C" void UARTE0_UART0_IRQHandler()
{
    bluetoe::test_rig::nrf52::details::uart_base::interrupt_handler();
}
