#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP

/**
 * @file nrf52_radio.hpp
 *
 * The scheduled radio of the nRF52, as bluetoe/scheduled_radio2.hpp requires it. Consumers
 * name it through <bluetoe/radio.hpp>.
 *
 * This is the advertising slice of decision 11, step 3: the time base, radio_ready(),
 * start_advertising() and schedule_advertising_event() with their receive window and the
 * scan response, the timer, and the callbacks, delivered from run(). Connection events,
 * encryption and PHY changes are present and decline.
 * See documentation/scheduled_radio_test_rig.md.
 *
 * @section timebase The time base
 *
 * abs_time is a 32 bit timer running at one microsecond from the 16 MHz peripheral
 * clock, and the high frequency crystal is started once and stays on: a test rig has no
 * power budget, and the sleep clock with its calibration and the handover between the
 * two clocks is a later slice with tests of its own. A second timer, started in the same
 * cycle as the first through a PPI fork, holds the user timer's compare, since the first
 * one's four registers are taken by the radio.
 *
 * @section events What the radio reports and when
 *
 * An advertising event transmits with its first bit on air at the requested time, then
 * opens the receiver for the inter frame space plus the longest legacy response. What is
 * reported with adv_received() is a scan request with a valid CRC that follows a scannable
 * advertisement, carries this device's address as the one it is addressed to, and comes
 * from a sender the acceptance filter passes (scheduled_radio2.hpp); it carries the time
 * its first bit was on air, computed back from the end of the packet. Anything else, a bad
 * CRC, another kind of PDU, one addressed elsewhere, a sender the filter rejects, or no PDU
 * at all, is reported with adv_timeout(), carrying the time the event's own transmission
 * began, so that a caller can chain intervals from it without knowing the window.
 *
 * A request that is accepted is answered with the scan response one inter frame space
 * after it ended, placed by the radio's own spacing rather than by software: the reception
 * is judged at the end of the packet, while the radio is still disabling, and the answer is
 * armed there so that the disable ramps the transmitter up. adv_received() is reported once
 * the answer is out, which is what makes the event's end mean the air is quiet again.
 *
 * A connection request is not recognised as a response yet, because there is nothing this
 * slice could do with a connection; that comes with the connection events.
 */

#include <bluetoe/security_tool_box.hpp>

#include <bluetoe/abs_time.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/buffer.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace bluetoe
{
    namespace nrf52_details
    {
        /**
         * @brief what does not depend on the callbacks type: the hardware and its state
         *
         * The interrupts write what happened into one slot per kind of event, and the
         * template's run() takes it from there and delivers it; the concepts' rule that
         * no second action of a kind is scheduled while one is pending is what makes one
         * slot enough.
         */
        class radio_base
        {
        public:
            /**
             * @brief for the interrupt handlers only
             */
            static void radio_interrupt();
            static void timer_interrupt();

        protected:
            /**
             * @brief starts the crystal, the timers and the random number generator, and
             *        configures the radio for legacy advertising
             */
            radio_base();

            /**
             * @brief sleeps until an interrupt happened or wake_up() was called
             *
             * Wait-for-event returns on an interrupt and on wake_up(); the event register
             * latches a wake_up() that came before the sleep, which is what makes the
             * guarantee of decision 17 hold.
             */
            void sleep();

            /**
             * @brief makes run() return, from any context
             */
            void wake_up();

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            /**
             * @brief registers the acceptance filter the template calls into
             *
             * The receive interrupt lives in this base, which has no callbacks type; the
             * template that has it stores a thunk here that reaches the callbacks'
             * is_in_acceptance_filter() (scheduled_radio2.hpp). A null thunk accepts every
             * sender, which is what an empty filter set means.
             */
            void set_acceptance_filter( bool ( *filter )( radio_base*, const link_layer::device_address& ) );

            /**
             * @brief the address this device advertises from
             *
             * A scan request is answered only if it is addressed to this address, which
             * the receive interrupt compares against the AdvA the request carries.
             */
            void set_local_address( const link_layer::device_address& address );

            bool start_advertising(
                std::uint32_t                       channel,
                const link_layer::write_buffer&     transmit,
                const link_layer::write_buffer&     response,
                const link_layer::read_buffer&      receive );

            bool schedule_advertising_event(
                std::uint32_t                       channel,
                link_layer::abs_time                when,
                const link_layer::write_buffer&     transmit,
                const link_layer::write_buffer&     response,
                const link_layer::read_buffer&      receive );

            bool cancel_radio_event();
            bool schedule_timer( link_layer::abs_time when );
            bool cancel_timer();

            enum class event
            {
                radio_ready,
                adv_received,
                adv_timeout,
                user_timer
            };

            struct happened
            {
                event                   kind;
                link_layer::abs_time    when;
                link_layer::read_buffer received;
            };

            /**
             * @brief the oldest thing to deliver, and it is forgotten
             */
            std::optional< happened > next_event();

        private:
            enum class state
            {
                idle,
                transmitting,
                receiving,
                responding
            };

            link_layer::abs_time now() const;
            bool schedule( std::uint32_t channel, link_layer::abs_time when, const link_layer::write_buffer& transmit, const link_layer::write_buffer& response, const link_layer::read_buffer& receive );
            bool sender_in_acceptance_filter();
            bool is_scan_request_for_us() const;
            void on_packet_end();
            void on_radio_disabled();
            void end_event();
            void on_timer_expired();

            volatile state              state_;
            link_layer::read_buffer     receive_;
            link_layer::write_buffer    response_;
            link_layer::abs_time        transmit_time_;

            /*
             * Whether the reception was one to report, and whether the answer to it is
             * armed. Both are decided at the end of the received packet and read by the
             * disable that follows it.
             */
            volatile bool               accepted_;
            volatile bool               answering_;

            volatile bool               ready_pending_;
            volatile bool               radio_event_pending_;
            event                       radio_event_;
            link_layer::abs_time        radio_event_time_;
            std::size_t                 received_size_;

            volatile bool               timer_scheduled_;
            link_layer::abs_time        timer_when_;
            volatile bool               timer_event_pending_;

            bool ( *acceptance_filter_ )( radio_base*, const link_layer::device_address& );

            /*
             * Set before an event and read by the receive interrupt: the address a scan
             * request has to be addressed to, and whether the advertisement this event
             * transmitted could be scanned at all.
             */
            link_layer::device_address  local_address_;
            volatile bool               scannable_;

            static radio_base*          instance_;
        };

        /**
         * @brief the scheduled radio of the nRF52
         *
         * CallBacks is the type the callbacks are delivered to, which derives from this class
         * and is reached through that relation. Options are the radio's options; there are
         * none yet.
         */
        template < typename CallBacks, typename... Options >
        class radio : public radio_base, public security_tool_box
        {
            static_assert( sizeof...( Options ) == 0, "the nRF52 scheduled radio has no options yet" );

        public:
            static constexpr bool           hardware_supports_encryption                = false;
            static constexpr bool           hardware_supports_lesc_pairing              = true;
            static constexpr bool           hardware_supports_legacy_pairing            = false;
            static constexpr bool           hardware_supports_2mbit                     = false;
            static constexpr bool           hardware_supports_synchronized_user_timer   = false;
            static constexpr bool           hardware_supports_link_layer_context        = false;

            /**
             * @brief the hardware generates preamble, access address and CRC itself and stores
             *        only the PDU
             */
            static constexpr std::size_t    radio_package_overhead                      = 0;
            static constexpr std::uint32_t  radio_max_supported_payload_length          = 255;

            /**
             * @brief the 32 MHz crystal of the development kits, which is the only clock
             *        this slice runs on
             */
            static constexpr std::uint32_t  sleep_time_accuracy_ppm                     = 20;

            /**
             * @brief no acceptance filter hardware; the caller filters in software through
             *        is_in_acceptance_filter() (scheduled_radio2.hpp)
             */
            static constexpr std::size_t    radio_maximum_acceptance_filter_entries     = 0;

            struct ccm_counter_t
            {
                std::uint64_t value = 0;
            };

            /**
             * @brief nothing to exclude: the callbacks are delivered from run()
             */
            struct lock_guard {};

            /**
             * @brief hands the base a thunk to the callbacks' acceptance filter
             *
             * The receive interrupt lives in radio_base, which does not know CallBacks;
             * this gives it a way to reach is_in_acceptance_filter() (scheduled_radio2.hpp).
             */
            radio()
            {
                radio_base::set_acceptance_filter( &apply_acceptance_filter );
            }

            /**
             * @brief delivers what happened, then sleeps until the next thing happens
             *
             * radio_ready() is delivered on the first call. An interrupt between the last
             * delivery and the sleep sets the event register, so the sleep ends at once
             * and the next call delivers what it left.
             */
            void run()
            {
                for ( std::optional< happened > next = radio_base::next_event(); next; next = radio_base::next_event() )
                {
                    CallBacks& callbacks = static_cast< CallBacks& >( *this );

                    switch ( next->kind )
                    {
                    case event::radio_ready:
                        callbacks.radio_ready();
                        break;
                    case event::adv_received:
                        callbacks.adv_received( next->when, next->received );
                        break;
                    case event::adv_timeout:
                        callbacks.adv_timeout( next->when );
                        break;
                    case event::user_timer:
                        callbacks.user_timer( next->when );
                        break;
                    }
                }

                radio_base::sleep();
            }

            using radio_base::wake_up;
            using radio_base::set_access_address_and_crc_init;
            using radio_base::set_local_address;
            using radio_base::start_advertising;
            using radio_base::schedule_advertising_event;
            using radio_base::cancel_radio_event;
            using radio_base::schedule_timer;
            using radio_base::cancel_timer;

            /**
             * @name Not implemented in this slice
             *
             * Present so that the class satisfies the concept; the scheduling function
             * declines.
             * @{
             */
            void set_ccm_counter( const ccm_counter_t&, const ccm_counter_t& ) {}
            void set_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t, link_layer::phy_ll_encoding::phy_ll_encoding_t ) {}

            bool schedule_connection_event( std::uint32_t, link_layer::abs_time, link_layer::abs_time )
            {
                return false;
            }
            /** @} */

        private:
            /*
             * The thunk the base calls to apply the acceptance filter, the one thing the
             * receive interrupt needs from the callbacks type it cannot name itself.
             */
            static bool apply_acceptance_filter( radio_base* base, const link_layer::device_address& address )
            {
                return static_cast< CallBacks& >( static_cast< radio& >( *base ) ).is_in_acceptance_filter( address );
            }
        };
    }
}

#endif
