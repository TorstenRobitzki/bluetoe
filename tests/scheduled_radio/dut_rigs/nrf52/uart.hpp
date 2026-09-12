#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_DUT_RIGS_NRF52_UART_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_DUT_RIGS_NRF52_UART_HPP

/**
 * @file uart.hpp
 *
 * The serial port of the nRF52 rigs, as link/serial_port.hpp requires it: the UART in its
 * legacy mode, one byte per interrupt in both directions, with hardware flow control. On the
 * development kits the pins are the ones routed to the J-Link's virtual COM port, so the
 * probe that flashes a rig is also the host's serial device.
 *
 * The port is a template over the rig's buffer type and the type it wakes. The interrupt
 * handler cannot be, so the part that touches the hardware is a base class the handler
 * reaches through a pointer to the one instance, and the template supplies what needs the
 * types: room in the receive buffer, the next byte to transmit, the wake-up.
 */

#include "link/serial_port.hpp"

#include <cstdint>

namespace bluetoe {
namespace test_rig {
namespace nrf52 {

    namespace details {

        class uart_base
        {
        public:
            /**
             * @brief configures the pins and the UART and starts receiving
             */
            void start();

            /**
             * @brief begins transmitting if the port is idle, and listens for requests again
             *
             * The rig calls this once it pushed a response, that is once it consumed the
             * request that filled the receive buffer; so this is also the moment to take
             * bytes from the host again.
             */
            void transmit_pending();

            /**
             * @brief the one instance's interrupt, for the handler only
             */
            static void interrupt_handler();

        protected:
            /*
             * No destructor: a firmware never destroys its port, and a destructor would make
             * the rig non-trivially destructible, which asks the runtime to register it.
             */
            uart_base();

            virtual bool has_room() const = 0;
            virtual void push_received( std::uint8_t byte ) = 0;
            virtual bool pop_to_transmit( std::uint8_t& byte ) = 0;
            virtual void wake() = 0;

        private:
            void interrupt();
            void listen();

            bool                transmitting_;
            static uart_base*   instance_;
        };
    }

    template < typename Buffer, typename Wake >
    class uart : public details::uart_base
    {
    public:
        uart( Buffer& receive, Buffer& transmit, Wake& wake )
            : receive_( receive )
            , transmit_( transmit )
            , wake_( wake )
        {
        }

    private:
        bool has_room() const override
        {
            return receive_.free() != 0;
        }

        void push_received( std::uint8_t byte ) override
        {
            receive_.push( &byte, 1 );
        }

        bool pop_to_transmit( std::uint8_t& byte ) override
        {
            if ( transmit_.available() == 0 )
                return false;

            transmit_.pop( &byte, 1 );

            return true;
        }

        void wake() override
        {
            wake_.wake_up();
        }

        Buffer& receive_;
        Buffer& transmit_;
        Wake&   wake_;
    };
}
}
}

#endif
