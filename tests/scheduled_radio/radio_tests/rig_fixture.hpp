#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RIG_FIXTURE_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RIG_FIXTURE_HPP

/**
 * @file rig_fixture.hpp
 *
 * The fixture the timing tests run on: the device under test and the tester, both opened by
 * the global fixture of dut.cpp. Each test resets the device through the tester, loads a
 * program into each instrument, starts the tester and then the device (decision 14), waits
 * for both to finish, and hands over what each recorded.
 *
 * The device and the tester count time on unrelated clocks (decision 3, 24), so a test
 * never compares a device time with a tester time. It compares an interval the device
 * requested with the same interval the tester observed. The tester's acceptance filter,
 * set to the device's address, keeps the air off the record, so what the tester reports is
 * the device (scheduled_radio2.hpp).
 */

#include "radio_tests/dut.hpp"
#include "radio_tests/tester.hpp"
#include "radio_tests/timeline.hpp"

#include "host/dut_functions.hpp"
#include "host/tester_functions.hpp"
#include "host/tester_time.hpp"
#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

#include <boost/test/unit_test.hpp>

#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <thread>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the address the device advertises from
     *
     * The device's advertising PDUs carry it as their AdvA, and the fixture puts it in the
     * tester's acceptance filter, so the tester reports only the device's advertisings and
     * not the air's (scheduled_radio2.hpp, Vol 6 Part B 4.3). A test's advertising PDUs
     * begin with these six bytes.
     */
    const link_layer::device_address dut_address{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0 }, false };

    /**
     * @brief the address the device answers, its one legitimate peer
     *
     * The fixture puts it in the device's acceptance filter, so that stray advertising in
     * the device's receive window is rejected instead of being reported and stalling the
     * program (scheduled_radio2.hpp). The tester sends its scan requests from this address.
     */
    const link_layer::device_address tester_address{ { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 }, false };

    /**
     * @brief `duration` as the delta_time a program carries
     *
     * A builder takes nanoseconds, so that a test writes the unit that fits its value.
     *
     * @throw std::invalid_argument if `duration` is not a whole number of microseconds, or
     *        does not fit into delta_time's 32 bit of them
     */
    inline link_layer::delta_time as_delta_time( std::chrono::nanoseconds duration )
    {
        const auto in_usec = std::chrono::duration_cast< std::chrono::microseconds >( duration );

        if ( in_usec != duration )
            throw std::invalid_argument( "duration is not a whole number of microseconds" );

        if ( in_usec < std::chrono::microseconds::zero()
            || in_usec.count() > std::numeric_limits< std::uint32_t >::max() )
            throw std::invalid_argument( "duration does not fit into delta_time" );

        return link_layer::delta_time( static_cast< std::uint32_t >( in_usec.count() ) );
    }

    /**
     * @brief how long a tester operation may wait for what it waits for
     *
     * Named, so that an operation's window is not mistaken for the delay of its transmission.
     */
    struct time_out
    {
        explicit time_out( std::chrono::nanoseconds duration )
            : window( duration )
        {
        }

        std::chrono::nanoseconds    window;
    };

    /** @cond HIDDEN_SYMBOLS */
    namespace details {

        template < typename... Calls >
        step step_on( callback_kind kind, Calls... calls )
        {
            static_assert( sizeof...( calls ) <= max_calls_per_step, "a step makes at most max_calls_per_step calls" );

            step result;
            result.on         = kind;
            result.call_count = sizeof...( calls );
            result.calls      = { calls... };

            return result;
        }
    }
    /** @endcond */

    /**
     * @brief a call a step makes
     * @{
     */
    inline call start_advertising( std::uint32_t channel, std::span< const std::uint8_t > transmit )
    {
        return call{
            .kind     = call_kind::start_advertising,
            .channel  = channel,
            .transmit = pdu( transmit ) };
    }

    inline call start_advertising(
        std::uint32_t channel, std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response )
    {
        return call{
            .kind     = call_kind::start_advertising,
            .channel  = channel,
            .transmit = pdu( transmit ),
            .response = pdu( response ) };
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, std::chrono::nanoseconds delay, std::span< const std::uint8_t > transmit )
    {
        return call{
            .kind     = call_kind::schedule_advertising_event,
            .channel  = channel,
            .delay    = as_delta_time( delay ),
            .transmit = pdu( transmit ) };
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, std::chrono::nanoseconds delay,
        std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response )
    {
        return call{
            .kind     = call_kind::schedule_advertising_event,
            .channel  = channel,
            .delay    = as_delta_time( delay ),
            .transmit = pdu( transmit ),
            .response = pdu( response ) };
    }

    /**
     * @brief a connection event on `channel`, receiving from `start` and until `end` without a
     *        reception, both relative to the callback
     */
    inline call schedule_connection_event( std::uint32_t channel, std::chrono::nanoseconds start, std::chrono::nanoseconds end )
    {
        return call{
            .kind      = call_kind::schedule_connection_event,
            .channel   = channel,
            .delay     = as_delta_time( start ),
            .end_delay = as_delta_time( end ) };
    }

    inline call schedule_timer( std::chrono::nanoseconds delay )
    {
        return call{ .kind = call_kind::schedule_timer, .delay = as_delta_time( delay ) };
    }

    inline call cancel_timer()
    {
        return call{ .kind = call_kind::cancel_timer };
    }

    inline call cancel_radio_event()
    {
        return call{ .kind = call_kind::cancel_radio_event };
    }

    inline call set_local_address( const link_layer::device_address& address )
    {
        return call{ .kind = call_kind::set_local_address, .address = address };
    }

    inline call set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        return call{ .kind = call_kind::set_access_address_and_crc_init, .access_address = access_address, .crc_init = crc_init };
    }
    /** @} */

    /**
     * @brief a step: the callback it waits for and the calls it makes then, up to
     *        max_calls_per_step of them
     * @{
     */
    template < std::same_as< call >... Calls >
    step on_start( Calls... calls )
    {
        return details::step_on( callback_kind::start, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_adv_received( Calls... calls )
    {
        return details::step_on( callback_kind::adv_received, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_adv_timeout( Calls... calls )
    {
        return details::step_on( callback_kind::adv_timeout, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_user_timer( Calls... calls )
    {
        return details::step_on( callback_kind::user_timer, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_connection_end_event( Calls... calls )
    {
        return details::step_on( callback_kind::connection_end_event, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_connection_timeout( Calls... calls )
    {
        return details::step_on( callback_kind::connection_timeout, calls... );
    }
    /** @} */

    /**
     * @brief a tester operation: listen on a channel for a duration, at 1 Mbit
     */
    inline operation listen( std::uint32_t channel, std::chrono::nanoseconds duration )
    {
        return operation{
            .kind    = operation_kind::receive,
            .channel = channel,
            .phy     = link_layer::phy_ll_encoding::le_1m_phy,
            .window  = as_delta_time( duration ) };
    }

    /**
     * @brief a tester operation: listen like listen() and end with the `count`th PDU received
     */
    inline operation receive( std::uint32_t channel, std::uint32_t count, time_out window )
    {
        return operation{
            .kind    = operation_kind::receive,
            .channel = channel,
            .phy     = link_layer::phy_ll_encoding::le_1m_phy,
            .window  = as_delta_time( window.window ),
            .count   = count };
    }

    /**
     * @brief a tester operation: listen like listen(), answer the first advertising PDU
     *        from `target` with `response` one inter frame space after it ended, and end with
     *        the reply
     */
    inline operation answer(
        std::uint32_t channel, const link_layer::device_address& target,
        std::span< const std::uint8_t > response, time_out window )
    {
        return operation{
            .kind     = operation_kind::answer,
            .channel  = channel,
            .phy      = link_layer::phy_ll_encoding::le_1m_phy,
            .window   = as_delta_time( window.window ),
            .target   = target,
            .response = pdu( response ) };
    }

    /**
     * @brief a tester operation: transmit `data` on `channel` with its first bit on air `delay`
     *        after the first bit of the PDU captured last, then listen and end with the reply;
     *        `window` bounds the whole operation
     */
    inline operation transmit(
        std::uint32_t channel, std::chrono::nanoseconds delay, std::span< const std::uint8_t > data,
        time_out window )
    {
        return operation{
            .kind     = operation_kind::transmit,
            .channel  = channel,
            .phy      = link_layer::phy_ll_encoding::le_1m_phy,
            .window   = as_delta_time( window.window ),
            .response = pdu( data ),
            .delay    = as_delta_time( delay ) };
    }

    /**
     * @brief a tester operation: stop the radio and use `access_address` and `crc_init` from
     *        here on
     */
    inline operation use_access_address( std::uint32_t access_address, std::uint32_t crc_init )
    {
        return operation{
            .kind           = operation_kind::set_access_address_and_crc_init,
            .access_address = access_address,
            .crc_init       = crc_init };
    }

    /**
     * @brief the rig with the device's acceptance filter left to the test
     *
     * For the tests of the filter itself; everything else runs on rig_fixture.
     */
    struct rig_fixture_without_device_filter
    {
        dut_connection&     device   = the_dut();
        tester_connection&  observer = the_tester();

        rig_fixture_without_device_filter()
        {
            // a known state on the device, its acceptance filter empty; the tester empties
            // its program itself
            device.restart( observer );

            // the tester reports only the device's advertisings, not the air's, by the same
            // filter on its side; it is not reset between tests, so this runs every test
            BOOST_REQUIRE( observer.call< &tester::add_to_acceptance_filter >( dut_address ) );

            // nor is its access address, which a test may have moved
            observer.call< &tester::set_access_address_and_crc_init >( advertising_access_address, advertising_crc_init );
        }

        void program_device( std::initializer_list< step > steps )
        {
            for ( const step& s : steps )
                BOOST_REQUIRE( device.call< &dut::add_step >( s ) );
        }

        void program_tester( std::initializer_list< operation > operations )
        {
            for ( const operation& o : operations )
                BOOST_REQUIRE( observer.call< &tester::add_operation >( o ) );
        }

        /**
         * @brief start the tester, then the device, and wait until both finished
         */
        void run()
        {
            BOOST_REQUIRE( observer.call< &tester::start_program >() );
            device.call< &dut::start_program >();

            for ( int i = 0; i != 200; ++i )
            {
                if ( device.call< &dut::program_finished >() && observer.call< &tester::program_finished >() )
                    break;

                std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
            }

            const std::uint8_t timed_out = observer.call< &tester::timed_out_operation >();
            BOOST_REQUIRE_MESSAGE( timed_out == no_operation_timed_out, "tester operation " << int( timed_out ) << " timed out" );

            BOOST_REQUIRE( device.call< &dut::program_finished >() );
            BOOST_REQUIRE( observer.call< &tester::program_finished >() );
        }

        std::vector< record > device_records()
        {
            std::vector< record > result;

            for ( ;; )
            {
                const record_batch batch = device.call< &dut::collect_records >();
                result.insert( result.end(), batch.records.begin(), batch.records.begin() + batch.count );

                if ( batch.count == 0 )
                    return result;
            }
        }

        /**
         * @brief queues data channel PDUs for the device to send in connection events
         */
        void queue_device_pdus( std::initializer_list< std::span< const std::uint8_t > > pdus )
        {
            for ( const auto& p : pdus )
                BOOST_REQUIRE( device.call< &dut::queue_pdu >( pdu( p ) ) );
        }

        /**
         * @brief the PDUs the device received in connection events, in order
         */
        std::vector< pdu > device_received()
        {
            std::vector< pdu > result;

            for ( ;; )
            {
                const received_batch batch = device.call< &dut::collect_received >();
                result.insert( result.end(), batch.pdus.begin(), batch.pdus.begin() + batch.count );

                if ( batch.count == 0 )
                    return result;
            }
        }

        /**
         * @brief the tester's timeline, required to be exactly `expected`
         */
        std::vector< captured_pdu > check_captured( const std::vector< expected_pdu >& expected )
        {
            const auto captured = tester_captured();

            test_rig::check_captured( captured, expected );

            return captured;
        }

        std::vector< captured_pdu > tester_captured()
        {
            std::vector< captured_pdu > result;

            for ( ;; )
            {
                const captured_batch batch = observer.call< &tester::collect_captured >();
                result.insert( result.end(), batch.captured.begin(), batch.captured.begin() + batch.count );

                if ( batch.count == 0 )
                    return result;
            }
        }
    };

    struct rig_fixture : rig_fixture_without_device_filter
    {
        rig_fixture()
        {
            // the device answers only the tester, so stray advertising in its receive
            // window is rejected instead of stalling the program (scheduled_radio2.hpp)
            BOOST_REQUIRE( device.call< &dut::add_to_acceptance_filter >( tester_address ) );
        }
    };
}
}

#endif
