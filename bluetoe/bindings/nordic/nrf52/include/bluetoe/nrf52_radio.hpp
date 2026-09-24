#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP

/**
 * @file nrf52_radio.hpp
 *
 * The scheduled radio of the nRF52, as bluetoe/scheduled_radio2.hpp requires it. Consumers
 * name it through <bluetoe/radio.hpp>.
 *
 * What is implemented: the time base, radio_ready(), start_advertising_event() and
 * schedule_advertising_event() with their receive window and the scan response, the timer,
 * the callbacks, delivered from run(), connection events at 1 or 2 Mbit, and, with the
 * option `encrypting`, their encryption (nrf52_ccm.hpp).
 *
 * Every PDU is stored with a spare byte between header and payload, encrypted_pdu_layout,
 * which the CCM needs and the RADIO keeps in memory without sending it; a radio that does
 * not encrypt stores PDUs the same way, so that it has one packet format.
 *
 * @section timebase The time base
 *
 * Two clocks: the RTC on the 32.768 kHz sleep clock runs all the time, and TIMER0 on the
 * 16 MHz peripheral clock runs during a radio event only. abs_time is microseconds since
 * the RTC started, on a 32 bit ring: while the radio is idle it is the RTC's ticks
 * converted, to the tick; during an event it is TIMER0's count from the tick the event
 * was placed at, to the microsecond.
 *
 * A radio event is placed like this: the microsecond it needs TIMER0 from is split into
 * a tick and a remainder; an RTC compare starts the high frequency crystal a startup
 * time before that tick, a second compare starts TIMER0, cleared, at the tick, through
 * PPI, and TIMER0's compares place the transmitter, the receiver and the window from
 * the remainder on. When the event ends, TIMER0 stops and the crystal is switched off
 * again, unless the sleep clock is synthesized from it (nrf.hpp), and the CPU can sleep
 * with the sleep clock alone. The user timer is a third RTC compare. The sleep clock's
 * source and the crystal's startup time are the options of nrf.hpp.
 *
 * @section trace Watching it
 *
 * With the CMake option BLUETOE_NRF52_RADIO_DEBUG the crystal, the RADIO, the packets,
 * the CCM, the interrupts, TIMER0 and the time outside run() show on seven pins for a
 * logic analyser; nrf52_trace.hpp says which.
 *
 * @section statistics Counting the clocks
 *
 * With bluetoe::nrf::clock_statistics the radio counts the starts of the high frequency
 * crystal, its running time in periods of the sleep clock and the calibrations of the RC
 * sleep clock, and hands them out through clock_statistics(): what the soak test of the
 * radio reads to see the crystal off between events. Nothing without the option.
 *
 * @section events What the radio reports and when
 *
 * An advertising event transmits with its first bit on air at the requested time, then
 * opens the receiver for the inter frame space plus the longest legacy response. What is
 * reported with adv_received() is a scan request that follows a scannable advertisement,
 * or a connect request that follows a connectable one, with a valid CRC, carrying this
 * device's address as the one it is addressed to, and from a sender the acceptance filter
 * passes (scheduled_radio2.hpp); it carries the time its first bit was on air, computed
 * back from the end of the packet. Anything else, a bad CRC, another kind of PDU, one
 * addressed elsewhere, a sender the filter rejects, or no PDU at all, is reported with
 * adv_timeout(), carrying the time the event's own transmission began, so that a caller
 * can chain intervals from it without knowing the window.
 *
 * A request that is accepted is answered with the scan response one inter frame space
 * after it ended, placed by the radio's TIFS rather than by software. TIFS holds only when
 * the shorts from the end of the packet to the transmitter's ramp up are in place before
 * the packet ends, so the answer is armed when a packet's address is received, and the
 * reception is judged at its end: the response is set, or the transmitter is cancelled.
 * adv_received() is reported once the answer is out, which is what makes the event's end
 * mean the air is quiet again.
 *
 * A connect request is not answered; the event ends with it, and the link layer takes it
 * from there.
 *
 * A connection event receives from its start and answers every PDU received one inter
 * frame space later, with a PDU from the link layer's buffer, the same way: the shorts to
 * the transmitter are armed at the address of a packet, and its end decides what is sent.
 * It closes after an answer when neither side has more data, or when nothing is received
 * in the inter frame space after the answer, and without an answer after the second PDU in
 * a row with an invalid CRC. The anchor it reports is the first bit of the first PDU
 * received.
 */

#include <bluetoe/security_tool_box.hpp>
#include <bluetoe/nrf52_ccm.hpp>
#include <bluetoe/nrf52_trace.hpp>
#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>

#include <bluetoe/abs_time.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/buffer.hpp>
#include <bluetoe/connection_events.hpp>
#include <bluetoe/ll_constants.hpp>
#include <bluetoe/scheduled_radio2.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace bluetoe
{
    namespace nrf52_details
    {
        /**
         * @brief all interrupts off while an instance is alive, the mask restored afterwards
         */
        class radio_lock_guard
        {
        public:
            radio_lock_guard();
            ~radio_lock_guard();

            radio_lock_guard( const radio_lock_guard& ) = delete;
            radio_lock_guard& operator=( const radio_lock_guard& ) = delete;

        private:
            std::uint32_t primask_;
        };

        /**
         * @brief a 39 bit packet counter of the CCM nonce
         */
        struct packet_counter
        {
            std::uint32_t   low  = 0;
            std::uint8_t    high = 0;

            void increment();
        };

        /**
         * @brief the encryption of one connection; see scheduled_radio_encryption
         *
         * The switches are the link layer's. The session key is stored the way the CCM
         * reads it, most significant byte first.
         */
        struct encryption_t
        {
            bool            receive_encrypted  = false;
            bool            transmit_encrypted = false;

            std::uint8_t    key[ 16 ]          = {};
            std::uint8_t    iv[ 8 ]            = {};
            packet_counter  receive_counter;
            packet_counter  transmit_counter;
        };

        /**
         * @brief the source of the sleep clock, the 32.768 kHz clock the RTC runs on; the
         *        options of nrf.hpp name them
         */
        enum class sleep_clock
        {
            synthesized,
            crystal,
            rc
        };

        /**
         * @brief what the options of a radio decide, as one value the base is instantiated on
         */
        struct radio_configuration
        {
            bool            encrypting;
            sleep_clock     source;
            std::uint32_t   hfxo_startup_us;
            bool            statistics;
        };

        /**
         * @brief the counts of bluetoe::nrf::clock_statistics, since the radio started
         *
         * The starts of the high frequency crystal and its running time in periods of the
         * sleep clock, 30.52 µs each, and the calibrations of the RC sleep clock.
         */
        struct clock_statistics_t
        {
            std::uint32_t   crystal_starts;
            std::uint32_t   crystal_ticks;
            std::uint32_t   calibrations;
        };

        /*
         * The counters behind the statistics, and their absence: an empty member costs
         * nothing in a radio without the option. Written from interrupts and read from run().
         * A start while the crystal runs, a placed event's while a calibration keeps it on,
         * is no start: the period goes on until the stop.
         */
        struct clock_counters
        {
            volatile std::uint32_t  crystal_starts  = 0;
            volatile std::uint32_t  crystal_ticks   = 0;
            volatile std::uint32_t  calibrations    = 0;
            std::uint32_t           started_tick    = 0;
            bool                    crystal_on      = false;
        };

        struct no_clock_counters {};

        /**
         * @brief the hardware and its state; radio adds what the options decide
         *
         * CallBacks is the type the callbacks are delivered to, the link layer or a test rig,
         * which derives from radio and so from this class; the interrupts reach its
         * acceptance filter and its PDU buffer through that relation (scheduled_radio2.hpp).
         *
         * The interrupts write what happened into one slot per kind of event, and run()
         * takes it from there and delivers it; the concepts' rule that no second action of a
         * kind is scheduled while one is pending is what makes one slot enough.
         *
         * Configuration is what the options decided: whether the connection events run
         * through the CCM (nrf52_ccm.hpp), which a radio that does not encrypt has compiled
         * out, so that nothing in the firmware names the CCM's code and the linker leaves it
         * out; and the sleep clock and the crystal's startup time, see the time base above.
         */
        template < typename CallBacks, radio_configuration Configuration >
        class radio_base_t
        {
        public:
            /**
             * @brief for the interrupt handlers only
             */
            static void radio_interrupt();
            static void rtc_interrupt();
            static void clock_interrupt();
            static void ccm_interrupt();

            /**
             * @brief the counts of bluetoe::nrf::clock_statistics, since the radio started
             *
             * Written from the interrupts and read here without a lock, so a count can be a
             * start or a stop ahead of another for the moment of the read. Only a radio built
             * with the option has it.
             */
            clock_statistics_t clock_statistics() const requires ( Configuration.statistics );

        protected:
            /**
             * @brief starts the sleep clock and the random number generator, and configures
             *        the radio for legacy advertising; radio_ready() follows once the sleep
             *        clock runs
             */
            radio_base_t();

            /**
             * @brief sleeps until an interrupt happened or wake_up() was called
             *
             * Wait-for-event returns on an interrupt and on wake_up(); the event register
             * latches a wake_up() that came before the sleep, which is what makes the
             * guarantee hold that wake_up() makes run() return.
             */
            void sleep();

            /**
             * @brief makes run() return, from any context
             */
            void wake_up();

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            void set_phy(
                link_layer::phy_ll_encoding::phy_ll_encoding_t receiving,
                link_layer::phy_ll_encoding::phy_ll_encoding_t transmitting );

            /**
             * @brief the address this device advertises from
             *
             * A scan request is answered only if it is addressed to this address, which
             * the receive interrupt compares against the AdvA the request carries.
             */
            void set_local_address( const link_layer::device_address& address );

            /**
             * @brief what a static random address of this device is generated from
             */
            std::uint32_t static_random_address_seed() const;

            /**
             * @brief the encryption of the connection events to come
             */
            void set_encryption( encryption_t& encryption );


            void start_advertising_event(
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

            bool schedule_connection_event( std::uint32_t channel, link_layer::abs_time start, link_layer::abs_time end );

            bool cancel_radio_event();
            bool schedule_timer( link_layer::abs_time when );
            bool cancel_timer();

            enum class event
            {
                radio_ready,
                adv_received,
                adv_timeout,
                user_timer,
                connection_timeout,
                connection_end_event
            };

            struct happened
            {
                event                               kind;
                link_layer::abs_time                when;
                link_layer::read_buffer             received;
                link_layer::connection_event_events events;
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
                responding,
                connection_receiving,
                connection_transmitting,
                // the transmitter is being cancelled, or sends the last answer
                connection_closing,
                // the air is quiet, the callback not yet delivered: still pending
                reporting
            };

            /*
             * The callbacks, and the PDU buffer the link layer hands over with
             * link_layer_pdu_buffer(), reached from the interrupts as well as from run().
             */
            CallBacks& callbacks()
            {
                return static_cast< CallBacks& >( *this );
            }

            auto& buffer()
            {
                return callbacks().link_layer_pdu_buffer();
            }

            /*
             * The two clocks: the RTC's ticks with the overflows counted, their microseconds,
             * the placement of TIMER0 at a tick and its release with the crystal after an
             * event; see the time base in the file's comment.
             */
            std::uint32_t ticks_now() const;
            static std::uint32_t microseconds_of( std::uint64_t ticks );
            void place_timer( link_layer::abs_time from );
            void release_clocks();
            void stop_crystal();
            void stop_crystal_unless_needed();
            void note_crystal_started( std::uint32_t tick );
            void note_calibration();
            void on_clock_event();
            void on_rtc_event();

            link_layer::abs_time now() const;
            void schedule(
                std::uint32_t channel, link_layer::abs_time when,
                const link_layer::write_buffer& transmit, const link_layer::write_buffer& response,
                const link_layer::read_buffer& receive );
            bool sender_in_acceptance_filter();
            bool is_scan_request_for_us() const;
            bool is_connect_request_for_us() const;
            bool can_answer() const;
            bool answer_armed() const;
            void on_address();
            void cancel_answer();
            void on_packet_end();
            void on_radio_disabled();
            void end_event();
            void on_timer_expired();
            void allocate_connection_reception();
            void on_connection_packet_end();
            void on_connection_disabled();
            void end_connection_event();

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

            // the RTC's overflows, for ticks beyond its 24 bits; abs_time at TIMER0's zero
            volatile std::uint32_t      rtc_epoch_;
            std::uint32_t               timer_base_;

            /*
             * The RC sleep clock's calibration: asked for by its timer or by a change of
             * temperature, run while the crystal is on for an event anyway, and keeping
             * the crystal on until it is done.
             */
            volatile bool               calibration_due_;
            volatile bool               calibrating_;
            volatile bool               first_calibration_;
            volatile bool               temperature_due_;
            volatile std::uint8_t       intervals_since_calibration_;
            std::int32_t                last_temperature_;
            std::int32_t                temperature_;

            [[no_unique_address]] std::conditional_t< Configuration.statistics, clock_counters, no_clock_counters > counters_;

            /*
             * Set before an event and read by the receive interrupt: the address a scan or
             * connect request has to be addressed to, and whether the advertisement this
             * event transmitted could be scanned, or connected to, at all.
             */
            link_layer::device_address  local_address_;
            volatile bool               scannable_;
            volatile bool               connectable_;

            /*
             * A connection: where the RADIO receives when the buffer has no room, and,
             * encrypted, always: the ciphertext of a reception or of the answer being sent.
             * Three header bytes and the longest payload with its MIC. The PDU being received
             * goes into the room of the buffer, or into the scratch when it has none.
             */
            std::uint8_t                scratch_[ 3 + link_layer::max_payload_size ];
            link_layer::read_buffer     reception_;
            volatile bool               into_scratch_;
            // where the RADIO wrote the packet: the room, or the scratch
            const std::uint8_t*         air_packet_;

            /*
             * The encryption of the connection, set by the link layer; null until it is. The
             * switches are read once per event, at its start, and stay false on a radio that
             * does not encrypt.
             */
            encryption_t*               encryption_;
            bool                        receive_encrypted_;
            bool                        transmit_encrypted_;

            /*
             * Whether the PDU last put on air was a ciphertext: an acknowledgement is for
             * that PDU, and only an encrypted one used up a packet counter value. The
             * transmit switch alone does not tell: the PDU before the switch is acknowledged
             * after it, and an empty PDU goes out in plain either way.
             */
            bool                        transmitted_encrypted_;

            /*
             * A reception the CCM was still decrypting when its packet ended is judged from
             * the CCM's interrupt; if the radio's disable came first, the answer's shorts are
             * set with the judgement.
             */
            volatile bool               judgement_pending_;
            volatile bool               disabled_before_answer_;

            /*
             * What one connection event accumulates, fresh with every event, as the event's
             * report is about that event, and the next event may be another connection's:
             * what happened so far, whether the event goes on after the answer being sent,
             * and the last PDU sent that was not empty, with its sequence number, until a PDU
             * received acknowledges it.
             */
            struct connection_event_state
            {
                volatile bool                       received_any                = false;
                volatile bool                       continues                   = false;
                std::uint8_t                        crc_errors_in_a_row         = 0;
                link_layer::abs_time                anchor;
                link_layer::abs_time                end;
                bool                                last_transmitted_more_data  = false;
                bool                                unacknowledged              = false;
                bool                                unacknowledged_sn           = false;
                link_layer::connection_event_events events;
            };

            connection_event_state      connection_;

            // the PHY of the connection events; advertising is always on 1 Mbit
            bool                        connection_2mbit_;

            static radio_base_t*          instance_;
        };

        /**
         * @brief the interrupt handlers reach the radio through these; its constructor sets them
         */
        struct interrupt_entries
        {
            void ( *radio )();
            void ( *rtc )();
            void ( *clock )();
            void ( *ccm )();
        };

        extern interrupt_entries interrupts;

        /**
         * @brief option of the radio: it encrypts connections, with the CCM (nrf52_ccm.hpp)
         */
        struct encrypting {};

        /*
         * The options a radio takes: encrypting, and those of nrf.hpp, the sleep clock's
         * source and the crystal's startup time.
         */
        template < typename Option >
        concept nrf_radio_option = std::is_same_v< Option, encrypting >
            || std::is_base_of_v< nrf::nrf_details::radio_option_meta_type, typename Option::meta_type >;

        template < typename Option >
        constexpr sleep_clock sleep_clock_of()
        {
            if constexpr ( std::is_same_v< Option, nrf::sleep_clock_crystal_oscillator > )
                return sleep_clock::crystal;
            else if constexpr ( std::is_same_v< Option, nrf::calibrated_rc_sleep_clock > )
                return sleep_clock::rc;
            else
                return sleep_clock::synthesized;
        }

        template < typename... Options >
        constexpr radio_configuration configuration_of()
        {
            using source  = typename details::find_by_meta_type< nrf::nrf_details::sleep_clock_source_meta_type, Options..., nrf::synthesized_sleep_clock >::type;
            using startup = typename details::find_by_meta_type< nrf::nrf_details::hfxo_startup_time_meta_type, Options..., nrf::high_frequency_crystal_oscillator_startup_time_default >::type;

            return radio_configuration{
                .encrypting      = ( std::is_same_v< Options, encrypting > || ... ),
                .source          = sleep_clock_of< source >(),
                .hfxo_startup_us = startup::value,
                .statistics      = ( std::is_same_v< Options, nrf::clock_statistics > || ... ) };
        }

        /**
         * @brief the scheduled radio of the nRF52
         *
         * CallBacks is the type the callbacks are delivered to, which derives from this class
         * and is reached through that relation. Options are the radio's options: encrypting,
         * a sleep clock source of nrf.hpp and the crystal's startup time, or none of them.
         */
        template < typename CallBacks, typename... Options >
        class radio : public radio_base_t< CallBacks, configuration_of< Options... >() >, public security_tool_box
        {
            static_assert( ( nrf_radio_option< Options > && ... ),
                "the nRF52 scheduled radio knows the options encrypting, a sleep clock source, the crystal's startup time and clock_statistics only" );
            static_assert( details::count_by_meta_type< nrf::nrf_details::sleep_clock_source_meta_type, Options... >::count <= 1,
                "more than one sleep clock source given to the nRF52 scheduled radio" );
            static_assert( details::count_by_meta_type< nrf::nrf_details::hfxo_startup_time_meta_type, Options... >::count <= 1,
                "more than one crystal startup time given to the nRF52 scheduled radio" );

            static constexpr radio_configuration configuration = configuration_of< Options... >();
            static constexpr bool                encrypts      = configuration.encrypting;

            using base_t = radio_base_t< CallBacks, configuration >;

        public:
            static constexpr bool           hardware_supports_encryption                = encrypts;
            static constexpr bool           hardware_supports_lesc_pairing              = true;
            static constexpr bool           hardware_supports_legacy_pairing            = true;
            static constexpr bool           hardware_supports_2mbit                     = true;
            static constexpr bool           hardware_supports_synchronized_user_timer   = false;
            static constexpr bool           hardware_supports_link_layer_context        = false;

            /**
             * @brief the hardware generates preamble, access address and CRC itself and stores
             *        only the PDU
             */
            static constexpr std::size_t    radio_package_overhead                      = 0;
            static constexpr std::uint32_t  radio_max_supported_payload_length          = 255;

            /**
             * @brief the sleep clock's accuracy: the RC oscillator's after calibration as the
             *        datasheet gives it, or the crystal's on the development kits
             */
            static constexpr std::uint32_t  sleep_time_accuracy_ppm                     = configuration.source == sleep_clock::rc ? 500 : 20;

            /**
             * @brief no acceptance filter hardware; the caller filters in software through
             *        is_in_acceptance_filter() (scheduled_radio2.hpp)
             */
            static constexpr std::size_t    radio_maximum_acceptance_filter_entries     = 0;

            /**
             * @brief the radio's interrupt is excluded with all others
             */
            using radio_lock_guard = nrf52_details::radio_lock_guard;

            /**
             * @brief nothing to exclude: the callbacks are delivered from run()
             */
            struct link_layer_lock_guard {};

            /**
             * @brief see scheduled_radio_encryption
             */
            std::pair< std::uint64_t, std::uint32_t > setup_encryption(
                encryption_t& encryption, const bluetoe::details::uint128_t& key, std::uint64_t skdm, std::uint32_t ivm )
                requires encrypts
            {
                return ccm::setup_encryption( encryption, key, skdm, ivm );
            }

            using encryption_t = nrf52_details::encryption_t;
            using base_t::set_encryption;

            /**
             * @brief delivers what happened, then sleeps until the next thing happens
             *
             * radio_ready() is delivered on the first call. An interrupt between the last
             * delivery and the sleep sets the event register, so the sleep ends at once
             * and the next call delivers what it left.
             */
            void run()
            {
                trace::run_entered();

                for ( std::optional< typename base_t::happened > next = base_t::next_event(); next; next = base_t::next_event() )
                {
                    CallBacks& callbacks = static_cast< CallBacks& >( *this );

                    switch ( next->kind )
                    {
                    case base_t::event::radio_ready:
                        callbacks.radio_ready();
                        break;
                    case base_t::event::adv_received:
                        callbacks.adv_received( next->when, next->received );
                        break;
                    case base_t::event::adv_timeout:
                        callbacks.adv_timeout( next->when );
                        break;
                    case base_t::event::user_timer:
                        callbacks.user_timer( next->when );
                        break;
                    case base_t::event::connection_timeout:
                        callbacks.connection_timeout( next->when );
                        break;
                    case base_t::event::connection_end_event:
                        callbacks.connection_end_event( next->when, next->events );
                        break;
                    }
                }

                base_t::sleep();
                trace::run_left();
            }

            using base_t::wake_up;
            using base_t::set_access_address_and_crc_init;
            using base_t::set_phy;
            using base_t::set_local_address;
            using base_t::static_random_address_seed;
            using base_t::start_advertising_event;
            using base_t::schedule_advertising_event;
            using base_t::schedule_connection_event;
            using base_t::cancel_radio_event;
            using base_t::schedule_timer;
            using base_t::cancel_timer;
        };
    }

    namespace link_layer
    {
        /**
         * @brief the nRF52 radio stores every PDU the way the CCM reads it
         */
        template < typename CallBacks, typename... Options >
        struct pdu_layout_by_radio< nrf52_details::radio< CallBacks, Options... > >
        {
            using pdu_layout = nrf_details::encrypted_pdu_layout;
        };
    }
}

#include <bluetoe/nrf52_radio_base.hpp>

#endif
