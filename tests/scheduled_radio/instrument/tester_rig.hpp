#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_TESTER_RIG_HPP

/**
 * @file tester_rig.hpp
 *
 * The platform independent half of the tester: what the contract in
 * tests/scheduled_radio/tester.hpp asks for that is neither the port nor the pin, on top
 * of what instrument/instrument.hpp provides to both instruments. See
 * documentation/scheduled_radio_test_rig.md, decisions 4 and 22.
 *
 * Besides the reset of the device under test, the tester runs a program of operations and
 * queues the PDUs it receives, the program interpreter of decision 14 for the tester: an
 * operation runs for the duration it names, and the next begins when it ends. The platform
 * gives it the radio, whose events, a received PDU or the end of a window, the tester
 * drains in run().
 *
 * The host instantiates the same template with dummies (host/tester_functions.hpp) to
 * obtain the function list the wire is keyed on, which is why no function of the list
 * carries a type of the platform.
 */

#include "instrument/instrument.hpp"
#include "link/frame.hpp"
#include "link/function_list.hpp"
#include "link/tester_program.hpp"

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
    constexpr std::size_t received_queue_size = 32;

    /**
     * @brief what the tester's radio reports to the program interpreter
     *
     * A received PDU, with the time its first bit was on air in the tester's ticks, or
     * the end of the window the current operation was listening for. Not a wire type: the
     * platform hands it to the rig, which turns a reception into a received_pdu and a
     * window end into the next operation.
     */
    enum class tester_event
    {
        received,
        window_ended
    };

    struct tester_happened
    {
        tester_event    kind;
        tester_time     when;
        pdu             data;
        bool            crc_ok;
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
        std::uint64_t                                   ticks )
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
         * received event for every PDU and a window_ended event when the time is up.
         */
        platform.receive( value, phy, ticks );

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
         * The functions of instrument.hpp and tester.hpp that are the tester's own; the
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
         * @name Program and received PDUs
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
            platform_.set_access_address_and_crc_init( access_address, crc_init );
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

            cursor_  = 0;
            started_ = true;

            // a program's received PDUs are numbered from zero; unlike the device under
            // test the tester is not reset between tests, so start clears the queue
            head_      = 0;
            queued_    = 0;
            produced_  = 0;
            collected_ = 0;

            begin( operations_[ 0 ] );

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
         * @brief hands over the oldest received PDUs and forgets them
         */
        received_batch collect_received()
        {
            received_batch batch;

            batch.first    = collected_;
            batch.produced = produced_;

            while ( batch.count != received_per_batch && queued_ != 0 )
            {
                batch.received[ batch.count ] = received_[ head_ ];

                head_ = ( head_ + 1 ) % received_queue_size;
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
            &tester_rig::add_operation,
            &tester_rig::start_program,
            &tester_rig::program_finished,
            &tester_rig::collect_received >;

    private:
        void handle_events()
        {
            for ( std::optional< tester_happened > next = platform_.next_event(); next; next = platform_.next_event() )
            {
                if ( next->kind == tester_event::received )
                {
                    received_pdu entry;
                    entry.when   = next->when;
                    entry.crc_ok = next->crc_ok;
                    entry.data   = next->data;

                    enqueue( entry );
                }
                else
                {
                    advance();
                }
            }
        }

        void advance()
        {
            if ( cursor_ == operation_count_ )
                return;

            ++cursor_;

            if ( cursor_ != operation_count_ )
                begin( operations_[ cursor_ ] );
        }

        void begin( const operation& op )
        {
            const std::uint64_t ticks = static_cast< std::uint64_t >( op.window.usec() ) * ( tester_ticks_per_second / 1'000'000 );

            platform_.receive( op.channel, op.phy, ticks );
        }

        /*
         * A full queue drops the newest PDU and counts it anyway, so that the host sees
         * the gap in the count rather than a rewritten history.
         */
        void enqueue( const received_pdu& entry )
        {
            ++produced_;

            if ( queued_ == received_queue_size )
                return;

            received_[ ( head_ + queued_ ) % received_queue_size ] = entry;
            ++queued_;
        }

        Platform                                        platform_;

        std::array< operation, max_operations >         operations_;
        std::uint8_t                                    operation_count_    = 0;
        std::uint8_t                                    cursor_             = 0;
        bool                                            started_            = false;

        std::array< received_pdu, received_queue_size > received_;
        std::size_t                                     head_               = 0;
        std::size_t                                     queued_             = 0;
        std::uint32_t                                   produced_           = 0;
        std::uint32_t                                   collected_          = 0;
    };
}
}

#endif
