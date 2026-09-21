#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_PLATFORM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_TESTER_PLATFORM_HPP

/**
 * @file platform.hpp
 *
 * What the tester needs from its development kit besides the serial port, as
 * instrument/tester_rig.hpp requires it: the reset line to the device under test, the
 * idling, and the radio that listens and timestamps.
 *
 * The reset line is one pin of the development kit, wired to the RESET pin of the device
 * under test's kit, with the grounds joined. It is driven open drain: pulled low for the
 * reset, released to high impedance otherwise, so that it never fights the kit's own reset
 * circuitry, which sits on the same net.
 *
 * The radio listens on a channel, captures the timer at the moment the access address is
 * received, and reports each PDU with the time its first bit was on air in the tester's
 * ticks (decision 24); a timer compare ends the window and is reported as well. An answer
 * operation also answers the first advertising PDU from a named target one inter frame
 * space after it ended, started by a timer compare rather than by software, and
 * reports that transmission with its time from the same capture. A connection event
 * transmits its PDUs the same way, each one inter frame space after the device's reply to
 * the one before, and lets the radio switch to receive on its own after each. The radio
 * keeps nothing of the device's binding, which it must not: the tester observes a radio, it
 * is not one (decision 23).
 */

#include "instrument/tester_rig.hpp"
#include "link/pdu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace bluetoe {
namespace test_rig {

    class platform
    {
    public:
        /**
         * @brief configures the reset pin, the timer and the radio
         */
        platform();

        /**
         * @brief pulls the reset line low for a few milliseconds and releases it
         *
         * Long enough for the filtering on a development kit's reset net; the host polls
         * for the device afterwards, so the exact duration does not matter.
         */
        void reset_device_under_test();

        /**
         * @brief sleeps until an event; the port's interrupt and the radio's are two
         */
        void run();

        void wake_up();

        /**
         * @name The radio
         * @{
         */
        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );
        void receive( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
            std::uint32_t operation_id );
        void answer( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
            const link_layer::device_address& target, const adv_pdu& response, std::uint32_t operation_id );
        bool connection_event( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
            std::uint32_t at, const pdu* pdus, std::uint32_t count, std::uint32_t t_ifs,
            std::uint32_t crc_errors, std::uint32_t operation_id );
        void stop();
        void accept_advertiser( std::uint32_t slot, const link_layer::device_address& address );
        std::optional< tester_happened > next_event();
        /** @} */

        /**
         * @name For the interrupt handlers only
         * @{
         */
        static void radio_interrupt();
        static void timer_interrupt();
        /** @} */

    private:
        void prepare( std::uint32_t channel, link_layer::phy_ll_encoding::phy_ll_encoding_t phy, std::uint64_t ticks,
            std::uint32_t operation_id );
        void on_packet_end();
        void on_event_packet_end();
        std::uint32_t received_first_bit( std::uint32_t address ) const;
        void report_transmitted( const std::uint8_t* sent, bool crc_ok, std::uint32_t address );
        void report_received( std::uint32_t first_bit, bool crc_ok );
        void on_radio_ready();
        void on_radio_disabled();
        bool arm_event_transmission( std::uint32_t at );
        bool crc_error( std::uint32_t pdu_index ) const;
        void on_window_end();
        void on_device_address_miss();
        bool from_target() const;
        void arm_answer( std::uint32_t first_bit );
        void enqueue( const tester_happened& event );

        /*
         * The events the radio produced and run() has not drained yet, a single producer,
         * single consumer ring: the interrupts write the tail, next_event() reads the head.
         * A burst deeper than this between two run()s would drop the newest, which does not
         * happen while run() drains on every wake.
         */
        static constexpr std::size_t            event_ring_size = 16;
        tester_happened                         events_[ event_ring_size ];
        volatile std::uint32_t                  event_head_;
        volatile std::uint32_t                  event_tail_;
        volatile std::uint32_t                  operation_id_   = 0;

        std::uint8_t                            receive_buffer_[ max_pdu_size ];

        // whether the radio matches advertisers, which is when it reports a miss, and the
        // access address, on which it only does for advertising
        bool                                    matching_advertisers_ = false;
        std::uint32_t                           access_address_       = 0;
        std::uint32_t                           crc_init_             = 0;

        // the PHY of the current operation, which the timing of its packets depends on
        bool                                    two_mbit_             = false;

        /*
         * An answer operation's state, shared between answer(), the window end and the two
         * radio interrupts: whether this operation answers at all, whether it already did, and
         * whether the packet now on air is the answer, so that its END is told from a
         * reception's. The answer's bytes are kept where the radio can transmit them.
         */
        volatile bool                           answering_      = false;
        volatile bool                           answered_       = false;
        volatile bool                           transmitting_   = false;
        link_layer::device_address              target_;
        std::uint8_t                            response_buffer_[ max_pdu_size ];

        /*
         * A connection event's state, shared between connection_event() and the radio
         * interrupts: its PDUs where the radio can transmit them, the one on air or the next,
         * the inter frame space to keep after each reply, and which PDUs go out with an invalid
         * CRC. `transmitting_` tells the END of one of its PDUs from a reply's, as for an answer.
         */
        volatile bool                           connecting_     = false;
        std::uint8_t                            event_pdus_[ max_event_pdus ][ max_pdu_size ];
        std::uint32_t                           event_count_    = 0;
        volatile std::uint32_t                  event_next_     = 0;
        std::uint32_t                           event_t_ifs_        = 0;
        std::uint32_t                           event_crc_errors_   = 0;

        static platform*                        instance_;
    };
}
}

#endif
