#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

/*
 * One connection, held for BLUETOE_SOAK_SECONDS: the tester as the central sends an empty PDU
 * every interval, the device answers, and between the events the device's user timer runs, so
 * that the clocks switch as they do under a link layer. Ten minutes by default, which crosses
 * the overflow of a 24 bit RTC at 32.768 kHz and, on an RC sleep clock calibrated every four
 * seconds, some 150 calibrations; days, if asked for.
 *
 * The instruments count rather than record: a step and an operation that run many times, and
 * the summaries of both programs (link/program.hpp, link/tester_program.hpp). Both are read
 * once a minute while the connection runs, printed as one line, and a sign of trouble ends
 * the test at once rather than after the duration.
 *
 * The tester holds no protocol state, so its PDU is the same in every event: the same SN
 * and NESN, which the device's PDU buffer takes as a retransmission it has seen and as no
 * acknowledgement of its own empty reply. Nothing accumulates from that; the device answers
 * every event, which is what the test is about.
 */

#include "host/central.hpp"
#include "host/errors.hpp"
#include "host/tester_time.hpp"
#include "test_tools/environment.hpp"
#include "test_tools/observations.hpp"
#include "test_tools/rig_fixture.hpp"

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace bluetoe::test_rig;
using namespace std::chrono_literals;

namespace {

    // how long a single request may be lost to the serial link before that is a fault
    constexpr int           request_attempts = 3;
    constexpr std::chrono::seconds watch_interval( 5 );
    constexpr std::chrono::seconds report_interval( 60 );

    struct soak_fixture : connection_fixture
    {
        // the interval is the base's, so the count waits for the body, where it is set
        soak_fixture()
        {
            interval = 100ms;
            events   = static_cast< std::uint32_t >( duration / interval );
        }

        const std::chrono::seconds  duration    = soak_duration();
        std::uint32_t               events      = 0;

        /*
         * A request lost on the serial link within days of running is likelier than a fault
         * of the rig, and a fault repeats: retried a few times.
         */
        template < typename Request >
        auto retried( Request request )
        {
            for ( int attempt = 1;; ++attempt )
            {
                try
                {
                    return request();
                }
                catch ( const link_error& )
                {
                    if ( attempt == request_attempts )
                        throw;

                    std::this_thread::sleep_for( 100ms );
                }
            }
        }

        static void print( std::chrono::seconds elapsed, const program_summary& device_side, const tester_summary& tester_side )
        {
            std::cout
                << std::setw( 7 ) << elapsed.count() << " s: events " << count_of( device_side, callback_kind::connection_end_event )
                << ", timeouts " << count_of( device_side, callback_kind::connection_timeout )
                << ", timers " << count_of( device_side, callback_kind::user_timer )
                << ", refused calls " << device_side.refused_calls
                << ", anchor error " << device_side.anchor_error_min << ".." << device_side.anchor_error_max << " us"
                << ", bins";

            for ( const std::uint32_t bin : device_side.anchor_errors )
                std::cout << ' ' << bin;

            std::cout
                << "; tester events " << tester_side.events << ", replies " << tester_side.replies
                << ", crc errors " << tester_side.crc_errors << ", unanswered " << tester_side.unanswered
                << "; crystal starts " << device_side.crystal_starts << ", on " << device_side.crystal_ticks
                << " ticks, calibrations " << device_side.calibrations << std::endl;
        }

        /*
         * Waits for both programs, reading the summaries as it goes: a line a minute, and a
         * device that timed out, a call it refused or an event the tester heard no answer to
         * ends the wait at once, since the rest of the duration would only repeat it.
         */
        void watch_until_finished()
        {
            const auto started  = std::chrono::steady_clock::now();
            const auto deadline = started + duration + program_time_limit;
            auto       reported = started;

            while ( std::chrono::steady_clock::now() < deadline && !retried( [ & ] { return programs_finished(); } ) )
            {
                std::this_thread::sleep_for( watch_interval );

                const program_summary device_side = retried( [ & ] { return device_summary(); } );
                const tester_summary  tester_side = retried( [ & ] { return observer_summary(); } );
                const auto            now         = std::chrono::steady_clock::now();

                const bool trouble = count_of( device_side, callback_kind::connection_timeout ) != 0
                    || device_side.refused_calls != 0
                    || tester_side.unanswered != 0;

                if ( trouble || now - reported >= report_interval )
                {
                    print( std::chrono::duration_cast< std::chrono::seconds >( now - started ), device_side, tester_side );
                    reported = now;
                }

                BOOST_REQUIRE_MESSAGE( !trouble, "the connection is in trouble; see the line above" );
            }

            const program_summary device_side = retried( [ & ] { return device_summary(); } );
            const tester_summary  tester_side = retried( [ & ] { return observer_summary(); } );

            print( std::chrono::duration_cast< std::chrono::seconds >( std::chrono::steady_clock::now() - started ), device_side, tester_side );
        }
    };

    // the tester's empty PDU is 10 bytes on air at 1 Mbit: preamble, access address, header, CRC
    constexpr std::chrono::microseconds empty_pdu_air_time( 80 );
    constexpr std::chrono::microseconds expected_reply_delay = std::chrono::microseconds( bluetoe::link_layer::inter_frame_space_us ) + empty_pdu_air_time;
    constexpr std::chrono::microseconds reply_delay_tolerance( 2 );

    // the crystal per event, startup and the event itself, and per calibration, generous, in ticks of the sleep clock
    constexpr std::uint32_t crystal_ticks_per_event       = 100;
    constexpr std::uint32_t crystal_ticks_per_calibration = 1200;
    constexpr std::chrono::seconds calibration_interval( 4 );
}

BOOST_FIXTURE_TEST_CASE( a_connection_holds_for_the_soak_duration, soak_fixture, *if_tester )
{
    BOOST_REQUIRE_MESSAGE( events >= 2, "BLUETOE_SOAK_SECONDS covers less than two events" );

    const auto advertisement = advertising( 6, 0x01 );

    central    tester_side;
    const auto first = tester_side.send();
    const auto later = tester_side.send();

    program_device( {
        on_start(
            start_advertising_event( 37, advertisement ) ),
        on_adv_timeout(
            set_access_address_and_crc_init( connection_access_address, connection_crc_init ),
            schedule_connection_event( data_channel, event_start, event_start + receive_window ) ),
        repeated( events - 1, on_connection_end_event(
            next_event( data_channel ),
            schedule_timer( interval / 2 ) ) ),
        on_connection_end_event() } );

    program_tester( {
        receive( 37, 1, operation_timeout ),
        use_access_address( connection_access_address, connection_crc_init ),
        connection_event( data_channel, first_pdu_after_advertising, { first } ),
        repeated( events - 1, connection_event( data_channel, interval, { later } ) ) } );

    std::cout << "soaking for " << duration.count() << " s, " << events << " events at " << interval.count() << " us" << std::endl;

    start_programs();
    watch_until_finished();
    require_programs_finished( duration + program_time_limit );

    const program_summary device_side = device_summary();
    const tester_summary  observed    = observer_summary();

    BOOST_CHECK_EQUAL( count_of( device_side, callback_kind::connection_end_event ), events );
    BOOST_CHECK_EQUAL( count_of( device_side, callback_kind::connection_timeout ), 0u );
    BOOST_CHECK_EQUAL( count_of( device_side, callback_kind::user_timer ), events - 1 );
    BOOST_CHECK_EQUAL( device_side.refused_calls, 0u );
    // the first event is placed from the advertising, not from an anchor
    BOOST_CHECK_EQUAL( device_side.anchors, events - 1 );
    BOOST_CHECK_LE( std::chrono::microseconds( -device_side.anchor_error_min ), tolerance_for( interval ) );
    BOOST_CHECK_LE( std::chrono::microseconds( device_side.anchor_error_max ), tolerance_for( interval ) );

    BOOST_CHECK_EQUAL( observed.events, events );
    BOOST_CHECK_EQUAL( observed.replies, events );
    BOOST_CHECK_EQUAL( observed.crc_errors, 0u );
    BOOST_CHECK_EQUAL( observed.unanswered, 0u );
    BOOST_CHECK( tester_duration( observed.reply_delay_min ) >= expected_reply_delay - reply_delay_tolerance );
    BOOST_CHECK( tester_duration( observed.reply_delay_max ) <= expected_reply_delay + reply_delay_tolerance );

    // a radio that switches its crystal counts a start per event and little time beyond the
    // events and the calibrations; one on a synthesized sleep clock never stops it and counts nothing
    if ( device_side.crystal_starts != 0 )
    {
        BOOST_CHECK_GE( device_side.crystal_starts, events );
        BOOST_CHECK_LE( device_side.crystal_ticks,
            device_side.crystal_starts * crystal_ticks_per_event + device_side.calibrations * crystal_ticks_per_calibration );
    }

    // an RC sleep clock, by its accuracy, is calibrated at least at its timer's pace
    if ( device.properties().sleep_time_accuracy_ppm >= 500 )
        BOOST_CHECK_GE( device_side.calibrations, static_cast< std::uint32_t >( duration / calibration_interval ) );
}
