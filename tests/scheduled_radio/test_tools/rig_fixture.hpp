#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RIG_FIXTURE_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RADIO_TESTS_RIG_FIXTURE_HPP

/**
 * @file rig_fixture.hpp
 *
 * The fixture the timing tests run on: the device under test and the tester, both opened by
 * the global fixture of dut.cpp. Each test resets the device through the tester, loads a
 * program into each instrument, built with host/program_builders.hpp, starts the tester and
 * then the device, waits for both to finish, and hands over what each recorded.
 *
 * The device and the tester count time on unrelated clocks, so a test
 * never compares a device time with a tester time. It compares an interval the device
 * requested with the same interval the tester observed. The tester's acceptance filter,
 * set to the device's address, keeps the air off the record, so what the tester reports is
 * the device (scheduled_radio2.hpp).
 */

#include "test_tools/dut.hpp"
#include "test_tools/records.hpp"
#include "test_tools/tester.hpp"
#include "test_tools/timeline.hpp"

#include "host/dut_functions.hpp"
#include "host/program_builders.hpp"
#include "host/tester_functions.hpp"
#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <span>
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
     * @brief how long a tester operation may wait for what it waits for
     */
    inline const time_out operation_timeout{ std::chrono::milliseconds( 300 ) };

    // the longest the programs of one test may run, and how often the fixture asks whether they did
    constexpr std::chrono::seconds      program_time_limit( 2 );
    constexpr std::chrono::milliseconds poll_interval( 10 );

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
            {
                BOOST_REQUIRE( device.call< &dut::add_step >( s.on, s.repeat ) );

                for ( const call& c : s.calls )
                    BOOST_REQUIRE( device.call< &dut::add_call >( c ) );
            }
        }

        void program_tester( std::initializer_list< tester_step > operations )
        {
            for ( const tester_step& s : operations )
            {
                BOOST_REQUIRE( observer.call< &tester::add_operation >( s.op ) );

                for ( const pdu& p : s.pdus )
                    BOOST_REQUIRE( observer.call< &tester::add_event_pdu >( p ) );
            }
        }

        /**
         * @brief start the tester, then the device, and wait until both finished
         *
         * `time_limit` is the longest the programs may take; a soak test passes its own.
         */
        void run( std::chrono::seconds time_limit = program_time_limit )
        {
            start_programs();
            wait_for_programs( time_limit );
            require_programs_finished( time_limit );
        }

        void start_programs()
        {
            BOOST_REQUIRE( observer.call< &tester::start_program >() );
            device.call< &dut::start_program >();
        }

        bool programs_finished()
        {
            return device.call< &dut::program_finished >() && observer.call< &tester::program_finished >();
        }

        void wait_for_programs( std::chrono::seconds time_limit )
        {
            const auto deadline = std::chrono::steady_clock::now() + time_limit;

            while ( std::chrono::steady_clock::now() < deadline && !programs_finished() )
                std::this_thread::sleep_for( poll_interval );
        }

        void require_programs_finished( std::chrono::seconds time_limit )
        {
            // a tester operation that timed out is the likelier cause of a device program that did not finish
            const std::uint8_t timed_out = observer.call< &tester::timed_out_operation >();
            BOOST_REQUIRE_MESSAGE( timed_out == no_operation_timed_out, "tester operation " << int( timed_out ) << " timed out" );

            BOOST_REQUIRE_MESSAGE( device.call< &dut::program_finished >(),
                "the device's program did not finish within " << time_limit.count() << " s" );
            BOOST_REQUIRE_MESSAGE( observer.call< &tester::program_finished >(),
                "the tester's program did not finish within " << time_limit.count() << " s" );
        }

        /**
         * @brief what the device's program did in numbers; see program_summary
         */
        program_summary device_summary()
        {
            return device.call< &dut::collect_summary >();
        }

        /**
         * @brief what the tester's connection events did in numbers; see tester_summary
         */
        tester_summary observer_summary()
        {
            return observer.call< &tester::collect_summary >();
        }

        std::vector< record > device_records()
        {
            std::vector< record > result;

            for ( ;; )
            {
                const record_batch batch = device.call< &dut::collect_records >();
                result.insert( result.end(), batch.items.begin(), batch.items.begin() + batch.count );

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
         * @brief the PDUs the device received in connection events, required to be exactly
         *        `expected`, in order; see records.hpp
         */
        void check_received( const std::vector< std::vector< std::uint8_t > >& expected )
        {
            test_rig::check_received( device_received(), expected );
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
                result.insert( result.end(), batch.items.begin(), batch.items.begin() + batch.count );

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

    /**
     * @brief the connection the connection tests run, in numbers both programs are built from
     * @{
     */
    constexpr std::uint32_t connection_access_address = 0x71764129;
    constexpr std::uint32_t connection_crc_init       = 0x7a8f23;
    constexpr std::uint32_t data_channel              = 5;

    // the connection event starts this long after the advertising, and receives until
    // `receive_window` later without a reception
    constexpr std::chrono::milliseconds event_start( 50 );
    constexpr std::chrono::milliseconds receive_window( 2 );

    // the tester's first PDU of the event begins this long after the advertising, 500 µs into
    // the receive window
    constexpr std::chrono::microseconds first_pdu_after_advertising =
        event_start + std::chrono::microseconds( 500 );
    /** @} */

    /**
     * @brief the rig for connection tests, with the parameters of the connection
     *
     * A test changes them before it builds its programs, so that the device's and the tester's
     * program are built from the same numbers.
     */
    struct connection_fixture : rig_fixture
    {
        std::chrono::microseconds   interval    = std::chrono::milliseconds( 10 );

        // how long before and after the anchor the device listens
        std::chrono::microseconds   widening    = std::chrono::microseconds( 250 );

        /**
         * @brief the device's next connection event on `channel`, placed from the anchor the
         *        connection_end_event() of the one before carried
         */
        call next_event( std::uint32_t channel ) const
        {
            return schedule_connection_event( channel, interval - widening, interval + widening );
        }
    };
}
}

#endif
