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
#include <array>
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
     * program (scheduled_radio2.hpp). The tester sends its scan requests from this address.
     */
    const link_layer::device_address tester_address{ { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 }, false };

    /**
     * @brief the access address and CRC init the specification fixes for advertising
     */
    constexpr std::uint32_t advertising_access_address = 0x8E89BED6;
    constexpr std::uint32_t advertising_crc_init       = 0x555555;

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
        std::uint32_t channel, link_layer::delta_time delay, std::span< const std::uint8_t > transmit )
    {
        call result;
        result.kind     = call_kind::schedule_advertising_event;
        result.channel  = channel;
        result.delay    = delay;
        result.transmit = pdu( transmit );

        return result;
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, link_layer::delta_time delay,
        std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response )
    {
        return call{
            .kind     = call_kind::schedule_advertising_event,
            .channel  = channel,
            .delay    = delay,
            .transmit = pdu( transmit ),
            .response = pdu( response ) };
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

    inline step on_adv_received( call c )
    {
        return step{ .on = callback_kind::adv_received, .call_count = 1, .calls = { c } };
    }

    inline step on_start( call first, call second )
    {
        return step{ .on = callback_kind::start, .call_count = 2, .calls = { first, second } };
    }

    inline step on_adv_timeout( call first, call second )
    {
        return step{ .on = callback_kind::adv_timeout, .call_count = 2, .calls = { first, second } };
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

    /**
     * @brief a tester operation: listen like receive(), but end after `count` received PDUs
     */
    inline operation receive( std::uint32_t channel, link_layer::delta_time window, std::uint32_t count )
    {
        return operation{
            .kind    = operation_kind::receive,
            .channel = channel,
            .phy     = link_layer::phy_ll_encoding::le_1m_phy,
            .window  = window,
            .count   = count };
    }

    /**
     * @brief a tester operation: listen like receive(), answer the first advertising PDU
     *        from `target` with `response` one inter frame space after it ended, and end
     *        with the reply
     */
    inline operation answer(
        std::uint32_t channel, link_layer::delta_time window,
        const link_layer::device_address& target, std::span< const std::uint8_t > response )
    {
        return operation{
            .kind     = operation_kind::answer,
            .channel  = channel,
            .phy      = link_layer::phy_ll_encoding::le_1m_phy,
            .window   = window,
            .target   = target,
            .response = pdu( response ) };
    }

    /**
     * @brief a SCAN_REQ from `scanner` to `advertiser`, as the tester transmits it
     *
     * Header type 0x03, TxAdd the scanner's address kind and RxAdd the advertiser's, then
     * the two addresses in that order, each six bytes as the address stores them.
     */
    inline std::array< std::uint8_t, 14 > scan_request(
        const link_layer::device_address& scanner, const link_layer::device_address& advertiser )
    {
        constexpr std::uint8_t scan_req  = 0x03;
        constexpr std::uint8_t tx_add    = 0x40;
        constexpr std::uint8_t rx_add    = 0x80;

        std::array< std::uint8_t, 14 > result = {};

        result[ 0 ] = scan_req
            | ( scanner.is_random() ? tx_add : 0 )
            | ( advertiser.is_random() ? rx_add : 0 );
        result[ 1 ] = 12;

        std::copy( scanner.begin(), scanner.end(), result.begin() + 2 );
        std::copy( advertiser.begin(), advertiser.end(), result.begin() + 8 );

        return result;
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
