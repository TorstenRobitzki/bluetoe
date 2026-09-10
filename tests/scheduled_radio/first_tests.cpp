/**
 * @file first_tests.cpp
 *
 * The first two tests of a scheduled radio implementation, written against the instruments
 * described in dut_rig.hpp and tester.hpp. They assume a rig_fixture that holds a `dut` and
 * a `tester`, whose constructor resets the device under test and waits for radio_ready,
 * whose run() starts the tester, then the device, and waits until both programs report
 * finished, and whose collection functions return vectors. Errors of the links throw;
 * nothing in here checks for them.
 *
 * This file is a sketch in the same sense as the headers it uses: the fixture does not exist
 * yet, and the file is not part of any build.
 */

#include "rig_fixture.hpp"

#include <boost/test/unit_test.hpp>

using namespace bluetoe::link_layer;
using namespace bluetoe::test_rig;

namespace {

    constexpr auto le_1m = phy_ll_encoding::le_1m_phy;

    // How precisely the implementation places a transmission. This is what decision 10
    // asks it to state; the value is a placeholder until the nRF52 does.
    const delta_time placement_tolerance = delta_time::usec( 5 );

    const delta_time event_interval      = delta_time::msec( 100 );

    // the DUT program spans two intervals and starts one round trip after the tester
    const delta_time listen_window       = 3 * event_interval;

    const std::uint8_t adv_ind[] = {
        0x00, 0x08,                                 // ADV_IND, length 8
        0x01, 0x02, 0x03, 0x04, 0x05, 0xc0,         // AdvA
        0x01, 0x06                                  // Flags: LE General Discoverable
    };

    const std::uint8_t adv_nonconn_ind[] = {
        0x02, 0x06,                                 // ADV_NONCONN_IND, length 6
        0x01, 0x02, 0x03, 0x04, 0x05, 0xc0          // AdvA
    };

    void check_within( delta_time actual, delta_time expected, delta_time tolerance )
    {
        const delta_time diff = actual > expected ? actual - expected : expected - actual;

        BOOST_CHECK_MESSAGE( diff <= tolerance,
            actual << " is not within " << tolerance << " of " << expected );
    }
}

/*
 * Neither time domain is related to the other. The first event is untimed and serves as
 * the origin; the two after it are the same two events in both domains, so the interval
 * between them, once as requested by the DUT and once as observed by the tester, is the
 * measurement.
 */
BOOST_FIXTURE_TEST_CASE( advertising_is_transmitted_at_the_requested_time, rig_fixture )
{
    dut.program(
        on_start(       start_advertising(          37,                 adv_ind ) ),
        on_adv_timeout( schedule_advertising_event( 37, event_interval, adv_ind ) ),
        on_adv_timeout( schedule_advertising_event( 37, event_interval, adv_ind ) ) );

    tester.program(
        receive( 37, le_1m, listen_window ) );

    run();

    const auto calls = dut.collect_executed_calls();
    const auto seen  = tester.collect_received();

    BOOST_REQUIRE_EQUAL( calls.size(), 3u );
    BOOST_CHECK( calls[ 0 ].result );
    BOOST_CHECK( calls[ 1 ].result );
    BOOST_CHECK( calls[ 2 ].result );

    BOOST_REQUIRE_EQUAL( seen.size(), 3u );

    for ( const auto& pdu : seen )
    {
        BOOST_CHECK( pdu.crc_ok );
        BOOST_CHECK_EQUAL_COLLECTIONS( pdu.data, pdu.data + pdu.size, std::begin( adv_ind ), std::end( adv_ind ) );
    }

    const delta_time requested = calls[ 2 ].when - calls[ 1 ].when;
    const delta_time observed  = seen[ 2 ].when - seen[ 1 ].when;

    // over the interval the DUT's clock may drift by its rated accuracy; everything beyond
    // that is how precisely the implementation places a transmission
    check_within( observed, requested, placement_tolerance + requested.ppm( dut.sleep_time_accuracy_ppm ) );
}

/*
 * The tester hears one channel at a time, so this listens where the DUT is not supposed to
 * be. Silence alone would also pass with a tester that hears nothing, so the DUT then
 * transmits a different PDU on the listening channel, and exactly that one is expected.
 */
BOOST_FIXTURE_TEST_CASE( advertising_is_transmitted_on_the_requested_channel_only, rig_fixture )
{
    for ( const std::uint32_t listening : { 38u, 39u } )
    {
        dut.program(
            on_start(       start_advertising(          37,                        adv_ind ) ),
            on_adv_timeout( schedule_advertising_event( listening, event_interval, adv_nonconn_ind ) ) );

        tester.program(
            receive( listening, le_1m, listen_window ) );

        run();

        const auto seen = tester.collect_received();

        BOOST_REQUIRE_EQUAL( seen.size(), 1u );
        BOOST_CHECK_EQUAL_COLLECTIONS( seen[ 0 ].data, seen[ 0 ].data + seen[ 0 ].size,
            std::begin( adv_nonconn_ind ), std::end( adv_nonconn_ind ) );
    }
}
