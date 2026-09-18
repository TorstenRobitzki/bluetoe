#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP

/**
 * @file tester_rig.hpp
 *
 * The platform independent half of the tester: the instrument of decision 23, everything
 * that is neither the port nor the pin, on top of what instrument/instrument.hpp provides
 * to both instruments. See documentation/scheduled_radio_test_rig.md, decisions 4, 22
 * and 23.
 *
 * Besides the reset of the device under test, the tester runs a program of operations and
 * queues the PDUs it receives, the program interpreter of decision 14 for the tester: an
 * operation runs for the duration it names, until it received the PDUs it counts, or, for
 * an answer, until the reply to its answer arrived, and the next begins when it ends. An
 * operation that waits for PDUs that do not come within its window times out and ends the
 * program, since what follows it would wait in vain as well. The platform gives it the
 * radio, whose events, a received PDU or the end of a window, the tester drains in run().
 *
 * The host instantiates the same template with dummies (host/tester_functions.hpp) to
 * obtain the function list the wire is keyed on, which is why no function of the list
 * carries a type of the platform.
 */

#include "instrument/instrument.hpp"
#include "link/frame.hpp"
#include "link/function_list.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief version of the wire protocol of the tester
     *
     * Counts the function lists a firmware was built with: changes whenever
     * tester_rig::functions changes after a tester was flashed with the current one.
     */
    constexpr std::uint16_t tester_protocol_version = 1;

    /**
     * @brief PDUs the tester keeps until the host collects them
     *
     * A test's worth, plus room for what a busy channel adds while the host is not
     * listening, which shows as a gap in the count rather than as lost history.
     */
    constexpr std::size_t captured_queue_size = 32;

    /**
     * @brief the rssi limit that accepts every PDU, however weak
     */
    constexpr std::uint8_t accept_any_rssi = 0xff;

    /**
     * @brief what timed_out_operation() returns when no operation timed out
     */
    constexpr std::uint8_t no_operation_timed_out = 0xff;

    /**
     * @brief the access address and CRC init the specification fixes for advertising
     *
     * A PDU on this access address carries its advertiser's address, and the acceptance
     * filter applies to it; a PDU on any other access address carries none, and the filter
     * does not apply.
     */
    constexpr std::uint32_t advertising_access_address = 0x8E89BED6;
    constexpr std::uint32_t advertising_crc_init       = 0x555555;

    /**
     * @brief what the tester's radio reports to the program interpreter
     *
     * A received PDU or one the radio transmitted, each with the time its first bit was on
     * air in the tester's ticks, or the end of the window the current operation was
     * listening for. Not a wire type: the platform hands it to the rig, which turns a
     * reception or a transmission into a captured_pdu and a window end into the next
     * operation.
     *
     * `operation_id` is the one the platform was given when the event's operation began, so
     * that an event still queued from an operation that ended early is told from the
     * current one's.
     */
    enum class tester_event
    {
        received,
        transmitted,
        window_ended
    };

    struct tester_happened
    {
        tester_event    kind;
        tester_time     when;
        pdu             data;
        bool            crc_ok;
        std::uint8_t    rssi;
        std::uint32_t   operation_id = 0;
    };

    /**
     * @brief what a platform provides to the tester besides the serial port
     *
     * The reset line of decision 4, the idling and wake-up of decision 17, and the radio:
     * a receiver that listens on a channel for a number of ticks and reports what it hears
     * and when the window ends. All of it is the tester's own hardware, none reaches the
     * wire.
     */
    template < typename T >
    concept tester_platform = requires (
        T                                               platform,
        std::uint32_t                                   value,
        link_layer::phy_ll_encoding::phy_ll_encoding_t  phy,
        std::uint64_t                                   ticks,
        const link_layer::device_address&               address,
        const pdu&                                      data,
        const std::array< pdu, max_event_pdus >&        pdus )
    {
        /*
         * Holds the reset input of the device under test asserted for as long as that
         * part needs, then releases it. Blocks for that long.
         */
        platform.reset_device_under_test();

        /*
         * Returns once wake_up() was called, and may return earlier.
         */
        platform.run();

        /*
         * Callable from any context, including the port's.
         */
        platform.wake_up();

        /*
         * The access address and CRC init the receiver matches against.
         */
        platform.set_access_address_and_crc_init( value, value );

        /*
         * Listens on `channel` with `phy` for `ticks` of the tester's clock, queuing a
         * received event for every PDU and a window_ended event when the time is up. Stops
         * whatever ran before, and tags every event from now on with `value`, the operation
         * id.
         */
        platform.receive( value, phy, ticks, value );

        /*
         * Listens like receive(), and the first time it hears an advertising PDU whose
         * AdvA is `address`, transmits `data` one inter frame space after that PDU ended,
         * queuing a transmitted event with the time the answer's first bit was on air. It
         * keeps listening for the rest of the window without answering again. Stops and
         * tags like receive().
         */
        platform.answer( value, phy, ticks, address, data, value );

        /*
         * One connection event as its central: transmits the first `value` of `pdus` on `value`
         * with its first bit on air at `value`, a time of the tester's clock, listens for the
         * reply after each and transmits the next `value` ticks after the reply ended, queuing
         * a transmitted event for every PDU sent and a received event for every reply. A PDU
         * whose bit is set in `value`, bit 0 for the first, goes out with an invalid CRC, and its
         * transmitted event says so. The radio stays idle after the last reply. False, and
         * nothing started, if the first transmission's time is too close or gone by. Stops and
         * tags like receive().
         */
        { platform.connection_event( value, phy, ticks, value, pdus, value, value, value, value ) } -> std::same_as< bool >;

        /*
         * Stops listening; no event is queued afterwards.
         */
        platform.stop();

        /*
         * Puts `address` into slot `value` of the advertisers the radio accepts. With at
         * least one slot filled, the radio abandons a packet from any other advertiser as
         * soon as its address is received, so that the receiver is free again for the device
         * under test. Only on the advertising access address, whose PDUs carry an
         * advertiser's address. Slots are only ever added, like the rig's acceptance filter.
         */
        platform.accept_advertiser( value, address );

        /*
         * The oldest event the radio has for the interpreter, and it is forgotten.
         */
        { platform.next_event() } -> std::same_as< std::optional< tester_happened > >;
    };

    /**
     * @brief the rig that is the tester
     *
     * Port is the platform's serial port, a template over the buffer type and the type
     * it wakes, which is the tester. Platform is what else the tester needs from the
     * hardware, see tester_platform; it is constructed by the tester, and nothing about
     * it reaches the wire.
     */
    template <
        template < typename Buffer, typename Wake > class Port,
        typename Platform,
        std::size_t MaxPayload = default_max_payload >
    class tester_rig : public instrument< tester_rig< Port, Platform, MaxPayload >, Port, MaxPayload >
    {
    public:
        using instrument_t = instrument< tester_rig, Port, MaxPayload >;

        /**
         * @brief advertiser addresses the tester's acceptance filter holds
         *
         * A test observes the device under test, and perhaps a second advertiser; more is
         * not needed.
         */
        static constexpr std::size_t max_acceptance_filter_entries = 4;

        tester_rig( std::string_view implementation_name, std::string_view build_identifier )
            : instrument_t( implementation_name, build_identifier )
            , platform_()
        {
            static_assert( tester_platform< Platform > );

            instrument_t::start();
        }

        /**
         * @brief one iteration of the main loop
         *
         * The radio's events are drained before a request is answered, so that a PDU the
         * radio just reported is in the queue when the host asks for it; then the request,
         * then sleep until the next event or the port.
         */
        void run()
        {
            handle_events();
            instrument_t::serve();
            platform_.run();
        }

        void wake_up()
        {
            platform_.wake_up();
        }

        /**
         * @name Instrument functions
         *
         * The functions that are the tester's own, beside those of instrument/instrument.hpp; the
         * names and the session token are the instrument's.
         * @{
         */
        std::uint16_t protocol_version() const
        {
            return tester_protocol_version;
        }

        /**
         * @brief hold the reset input of the device under test asserted, then release it
         *
         * The host does not wait a fixed time afterwards: it polls until the device
         * answers and requires the session token it set before the reset to read zero.
         */
        void reset_device_under_test()
        {
            platform_.reset_device_under_test();
        }
        /** @} */

        /**
         * @name Program and captured PDUs
         * @{
         */

        /**
         * @brief the access address and CRC init the receiver matches
         *
         * Advertising uses the values the specification fixes; a test that observes a
         * connection sets that connection's.
         */
        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
        {
            access_address_ = access_address;
            platform_.set_access_address_and_crc_init( access_address, crc_init );
        }

        /**
         * @brief drops a received PDU weaker than `limit` decibels below a milliwatt
         *
         * A PDU below the limit is not queued and does not count as one produced, because
         * it was rejected on purpose, not lost. The default limit accepts every PDU; a test
         * that observes over a cable sets it above the air leaking in, so the tester keeps
         * only the device under test. See documentation/scheduled_radio_test_rig.md.
         */
        void set_rssi_limit( std::uint8_t limit )
        {
            rssi_limit_ = limit;
        }

        /**
         * @brief adds an advertiser address to the tester's acceptance filter
         *
         * With the set empty the tester reports every advertiser it hears; with an address
         * in it the tester reports only the advertisers of the set, the scanner filter
         * policy of Vol 6, Part B, 4.3, so that a test observes the device under test and
         * not the air. True if the address was added or was already there, false if the set
         * is full. The tester is not reset between tests and start_program() does not clear
         * the set, so a test that relies on it adds its address before every run.
         */
        bool add_to_acceptance_filter( const link_layer::device_address& address )
        {
            const auto end = acceptance_filter_.begin() + acceptance_filter_count_;

            if ( std::find( acceptance_filter_.begin(), end, address ) != end )
                return true;

            if ( acceptance_filter_count_ == max_acceptance_filter_entries )
                return false;

            acceptance_filter_[ acceptance_filter_count_ ] = address;
            platform_.accept_advertiser( acceptance_filter_count_, address );
            ++acceptance_filter_count_;

            return true;
        }

        /**
         * @brief appends an operation to the program
         *
         * The tester is not reset between tests, so the first operation added after a
         * program ran to its end empties that program and begins a new one. Refused if
         * the program is full.
         */
        bool add_operation( const operation& next )
        {
            if ( program_finished() )
            {
                operation_count_ = 0;
                cursor_          = 0;
                started_         = false;
                timed_out_       = no_operation_timed_out;
            }

            if ( operation_count_ == max_operations )
                return false;

            operations_[ operation_count_ ] = next;
            ++operation_count_;

            return true;
        }

        /**
         * @brief runs the program from its first operation
         *
         * @return false, and starts nothing, if the program is empty or was already
         *         started; true once the first operation has begun.
         */
        bool start_program()
        {
            if ( operation_count_ == 0 || started_ )
                return false;

            cursor_        = 0;
            started_       = true;
            timed_out_     = no_operation_timed_out;
            has_reference_ = false;
            has_anchor_    = false;

            // a program's received PDUs are numbered from zero; unlike the device under
            // test the tester is not reset between tests, so start clears the queue
            head_      = 0;
            queued_    = 0;
            produced_  = 0;
            collected_ = 0;

            begin_current();

            return true;
        }

        /**
         * @brief whether every operation of the started program has run
         */
        bool program_finished() const
        {
            return started_ && cursor_ == operation_count_;
        }

        /**
         * @brief the index of the operation that timed out and ended the program
         *
         * An operation times out when its window ends before the PDUs it waits for came:
         * a receive with a count that was not reached, or an answer that never heard its
         * target. An answer without a reply does not time out; that is a result a test
         * asserts. no_operation_timed_out otherwise.
         */
        std::uint8_t timed_out_operation() const
        {
            return timed_out_;
        }

        /**
         * @brief hands over the oldest captured PDUs and forgets them
         *
         * An empty batch is a meaningful answer: it is how a test asserts that the device
         * under test transmitted nothing.
         */
        captured_batch collect_captured()
        {
            captured_batch batch;

            batch.first    = collected_;
            batch.produced = produced_;

            while ( batch.count != captured_per_batch && queued_ != 0 )
            {
                batch.captured[ batch.count ] = captured_[ head_ ];

                head_ = ( head_ + 1 ) % captured_queue_size;
                --queued_;
                ++collected_;
                ++batch.count;
            }

            return batch;
        }
        /** @} */

        using functions = function_list<
            &tester_rig::protocol_version,
            &tester_rig::implementation_name,
            &tester_rig::build_identifier,
            &tester_rig::set_session_token,
            &tester_rig::reset_device_under_test,
            &tester_rig::set_access_address_and_crc_init,
            &tester_rig::set_rssi_limit,
            &tester_rig::add_to_acceptance_filter,
            &tester_rig::add_operation,
            &tester_rig::start_program,
            &tester_rig::program_finished,
            &tester_rig::collect_captured,
            &tester_rig::timed_out_operation >;

    private:
        void handle_events()
        {
            for ( std::optional< tester_happened > next = platform_.next_event(); next; next = platform_.next_event() )
            {
                if ( next->kind == tester_event::received )
                {
                    // a PDU dropped by a filter is not counted as one produced, since it
                    // was rejected on purpose, not lost between the radio and the host
                    if ( next->rssi > rssi_limit_ )
                        continue;

                    // only a PDU on the advertising access address carries an advertiser's address
                    if ( access_address_ == advertising_access_address && !in_acceptance_filter( next->data ) )
                        continue;

                    captured_pdu entry;
                    entry.when   = next->when;
                    entry.crc_ok = next->crc_ok;
                    entry.rssi   = next->rssi;
                    entry.data   = next->data;

                    enqueue( entry );

                    // what the first connection event is placed from
                    reference_     = entry.when;
                    has_reference_ = true;

                    if ( next->operation_id != operation_id_ )
                        continue;

                    // a connection event ends with the reply to its last PDU, whatever its CRC
                    if ( current_is( operation_kind::connection_event ) )
                    {
                        if ( ++received_ == operations_[ cursor_ ].pdu_count )
                            advance();
                    }
                    // the PDU after an answer is the reply to it, whatever its CRC
                    else if ( answered_ )
                        advance();
                    else if ( next->crc_ok )
                        count_received();
                }
                else if ( next->kind == tester_event::transmitted )
                {
                    // what the tester sent is captured beside what it heard; no filter
                    // applies, since the rig itself decided to send it
                    captured_pdu entry;
                    entry.direction = pdu_direction::transmitted;
                    entry.when      = next->when;
                    entry.crc_ok    = next->crc_ok;
                    entry.data      = next->data;

                    enqueue( entry );

                    if ( next->operation_id != operation_id_ )
                        continue;

                    // the first PDU of a connection event is its anchor, which the next is placed from
                    if ( current_is( operation_kind::connection_event ) && !answered_ )
                    {
                        anchor_     = entry.when;
                        has_anchor_ = true;
                    }

                    answered_ = true;
                }
                else if ( next->operation_id == operation_id_ )
                {
                    window_ended();
                }
            }
        }

        /*
         * The acceptance filter of Vol 6, Part B, 4.3, applied to what the tester hears:
         * the advertiser is the first address field of an advertising channel PDU, the six
         * bytes after the two byte header, public or random by the header's TxAdd bit. An
         * empty set accepts every advertiser.
         */
        bool in_acceptance_filter( const pdu& data ) const
        {
            if ( acceptance_filter_count_ == 0 )
                return true;

            constexpr std::uint8_t tx_add_mask = 0x40;

            const bool is_random = data.data[ 0 ] & tx_add_mask;
            const link_layer::device_address advertiser( &data.data[ 2 ], is_random );

            const auto end = acceptance_filter_.begin() + acceptance_filter_count_;

            return std::find( acceptance_filter_.begin(), end, advertiser ) != end;
        }

        bool current_is( operation_kind kind ) const
        {
            return cursor_ != operation_count_ && operations_[ cursor_ ].kind == kind;
        }

        void count_received()
        {
            if ( cursor_ == operation_count_ )
                return;

            ++received_;

            if ( received_ == operations_[ cursor_ ].count )
                advance();
        }

        void window_ended()
        {
            if ( cursor_ == operation_count_ )
                return;

            // a connection event without all its replies ends here: a device that does not
            // answer is what a test observes, in the captured PDUs
            const operation& current   = operations_[ cursor_ ];
            const bool       answers   = current.kind == operation_kind::answer;
            const bool       timed_out = current.count != 0 || ( answers && !answered_ );

            if ( timed_out )
                time_out();
            else
                advance();
        }

        void time_out()
        {
            timed_out_ = cursor_;
            cursor_    = operation_count_;
            platform_.stop();
        }

        void advance()
        {
            if ( cursor_ == operation_count_ )
                return;

            ++cursor_;
            begin_current();
        }

        /*
         * Begins the operation at the cursor. A change of the access address takes effect at
         * once, with the radio stopped, and the operation after it begins right away.
         */
        void begin_current()
        {
            for ( ; cursor_ != operation_count_ && operations_[ cursor_ ].kind == operation_kind::set_access_address_and_crc_init; ++cursor_ )
            {
                platform_.stop();
                set_access_address_and_crc_init( operations_[ cursor_ ].access_address, operations_[ cursor_ ].crc_init );
            }

            if ( cursor_ != operation_count_ )
                begin( operations_[ cursor_ ] );
            else
                platform_.stop();
        }

        // a fresh id per operation, never reused, so a stale event can not match a later one
        void begin( const operation& op )
        {
            const std::uint64_t ticks = static_cast< std::uint64_t >( op.window.usec() ) * ( tester_ticks_per_second / 1'000'000 );

            ++operation_id_;
            received_ = 0;
            answered_ = false;

            if ( op.kind == operation_kind::answer )
            {
                platform_.answer( op.channel, op.phy, ticks, op.target, op.response, operation_id_ );
            }
            else if ( op.kind == operation_kind::connection_event )
            {
                // the first event is placed from the PDU captured last, every later one from the anchor before
                const std::uint64_t delay = static_cast< std::uint64_t >( op.delay.usec() ) * ( tester_ticks_per_second / 1'000'000 );
                const tester_time   from  = has_anchor_ ? anchor_ : reference_;
                const std::uint32_t at    = static_cast< std::uint32_t >( from.ticks + delay );
                const std::uint32_t t_ifs = op.t_ifs.usec() * ( tester_ticks_per_second / 1'000'000 );

                if ( !( has_anchor_ || has_reference_ )
                  || !platform_.connection_event( op.channel, op.phy, ticks, at, op.pdus, op.pdu_count, t_ifs, op.crc_errors, operation_id_ ) )
                    time_out();
            }
            else
            {
                platform_.receive( op.channel, op.phy, ticks, operation_id_ );
            }
        }

        /*
         * A full queue drops the newest PDU and counts it anyway, so that the host sees
         * the gap in the count rather than a rewritten history.
         */
        void enqueue( const captured_pdu& entry )
        {
            ++produced_;

            if ( queued_ == captured_queue_size )
                return;

            captured_[ ( head_ + queued_ ) % captured_queue_size ] = entry;
            ++queued_;
        }

        Platform                                        platform_;
        std::uint8_t                                    rssi_limit_         = accept_any_rssi;

        std::array< link_layer::device_address, max_acceptance_filter_entries >  acceptance_filter_;
        std::size_t                                                             acceptance_filter_count_ = 0;

        std::array< operation, max_operations >         operations_;
        std::uint8_t                                    operation_count_    = 0;
        std::uint8_t                                    cursor_             = 0;
        bool                                            started_            = false;
        std::uint32_t                                   operation_id_       = 0;
        std::uint32_t                                   received_           = 0;
        bool                                            answered_           = false;
        std::uint32_t                                   access_address_     = advertising_access_address;
        std::uint8_t                                    timed_out_          = no_operation_timed_out;
        tester_time                                     reference_;
        bool                                            has_reference_      = false;
        tester_time                                     anchor_;
        bool                                            has_anchor_         = false;

        std::array< captured_pdu, captured_queue_size > captured_;
        std::size_t                                     head_               = 0;
        std::size_t                                     queued_             = 0;
        std::uint32_t                                   produced_           = 0;
        std::uint32_t                                   collected_          = 0;
    };
}
}

#endif
