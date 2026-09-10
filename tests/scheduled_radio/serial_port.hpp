#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_SERIAL_PORT_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_SERIAL_PORT_HPP

/**
 * @file serial_port.hpp
 *
 * Requirements of the one platform dependent part of an instrument: the serial port the
 * rig talks to the host through. See documentation/scheduled_radio_test_rig.md, decision 16.
 *
 * An instrument has three parts. The scheduled radio implementation, which is the subject.
 * The rig, which is the same on every platform: framing, the request and response protocol,
 * the program interpreter, the records, the session token. And this port, which is written
 * once per platform and is meant to be as small as a UART setup and an interrupt handler
 * with two branches.
 *
 * @section contract The contract
 *
 * The port is event driven. It owns no buffer and makes no decision: it is constructed on
 * two ring buffers the rig owns, pushes every received byte into one and pulls bytes to
 * transmit from the other as long as there are any.
 *
 * Nothing is lost. Each buffer tells how much it can take and how much it holds, and the
 * port holds the host off while the receive buffer cannot take what arrives: USB CDC does
 * that by not acknowledging, a UART with RTS/CTS by deasserting. A port without any means
 * of back pressure relies on the protocol, which guarantees it anyway: a request is never
 * larger than the receive buffer, and the host does not send the next one before it has
 * the answer to the last.
 *
 * The port touches the buffers from a context whose priority is below the priority the
 * radio uses. This is the one rule the port has to honour, and it is the port's business
 * how; on a part with prioritised interrupts it is the priority of the UART interrupt.
 *
 * The same port serves the tester, whose link is the same code.
 *
 * @section platform What else is platform dependent
 *
 * One thing outside this file, the port's business as well: whatever wiring makes the
 * reset input actually reset the device (on the nRF52, PSELRESET in the UICR).
 */

#include <cstddef>
#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a ring buffer of bytes with one producer and one consumer
     *
     * Owned by the rig, shared with the port. One side only pushes, the other only pops,
     * from two different contexts, and no call blocks or disables interrupts.
     *
     * The sizes both sides ask for are lower bounds: the other side may have made more
     * room, or added more data, since the answer was computed, but never less. That is
     * what lets each side act on the answer without any further synchronisation.
     */
    class byte_ring_buffer
    {
    public:
        /**
         * @brief number of bytes that can be pushed right now, at least
         */
        std::size_t free() const;

        /**
         * @brief append bytes
         *
         * @pre size <= free()
         */
        void push( const std::uint8_t* data, std::size_t size );

        /**
         * @brief number of bytes that can be popped right now, at least
         */
        std::size_t available() const;

        /**
         * @brief remove the oldest bytes
         *
         * @pre size <= available()
         */
        void pop( std::uint8_t* out, std::size_t size );
    };

    /**
     * @brief what a platform has to provide
     */
    class serial_port
    {
    public:
        /**
         * @brief bind the port to the rig's buffers
         *
         * The port pushes into `receive` and pops from `transmit`. Both outlive the port.
         */
        serial_port( byte_ring_buffer& receive, byte_ring_buffer& transmit );

        /**
         * @brief configure the port and begin
         *
         * From here on everything that arrives is pushed into the receive buffer, with the
         * host held off while free() is smaller than what would arrive, and whenever the
         * port can send it pops from the transmit buffer until available() is zero.
         */
        void start();

        /**
         * @brief the rig pushed into the transmit buffer while the port was idle
         *
         * The port resumes popping. No effect if the port is transmitting already.
         */
        void transmit_pending();
    };
}
}

#endif
