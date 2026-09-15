#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/proxy.hpp"
#include "host/tester_functions.hpp"
#include "host/tester_time.hpp"
#include "instrument/tester_rig.hpp"
#include "link/frame.hpp"
#include "link/tester_program.hpp"
#include "self_tests/observed_port.hpp"

#include <bluetoe/address.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using namespace bluetoe::test_rig::self_test;
using namespace std::chrono_literals;
using bluetoe::link_layer::delta_time;
namespace phy = bluetoe::link_layer::phy_ll_encoding;

namespace {

    /*
     * The tester's platform, instrumented: it keeps every receive the interpreter starts
     * and lets the test inject the radio's events. It keeps its instance, since the tester
     * owns the object.
     */
    class scripted_platform
    {
    public:
        scripted_platform()
        {
            instance = this;
        }

        void reset_device_under_test() {}
        void run() {}
        void wake_up() {}

        void set_access_address_and_crc_init( std::uint32_t aa, std::uint32_t crc )
        {
            access_address = aa;
            crc_init       = crc;
        }

        void receive( std::uint32_t channel, phy::phy_ll_encoding_t p, std::uint64_t ticks )
        {
            receives.push_back( { channel, p, ticks } );
        }

        void scan( std::uint32_t channel, phy::phy_ll_encoding_t p, std::uint64_t ticks,
            const bluetoe::link_layer::device_address& target, const pdu& response )
        {
            scans.push_back( { channel, p, ticks, target, response } );
        }

        std::optional< tester_happened > next_event()
        {
            if ( events.empty() )
                return std::nullopt;

            const tester_happened result = events.front();
            events.pop_front();

            return result;
        }

        void push_received( tester_time when, std::span< const std::uint8_t > bytes, bool crc_ok, std::uint8_t rssi = 60 )
        {
            tester_happened e;
            e.kind   = tester_event::received;
            e.when   = when;
            e.data   = pdu( bytes );
            e.crc_ok = crc_ok;
            e.rssi   = rssi;

            events.push_back( e );
        }

        void push_transmitted( tester_time when, std::span< const std::uint8_t > bytes )
        {
            tester_happened e;
            e.kind   = tester_event::transmitted;
            e.when   = when;
            e.data   = pdu( bytes );
            e.crc_ok = true;
            e.rssi   = 0;

            events.push_back( e );
        }

        void push_window_ended()
        {
            tester_happened e{};
            e.kind = tester_event::window_ended;

            events.push_back( e );
        }

        struct receive_call
        {
            std::uint32_t               channel;
            phy::phy_ll_encoding_t      phy;
            std::uint64_t               ticks;
        };

        struct scan_call
        {
            std::uint32_t                           channel;
            phy::phy_ll_encoding_t                  phy;
            std::uint64_t                           ticks;
            bluetoe::link_layer::device_address     target;
            pdu                                     response;
        };

        std::vector< receive_call >     receives;
        std::vector< scan_call >        scans;
        std::deque< tester_happened >   events;
        std::uint32_t                   access_address = 0;
        std::uint32_t                   crc_init       = 0;

        static inline scripted_platform* instance = nullptr;
    };

    using rig_t = tester_rig< observed_port, scripted_platform >;

    // the host's list is the tester's list, whatever the tester is instantiated with
    static_assert( tester_functions::size == rig_t::functions::size );

    struct rig_transport
    {
        rig_t                                   rig{ "scripted tester on the host", "unit test build" };
        scripted_platform&                      platform = *scripted_platform::instance;
        observed_port_base&                     port     = *observed_port_base::instance;
        frame_sender< erased_buffer >           sender{ port.receive() };
        frame_receiver< 256, erased_buffer >    receiver{ port.transmit() };

        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            if ( !sender.send( request ) )
                throw link_error( "request does not fit" );

            rig.run();

            if ( receiver.receive() != receive_result::frame )
                throw link_error( "no response" );

            return { receiver.payload().begin(), receiver.payload().end() };
        }
    };

    const std::uint8_t adv_ind[]   = { 0x00, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0, 0x01, 0x06 };
    // the same shape from another advertiser: the six address bytes after the header differ
    const std::uint8_t other_adv[] = { 0x00, 0x08, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x01, 0x06 };

    operation recv( std::uint32_t channel, delta_time window )
    {
        operation o;
        o.kind    = operation_kind::receive;
        o.channel = channel;
        o.phy     = phy::le_1m_phy;
        o.window  = window;

        return o;
    }

    operation scan_op( std::uint32_t channel, delta_time window, const bluetoe::link_layer::device_address& target )
    {
        return operation{
            .kind     = operation_kind::scan,
            .channel  = channel,
            .phy      = phy::le_1m_phy,
            .window   = window,
            .target   = target,
            .response = pdu( adv_ind ) };
    }

    struct fixture
    {
        rig_transport                               transport;
        proxy< rig_t::functions, rig_transport >    remote{ transport };
        rig_t&                                      rig      = transport.rig;
        scripted_platform&                          platform = transport.platform;

        std::vector< captured_pdu > collect_all()
        {
            std::vector< captured_pdu > result;

            for ( ;; )
            {
                const captured_batch batch = remote.call< &rig_t::collect_captured >();

                BOOST_REQUIRE_EQUAL( batch.first, result.size() );
                result.insert( result.end(), batch.captured.begin(), batch.captured.begin() + batch.count );

                if ( batch.count == 0 )
                    return result;
            }
        }
    };
}

BOOST_FIXTURE_TEST_CASE( no_program_is_not_finished, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( starting_begins_the_first_operation, fixture )
{
    BOOST_REQUIRE( remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) ) );

    BOOST_CHECK( platform.receives.empty() );

    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    BOOST_REQUIRE_EQUAL( platform.receives.size(), 1u );
    BOOST_CHECK_EQUAL( platform.receives[ 0 ].channel, 37u );
    BOOST_CHECK( platform.receives[ 0 ].phy == phy::le_1m_phy );
    BOOST_CHECK( tester_duration( platform.receives[ 0 ].ticks ) == 100ms );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( a_window_end_begins_the_next_operation, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    remote.call< &rig_t::add_operation >( recv( 38, delta_time::msec( 50 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_window_ended();
    rig.run();

    BOOST_REQUIRE_EQUAL( platform.receives.size(), 2u );
    BOOST_CHECK_EQUAL( platform.receives[ 1 ].channel, 38u );
    BOOST_CHECK( tester_duration( platform.receives[ 1 ].ticks ) == 50ms );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( the_program_is_finished_when_the_last_window_ended, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    platform.push_window_ended();
    rig.run();

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
    // no further operation was begun
    BOOST_CHECK_EQUAL( platform.receives.size(), 1u );
}

BOOST_FIXTURE_TEST_CASE( an_empty_program_does_not_start, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::start_program >() );

    // nothing to run, so nothing is finished either; a real program has operations
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
    BOOST_CHECK( platform.receives.empty() );
}

BOOST_FIXTURE_TEST_CASE( starting_a_running_program_again_is_refused, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    remote.call< &rig_t::add_operation >( recv( 38, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    // while the first operation is still running, a second start begins nothing
    BOOST_CHECK( !remote.call< &rig_t::start_program >() );
    BOOST_CHECK_EQUAL( platform.receives.size(), 1u );
}

BOOST_FIXTURE_TEST_CASE( a_new_program_can_be_built_after_the_last_one_ended, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );
    platform.push_window_ended();
    rig.run();
    BOOST_REQUIRE( remote.call< &rig_t::program_finished >() );

    // adding an operation now empties the finished program and begins a new one
    BOOST_REQUIRE( remote.call< &rig_t::add_operation >( recv( 38, delta_time::msec( 50 ) ) ) );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    BOOST_REQUIRE_EQUAL( platform.receives.size(), 2u );
    BOOST_CHECK_EQUAL( platform.receives[ 1 ].channel, 38u );
}

BOOST_FIXTURE_TEST_CASE( a_captured_pdu_is_queued_with_its_time_and_bytes, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 12345us ), adv_ind, true, 42 );
    rig.run();

    const auto received = collect_all();

    BOOST_REQUIRE_EQUAL( received.size(), 1u );
    BOOST_CHECK( time_of( received[ 0 ].when ) == 12345us );
    BOOST_CHECK( received[ 0 ].crc_ok );
    BOOST_CHECK_EQUAL( received[ 0 ].rssi, 42 );
    BOOST_CHECK( received[ 0 ].data == pdu( adv_ind ) );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_weaker_than_the_rssi_limit_is_dropped_not_lost, fixture )
{
    // keep only signals of -40 dBm or stronger (rssi 40 or less)
    remote.call< &rig_t::set_rssi_limit >( 40 );

    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 1ms ), adv_ind, true, 17 );   // strong, kept
    platform.push_received( at( 2ms ), adv_ind, true, 65 );   // weak, dropped
    platform.push_received( at( 3ms ), adv_ind, true, 40 );   // exactly at the limit, kept
    rig.run();

    const captured_batch batch = remote.call< &rig_t::collect_captured >();

    // the two strong PDUs are kept; the weak one is not counted as produced either
    BOOST_REQUIRE_EQUAL( batch.count, 2u );
    BOOST_CHECK_EQUAL( batch.produced, 2u );
    BOOST_CHECK_EQUAL( batch.captured[ 0 ].rssi, 17 );
    BOOST_CHECK_EQUAL( batch.captured[ 1 ].rssi, 40 );
}

BOOST_FIXTURE_TEST_CASE( an_empty_acceptance_filter_keeps_every_advertiser, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 1ms ), adv_ind, true );
    platform.push_received( at( 2ms ), other_adv, true );
    rig.run();

    BOOST_CHECK_EQUAL( collect_all().size(), 2u );
}

BOOST_FIXTURE_TEST_CASE( a_pdu_from_outside_the_acceptance_filter_is_dropped_not_lost, fixture )
{
    // accept only the advertiser of adv_ind, its six address bytes public
    const bluetoe::link_layer::device_address accepted{ { 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0 }, false };
    BOOST_REQUIRE( remote.call< &rig_t::add_to_acceptance_filter >( accepted ) );

    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 1ms ), adv_ind, true, 30 );     // the accepted advertiser, kept
    platform.push_received( at( 2ms ), other_adv, true, 30 );   // another advertiser, dropped
    rig.run();

    const captured_batch batch = remote.call< &rig_t::collect_captured >();

    // only the accepted advertiser is kept, and the other is not counted as produced
    BOOST_REQUIRE_EQUAL( batch.count, 1u );
    BOOST_CHECK_EQUAL( batch.produced, 1u );
    BOOST_CHECK( batch.captured[ 0 ].data == pdu( adv_ind ) );
}

BOOST_FIXTURE_TEST_CASE( a_crc_error_is_reported_not_dropped, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 1ms ), adv_ind, false );
    rig.run();

    const auto received = collect_all();

    BOOST_REQUIRE_EQUAL( received.size(), 1u );
    BOOST_CHECK( !received[ 0 ].crc_ok );
}

BOOST_FIXTURE_TEST_CASE( captured_come_in_batches_with_continuing_indices, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    for ( int i = 0; i != 9; ++i )
        platform.push_received( at( i * 1ms ), adv_ind, true );
    rig.run();

    const captured_batch first = remote.call< &rig_t::collect_captured >();
    BOOST_CHECK_EQUAL( first.first, 0u );
    BOOST_CHECK_EQUAL( first.produced, 9u );
    BOOST_CHECK_EQUAL( first.count, captured_per_batch );

    const captured_batch second = remote.call< &rig_t::collect_captured >();
    BOOST_CHECK_EQUAL( second.first, captured_per_batch );

    const captured_batch third = remote.call< &rig_t::collect_captured >();
    BOOST_CHECK_EQUAL( third.first, 2 * captured_per_batch );
    BOOST_CHECK_EQUAL( third.count, 1u );

    const captured_batch empty = remote.call< &rig_t::collect_captured >();
    BOOST_CHECK_EQUAL( empty.count, 0u );
    BOOST_CHECK_EQUAL( empty.produced, 9u );
}

BOOST_FIXTURE_TEST_CASE( a_full_queue_drops_the_newest_and_counts_them, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    for ( std::size_t i = 0; i != captured_queue_size + 5; ++i )
        platform.push_received( at( std::chrono::milliseconds{ static_cast< int >( i ) } ), adv_ind, true );
    rig.run();

    const auto received = collect_all();

    BOOST_CHECK_EQUAL( received.size(), captured_queue_size );
    // the last kept PDU is the newest of those that fit, the rest were dropped
    BOOST_CHECK( time_of( received.back().when ) == std::chrono::milliseconds{ static_cast< int >( captured_queue_size - 1 ) } );
}

BOOST_FIXTURE_TEST_CASE( the_access_address_and_crc_init_reach_the_platform, fixture )
{
    remote.call< &rig_t::set_access_address_and_crc_init >( 0x8e89bed6, 0x555555 );

    BOOST_CHECK_EQUAL( platform.access_address, 0x8e89bed6u );
    BOOST_CHECK_EQUAL( platform.crc_init, 0x555555u );
}

BOOST_FIXTURE_TEST_CASE( a_full_program_refuses_another_operation, fixture )
{
    for ( std::size_t i = 0; i != max_operations; ++i )
        BOOST_CHECK( remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 1 ) ) ) );

    BOOST_CHECK( !remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 1 ) ) ) );
}

/*
 * The scan operation and the transmitted entry are new on the wire; both round trip with
 * the fields they add, so that the tester and the host agree on them.
 */
BOOST_AUTO_TEST_CASE( a_scan_operation_round_trips_with_its_target_and_response )
{
    const operation scan{
        .kind     = operation_kind::scan,
        .channel  = 38,
        .window   = delta_time::msec( 100 ),
        .target   = bluetoe::link_layer::device_address{ { 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0 }, false },
        .response = pdu( adv_ind ) };

    std::array< std::uint8_t, 128 > storage = {};
    buffer_sink out( storage );
    BOOST_REQUIRE( serialize( out, scan ) );

    buffer_source in( storage.data(), out.size() );
    operation     decoded;
    BOOST_REQUIRE( deserialize( in, decoded ) );

    BOOST_CHECK( decoded == scan );
    BOOST_CHECK_EQUAL( in.remaining(), 0u );
}

BOOST_AUTO_TEST_CASE( a_transmitted_entry_round_trips_with_its_direction )
{
    const captured_pdu sent{
        .direction = pdu_direction::transmitted,
        .when      = tester_time{ 123456789 },
        .crc_ok    = true,
        .data      = pdu( adv_ind ) };

    std::array< std::uint8_t, 128 > storage = {};
    buffer_sink out( storage );
    BOOST_REQUIRE( serialize( out, sent ) );

    buffer_source in( storage.data(), out.size() );
    captured_pdu  decoded;
    BOOST_REQUIRE( deserialize( in, decoded ) );

    BOOST_CHECK( decoded.direction == pdu_direction::transmitted );
    BOOST_CHECK( decoded.when == sent.when );
    BOOST_CHECK( decoded.crc_ok );
    BOOST_CHECK_EQUAL( decoded.rssi, 0 );
    BOOST_CHECK( decoded.data == sent.data );
    BOOST_CHECK_EQUAL( in.remaining(), 0u );
}

/*
 * A transmission the radio reports is captured beside what it heard, with its direction
 * and time, and the acceptance filter does not apply to it.
 */
BOOST_FIXTURE_TEST_CASE( a_transmission_is_captured_with_its_direction_and_time, fixture )
{
    remote.call< &rig_t::add_operation >( recv( 37, delta_time::msec( 100 ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    platform.push_received( at( 1ms ), adv_ind, true );
    platform.push_transmitted( at( 1150us ), adv_ind );
    rig.run();

    const auto captured = collect_all();

    BOOST_REQUIRE_EQUAL( captured.size(), 2u );
    BOOST_CHECK( captured[ 0 ].direction == pdu_direction::received );
    BOOST_CHECK( captured[ 1 ].direction == pdu_direction::transmitted );
    BOOST_CHECK( time_of( captured[ 1 ].when ) == 1150us );
    BOOST_CHECK( captured[ 1 ].crc_ok );
    BOOST_CHECK_EQUAL( captured[ 1 ].rssi, 0 );
    BOOST_CHECK( captured[ 1 ].data == pdu( adv_ind ) );
}

BOOST_FIXTURE_TEST_CASE( starting_a_scan_begins_it_on_the_platform_with_its_target_and_response, fixture )
{
    const bluetoe::link_layer::device_address target{ { 1, 2, 3, 4, 5, 6 }, false };

    BOOST_REQUIRE( remote.call< &rig_t::add_operation >( scan_op( 37, delta_time::msec( 100 ), target ) ) );
    BOOST_REQUIRE( remote.call< &rig_t::start_program >() );

    BOOST_CHECK( platform.receives.empty() );
    BOOST_REQUIRE_EQUAL( platform.scans.size(), 1u );
    BOOST_CHECK_EQUAL( platform.scans[ 0 ].channel, 37u );
    BOOST_CHECK( tester_duration( platform.scans[ 0 ].ticks ) == 100ms );
    BOOST_CHECK( platform.scans[ 0 ].target == target );
    BOOST_CHECK( platform.scans[ 0 ].response == pdu( adv_ind ) );
}
