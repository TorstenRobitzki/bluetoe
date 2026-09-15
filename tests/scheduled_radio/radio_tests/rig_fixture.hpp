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

#include "host/dut_functions.hpp"
#include "host/tester_functions.hpp"
#include "host/tester_time.hpp"
#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
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
     * program (scheduled_radio2.hpp). The tester does not transmit yet, so for now this
     * only rejects, which is what a listen-only test wants; when the tester sends scan
     * requests it will send them from this address.
     */
    const link_layer::device_address tester_address{ { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 }, false };

    /**
     * @brief a step of a device program: a call to make on a callback
     * @{
     */
    inline call start_advertising( std::uint32_t channel, std::span< const std::uint8_t > transmit )
    {
        call result;
        result.kind     = call_kind::start_advertising;
        result.channel  = channel;
        result.transmit = pdu( transmit );

        return result;
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, link_layer::delta_time delay, std::span< const std::uint8_t > transmit )
    {
        call result;
        result.kind     = call_kind::schedule_advertising_event;
        result.channel  = channel;
        result.delay    = delay;
        result.transmit = pdu( transmit );

        return result;
    }

    inline step on_start( call c )
    {
        step result;
        result.on         = callback_kind::start;
        result.call_count = 1;
        result.calls[ 0 ] = c;

        return result;
    }

    inline step on_adv_timeout( call c )
    {
        step result;
        result.on         = callback_kind::adv_timeout;
        result.call_count = 1;
        result.calls[ 0 ] = c;

        return result;
    }
    /** @} */

    /**
     * @brief a tester operation: listen on a channel for a duration, at 1 Mbit
     */
    inline operation receive( std::uint32_t channel, link_layer::delta_time window )
    {
        operation result;
        result.kind    = operation_kind::receive;
        result.channel = channel;
        result.phy     = link_layer::phy_ll_encoding::le_1m_phy;
        result.window  = window;

        return result;
    }

    struct rig_fixture
    {
        dut_connection&     device   = the_dut();
        tester_connection&  observer = the_tester();

        rig_fixture()
        {
            // a known state on the device; the tester empties its program itself
            device.restart( observer );

            // the device answers only the tester, so stray advertising in its receive
            // window is rejected instead of stalling the program (scheduled_radio2.hpp)
            BOOST_REQUIRE( device.call< &dut::add_to_acceptance_filter >( tester_address ) );

            // the tester reports only the device's advertisings, not the air's, by the same
            // filter on its side; it is not reset between tests, so this runs every test
            BOOST_REQUIRE( observer.call< &tester::add_to_acceptance_filter >( dut_address ) );
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

        std::vector< received_pdu > tester_received()
        {
            std::vector< received_pdu > result;

            for ( ;; )
            {
                const received_batch batch = observer.call< &tester::collect_received >();
                result.insert( result.end(), batch.received.begin(), batch.received.begin() + batch.count );

                if ( batch.count == 0 )
                    return result;
            }
        }
    };
}
}

#endif
