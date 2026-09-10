#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_HPP

/**
 * @file tester.hpp
 *
 * Requirements of the tester, the instrument that observes and stimulates the device under
 * test over the air. Everything in instrument.hpp applies here as well.
 *
 * The tester holds no protocol state. It transmits when it is told to, receives when it is
 * told to, and reports what it saw and when. The one thing it does on its own is answer a
 * received PDU after the fixed inter frame space, because no host can meet that deadline.
 *
 * That is the whole vocabulary for testing a radio, because a radio has no protocol state
 * either. Testing a link layer over the air later will need the tester to hold a connection,
 * which means adding to this vocabulary rather than moving the tests into the tester.
 *
 * @section clock The clock
 *
 * The timestamps the tester records are the measurement reference for the whole setup.
 * Their accuracy has to be good enough that the error of the tester is negligible against
 * the error being measured, which is why the board carries a better oscillator than the
 * part it is measuring. The clock is not exposed as a function: every time the host sees
 * is attached to something that was observed.
 *
 * How the tester itself is shown to be accurate is not answered here and has to be settled
 * before any measurement it produces can be believed.
 *
 * @section programs Programs
 *
 * The tester executes a program of its operations, loaded by the host and started with
 * start(). Operations run one after the other, each for the duration it names, the first
 * one from the moment the program was started. The program is finished when its last
 * operation ended.
 *
 * No operation is placed at a point in time. The tester has no origin that means anything
 * to a test, and the origin of the device under test only becomes visible to the tester
 * when a PDU arrives. An operation that has to transmit at a particular moment, such as
 * hitting a receive window the device opened, will therefore be expressed relative to a
 * received PDU. That operation does not exist yet.
 */

#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief one PDU the tester received, with the time it arrived
     */
    struct received_pdu
    {
        /**
         * @brief increases by one per received PDU, so that loss is detectable
         */
        std::uint32_t           sequence_number;

        /**
         * @brief the time the first bit of the PDU was on air
         *
         * In the tester's time domain, which is the reference for the setup.
         */
        link_layer::abs_time    when;

        const std::uint8_t*     data;
        std::size_t             size;

        /**
         * @brief false if the PDU was received with a CRC error
         *
         * A malformed PDU is still reported, because a test may be asserting that the
         * device under test transmits something wrong.
         */
        bool                    crc_ok;
    };

    /**
     * @{
     * @name Program vocabulary
     *
     * Each operation runs for the duration it names, from the moment the previous one
     * ended.
     */
    struct operation;

    /**
     * @brief listen on one channel for a given duration
     *
     * Every PDU received in that window is queued with the time its first bit was on
     * air, and collected by the host afterwards.
     */
    operation receive(
        std::uint32_t                           channel,
        link_layer::phy_ll_encoding::phy_ll_encoding_t phy,
        link_layer::delta_time                  listen_window );

    /**
     * @brief listen, and answer the first PDU received
     *
     * The response is transmitted `delay` after the end of the received PDU. This is the
     * only decision the tester makes on its own, and it is here because the deadline
     * cannot be met from the host.
     *
     * The received PDU is queued as usual, so a test can assert on what triggered the
     * response as well as on what the response caused.
     *
     * @param delay time between the end of the received PDU and the first bit of the
     *              response. Deliberately a parameter rather than fixed at the inter
     *              frame space, so that a test can answer early or late and find the
     *              edges of the receive window of the device under test.
     */
    operation respond_to_next(
        std::uint32_t                           channel,
        link_layer::phy_ll_encoding::phy_ll_encoding_t phy,
        link_layer::delta_time                  listen_window,
        link_layer::delta_time                  delay,
        const std::uint8_t*                     response,
        std::size_t                             response_size );
    /** @} */

    /**
     * @brief requirements of the tester
     */
    class tester
    {
    public:
        /**
         * @brief load a program, replacing any earlier one
         */
        template < typename... Operations >
        void program( Operations... operations );

        /**
         * @brief start the loaded program with its first operation
         */
        void start();

        /**
         * @brief stop the running program
         *
         * A test that has what it needs does not wait for the window it asked for to end.
         */
        void cancel();

        /**
         * @brief the access address and CRC init the tester uses
         *
         * Advertising uses the values the specification fixes; a test that wants to observe
         * or take part in a connection sets the ones that connection uses.
         */
        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

        /**
         * @brief hand over the PDUs received since the host last asked
         *
         * Oldest first, and the queue is emptied. An empty result is a meaningful answer:
         * it is how a test asserts that the device under test transmitted nothing.
         */
        std::size_t collect_received( received_pdu* out, std::size_t max_count );

        /**
         * @brief hold the reset input of the device under test asserted, then release it
         *
         * The tester owns the pin; the host decides when it is used. The host waits for the
         * boot counter of the device under test to change rather than for a fixed time.
         */
        void reset_device_under_test();
    };
}
}

#endif
