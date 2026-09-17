#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/dummy_radio.hpp"
#include "host/dut_functions.hpp"
#include "host/proxy.hpp"
#include "instrument/dut_rig.hpp"
#include "link/frame.hpp"
#include "link/program.hpp"
#include "self_tests/observed_port.hpp"

#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;
using namespace bluetoe::test_rig::self_test;
using bluetoe::link_layer::abs_time;
using bluetoe::link_layer::delta_time;
using bluetoe::link_layer::device_address;

namespace {

    /*
     * The dummy radio of host/, instrumented for programs: it keeps every scheduling call
     * the rig makes, with the arguments as the radio saw them, and answers with what the
     * test told it to. The test fires the callbacks on the rig itself.
     */
    struct made_call
    {
        call_kind                       kind;
        std::uint32_t                   channel = 0;
        abs_time                        when = {};
        abs_time                        end = {};
        std::vector< std::uint8_t >     transmit = {};
        std::vector< std::uint8_t >     response = {};
        std::uint8_t*                   receive = nullptr;
        std::size_t                     receive_size = 0;
        device_address                  address = {};
        std::uint32_t                   access_address = 0;
        std::uint32_t                   crc_init = 0;
    };

    template < typename CallBacks >
    class scripted_radio : public dummy_radio< CallBacks >
    {
    public:
        scripted_radio()
        {
            instance = this;
        }

        void set_local_address( const device_address& address )
        {
            calls.push_back( { .kind = call_kind::set_local_address, .address = address } );
        }

        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
        {
            calls.push_back( { .kind = call_kind::set_access_address_and_crc_init, .access_address = access_address, .crc_init = crc_init } );
        }

        void start_advertising( std::uint32_t channel, const bluetoe::link_layer::write_buffer& transmit,
            const bluetoe::link_layer::write_buffer& response, const bluetoe::link_layer::read_buffer& receive )
        {
            remember( call_kind::start_advertising, channel, abs_time(), transmit, response, receive );
        }

        bool schedule_advertising_event( std::uint32_t channel, abs_time when, const bluetoe::link_layer::write_buffer& transmit,
            const bluetoe::link_layer::write_buffer& response, const bluetoe::link_layer::read_buffer& receive )
        {
            return remember( call_kind::schedule_advertising_event, channel, when, transmit, response, receive );
        }

        bool schedule_connection_event( std::uint32_t channel, abs_time start, abs_time end )
        {
            calls.push_back( { .kind = call_kind::schedule_connection_event, .channel = channel, .when = start, .end = end } );

            return answer;
        }

        bool schedule_timer( abs_time when )
        {
            return remember( call_kind::schedule_timer, 0, when, {}, {}, {} );
        }

        bool cancel_radio_event()
        {
            return remember( call_kind::cancel_radio_event, 0, abs_time(), {}, {}, {} );
        }

        bool cancel_timer()
        {
            return remember( call_kind::cancel_timer, 0, abs_time(), {}, {}, {} );
        }

        std::vector< made_call >    calls;
        bool                        answer = true;

        static inline scripted_radio* instance = nullptr;

    private:
        bool remember( call_kind kind, std::uint32_t channel, abs_time when, const bluetoe::link_layer::write_buffer& transmit,
            const bluetoe::link_layer::write_buffer& response, const bluetoe::link_layer::read_buffer& receive )
        {
            calls.push_back( {
                .kind         = kind,
                .channel      = channel,
                .when         = when,
                .transmit     = { transmit.buffer, transmit.buffer + transmit.size },
                .response     = { response.buffer, response.buffer + response.size },
                .receive      = receive.buffer,
                .receive_size = receive.size } );

            return answer;
        }
    };

    using rig_t = dut_rig< scripted_radio, observed_port >;

    static_assert( dut_functions::size == rig_t::functions::size );

    struct rig_transport
    {
        rig_t                                   rig{ "scripted radio on the host", "unit test build" };
        scripted_radio< rig_t >&                radio = *scripted_radio< rig_t >::instance;
        observed_port_base&                     port  = *observed_port_base::instance;
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

    const std::uint8_t adv_ind[] = { 0x00, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0, 0x01, 0x06 };
    const std::uint8_t scan_rsp[] = { 0x04, 0x06, 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0 };

    call start_advertising( std::uint32_t channel, std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response = {} )
    {
        call result;
        result.kind     = call_kind::start_advertising;
        result.channel  = channel;
        result.transmit = pdu( transmit );
        result.response = pdu( response );

        return result;
    }

    call schedule_advertising_event( std::uint32_t channel, delta_time delay, std::span< const std::uint8_t > transmit )
    {
        call result;
        result.kind     = call_kind::schedule_advertising_event;
        result.channel  = channel;
        result.delay    = delay;
        result.transmit = pdu( transmit );

        return result;
    }

    call schedule_timer( delta_time delay )
    {
        call result;
        result.kind  = call_kind::schedule_timer;
        result.delay = delay;

        return result;
    }

    call schedule_connection_event( std::uint32_t channel, delta_time start, delta_time end )
    {
        return call{ .kind = call_kind::schedule_connection_event, .channel = channel, .delay = start, .end_delay = end };
    }

    call cancel_radio_event()
    {
        call result;
        result.kind = call_kind::cancel_radio_event;

        return result;
    }

    call set_local_address( const device_address& address )
    {
        return call{ .kind = call_kind::set_local_address, .address = address };
    }

    call set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        return call{ .kind = call_kind::set_access_address_and_crc_init, .access_address = access_address, .crc_init = crc_init };
    }

    template < typename... Calls >
    step on( callback_kind kind, Calls... calls )
    {
        step result;
        result.on         = kind;
        result.call_count = sizeof...( calls );

        const std::array< call, sizeof...( Calls ) > given = { calls... };
        std::copy( given.begin(), given.end(), result.calls.begin() );

        return result;
    }

    struct fixture
    {
        rig_transport                               transport;
        proxy< rig_t::functions, rig_transport >    remote{ transport };
        rig_t&                                      rig   = transport.rig;
        scripted_radio< rig_t >&                    radio = transport.radio;

        void load( std::span< const step > steps )
        {
            for ( const step& s : steps )
                BOOST_REQUIRE( remote.call< &rig_t::add_step >( s ) );
        }

        std::vector< record > collect_all()
        {
            std::vector< record > result;

            for ( ;; )
            {
                const record_batch batch = remote.call< &rig_t::collect_records >();

                BOOST_REQUIRE_EQUAL( batch.first, result.size() );
                result.insert( result.end(), batch.records.begin(), batch.records.begin() + batch.count );

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

BOOST_FIXTURE_TEST_CASE( the_start_step_runs_on_start, fixture )
{
    const step program[] = { on( callback_kind::start, start_advertising( 37, adv_ind, scan_rsp ) ) };
    load( program );

    BOOST_CHECK( radio.calls.empty() );

    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 1u );
    BOOST_CHECK( radio.calls[ 0 ].kind == call_kind::start_advertising );
    BOOST_CHECK_EQUAL( radio.calls[ 0 ].channel, 37u );
    BOOST_TEST( radio.calls[ 0 ].transmit == std::vector< std::uint8_t >( std::begin( adv_ind ), std::end( adv_ind ) ), boost::test_tools::per_element() );
    BOOST_TEST( radio.calls[ 0 ].response == std::vector< std::uint8_t >( std::begin( scan_rsp ), std::end( scan_rsp ) ), boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( radio.calls[ 0 ].receive_size, max_advertising_pdu_size );
}

BOOST_FIXTURE_TEST_CASE( the_call_is_recorded_with_its_result, fixture )
{
    const step program[] = { on( callback_kind::start, start_advertising( 38, adv_ind ) ) };
    load( program );
    radio.answer = false;

    remote.call< &rig_t::start_program >();

    const auto records = collect_all();

    // the start of a program is the host's doing, not the radio's, and is not recorded
    BOOST_REQUIRE_EQUAL( records.size(), 1u );
    BOOST_CHECK( records[ 0 ].kind == record_kind::call );
    BOOST_CHECK( records[ 0 ].call == call_kind::start_advertising );
    BOOST_CHECK_EQUAL( records[ 0 ].channel, 38u );
    BOOST_CHECK( !records[ 0 ].result );
}

BOOST_FIXTURE_TEST_CASE( a_timed_call_is_placed_relative_to_the_callback, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, delta_time::msec( 100 ), adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_timer( delta_time::usec( 500 ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.adv_timeout( abs_time( 110300 ) );

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 3u );
    BOOST_CHECK( radio.calls[ 1 ].kind == call_kind::schedule_advertising_event );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].when.data(), 110000u );
    BOOST_CHECK( radio.calls[ 2 ].kind == call_kind::schedule_timer );
    BOOST_CHECK_EQUAL( radio.calls[ 2 ].when.data(), 110800u );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 5u );
    BOOST_CHECK( records[ 1 ].callback == callback_kind::adv_timeout );
    BOOST_CHECK_EQUAL( records[ 1 ].when.data(), 10000u );
    BOOST_CHECK_EQUAL( records[ 2 ].when.data(), 110000u );
    BOOST_CHECK_EQUAL( records[ 4 ].when.data(), 110800u );
}

BOOST_FIXTURE_TEST_CASE( a_callback_of_another_kind_is_recorded_and_leaves_the_step_waiting, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::user_timer,  schedule_advertising_event( 37, delta_time::msec( 1 ), adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 5 ) );
    BOOST_CHECK_EQUAL( radio.calls.size(), 1u );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    rig.user_timer( abs_time( 7 ) );
    BOOST_CHECK_EQUAL( radio.calls.size(), 2u );

    const auto records = collect_all();
    BOOST_REQUIRE_EQUAL( records.size(), 4u );
    BOOST_CHECK( records[ 1 ].callback == callback_kind::adv_timeout );
    BOOST_CHECK( records[ 2 ].callback == callback_kind::user_timer );
}

BOOST_FIXTURE_TEST_CASE( the_program_is_finished_when_the_last_scheduled_action_ended, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, delta_time::msec( 1 ), adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    // the start step scheduled an event that is still pending
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    rig.adv_timeout( abs_time( 1000 ) );
    // the last step ran, but what it scheduled is pending
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    rig.adv_timeout( abs_time( 2000 ) );
    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

/*
 * start_advertising() cannot refuse, so the call that can is the one to check: a program
 * whose last step was refused waits for no callback, since none is coming.
 */
BOOST_FIXTURE_TEST_CASE( a_refused_call_leaves_nothing_pending, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, delta_time::msec( 1 ), adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    radio.answer = false;
    rig.adv_timeout( abs_time( 1000 ) );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( a_cancel_that_succeeds_ends_the_pending_action, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, delta_time::msec( 1 ), adv_ind ), cancel_radio_event() ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 1000 ) );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( an_empty_program_is_finished_at_once, fixture )
{
    load( {} );
    remote.call< &rig_t::start_program >();

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( a_received_pdu_is_recorded_with_its_bytes, fixture )
{
    const step program[] = { on( callback_kind::start, start_advertising( 37, adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    std::uint8_t scan_req[] = { 0x03, 0x0c, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0 };
    rig.adv_received( abs_time( 4242 ), { scan_req, sizeof( scan_req ) } );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 2u );
    BOOST_CHECK( records[ 1 ].callback == callback_kind::adv_received );
    BOOST_CHECK_EQUAL( records[ 1 ].when.data(), 4242u );
    BOOST_CHECK( records[ 1 ].data == pdu( scan_req ) );
    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( radio_ready_is_recorded, fixture )
{
    rig.radio_ready();

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 1u );
    BOOST_CHECK( records[ 0 ].callback == callback_kind::radio_ready );
}

BOOST_FIXTURE_TEST_CASE( records_come_in_batches_with_continuing_indices, fixture )
{
    for ( int i = 0; i != 9; ++i )
        rig.user_timer( abs_time( i ) );

    const record_batch first = remote.call< &rig_t::collect_records >();
    BOOST_CHECK_EQUAL( first.first, 0u );
    BOOST_CHECK_EQUAL( first.produced, 9u );
    BOOST_CHECK_EQUAL( first.count, records_per_batch );

    const record_batch second = remote.call< &rig_t::collect_records >();
    BOOST_CHECK_EQUAL( second.first, records_per_batch );
    BOOST_CHECK_EQUAL( second.count, records_per_batch );

    const record_batch third = remote.call< &rig_t::collect_records >();
    BOOST_CHECK_EQUAL( third.first, 2 * records_per_batch );
    BOOST_CHECK_EQUAL( third.count, 1u );
    BOOST_CHECK_EQUAL( third.records[ 0 ].when.data(), 8u );

    const record_batch empty = remote.call< &rig_t::collect_records >();
    BOOST_CHECK_EQUAL( empty.first, 9u );
    BOOST_CHECK_EQUAL( empty.produced, 9u );
    BOOST_CHECK_EQUAL( empty.count, 0u );
}

BOOST_FIXTURE_TEST_CASE( a_full_queue_drops_the_newest_and_counts_them, fixture )
{
    for ( std::uint32_t i = 0; i != record_queue_size + 5; ++i )
        rig.user_timer( abs_time( i ) );

    std::vector< record > kept;
    record_batch          batch;

    do
    {
        batch = remote.call< &rig_t::collect_records >();
        kept.insert( kept.end(), batch.records.begin(), batch.records.begin() + batch.count );
    }
    while ( batch.count != 0 );

    BOOST_CHECK_EQUAL( batch.produced, record_queue_size + 5 );
    BOOST_CHECK_EQUAL( kept.size(), record_queue_size );
    BOOST_CHECK_EQUAL( kept.back().when.data(), record_queue_size - 1 );
}

BOOST_FIXTURE_TEST_CASE( a_full_program_refuses_another_step, fixture )
{
    for ( std::size_t i = 0; i != max_steps; ++i )
        BOOST_CHECK( remote.call< &rig_t::add_step >( on( callback_kind::adv_timeout ) ) );

    BOOST_CHECK( !remote.call< &rig_t::add_step >( on( callback_kind::adv_timeout ) ) );
}

BOOST_FIXTURE_TEST_CASE( a_timed_call_on_start_is_refused, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::add_step >( on( callback_kind::start, schedule_advertising_event( 37, delta_time::msec( 1 ), adv_ind ) ) ) );
    BOOST_CHECK( !remote.call< &rig_t::add_step >( on( callback_kind::start, schedule_timer( delta_time::msec( 1 ) ) ) ) );
    BOOST_CHECK( !remote.call< &rig_t::add_step >( on( callback_kind::radio_ready ) ) );
    BOOST_CHECK( remote.call< &rig_t::add_step >( on( callback_kind::start, cancel_radio_event() ) ) );
}

BOOST_FIXTURE_TEST_CASE( a_setup_call_reaches_the_radio_and_schedules_nothing, fixture )
{
    const device_address address{ { 1, 2, 3, 4, 5, 6 }, true };

    const step program[] = {
        on( callback_kind::start, set_local_address( address ), set_access_address_and_crc_init( 0x12345678, 0xabcdef ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 2u );
    BOOST_CHECK( radio.calls[ 0 ].kind == call_kind::set_local_address );
    BOOST_CHECK( radio.calls[ 0 ].address == address );
    BOOST_CHECK( radio.calls[ 1 ].kind == call_kind::set_access_address_and_crc_init );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].access_address, 0x12345678u );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].crc_init, 0xabcdefu );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

/*
 * A step goes in one request behind a one byte opcode, so the largest a step can be has to
 * fit: every call carrying every parameter at its largest.
 */
BOOST_AUTO_TEST_CASE( the_largest_step_fits_into_one_request )
{
    const std::array< std::uint8_t, max_advertising_pdu_size > largest_pdu = {};

    const call largest{
        .kind           = call_kind::schedule_advertising_event,
        .channel        = 39,
        .delay          = delta_time::msec( 10 ),
        .transmit       = pdu( largest_pdu ),
        .response       = pdu( largest_pdu ),
        .address        = device_address{ { 1, 2, 3, 4, 5, 6 }, true },
        .access_address = 0xffffffff,
        .crc_init       = 0xffffff };

    std::array< call, max_calls_per_step > calls;
    calls.fill( largest );

    const step largest_step{ .on = callback_kind::adv_timeout, .call_count = max_calls_per_step, .calls = calls };

    std::array< std::uint8_t, default_max_payload - 1 > request;
    buffer_sink out( request );

    BOOST_CHECK( serialize( out, largest_step ) );
}

BOOST_FIXTURE_TEST_CASE( a_connection_event_is_placed_relative_to_the_callback, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, delta_time::msec( 10 ), delta_time::msec( 12 ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 2u );
    BOOST_CHECK( radio.calls[ 1 ].kind == call_kind::schedule_connection_event );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].channel, 5u );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].when.data(), 20000u );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].end.data(), 22000u );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 3u );
    BOOST_CHECK( records[ 2 ].call == call_kind::schedule_connection_event );
    BOOST_CHECK_EQUAL( records[ 2 ].when.data(), 20000u );
    BOOST_CHECK( records[ 2 ].result );
}

BOOST_FIXTURE_TEST_CASE( a_connection_event_on_start_is_refused, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::add_step >( on( callback_kind::start, schedule_connection_event( 5, delta_time::msec( 10 ), delta_time::msec( 12 ) ) ) ) );
}

// the program is finished once the connection event reported its end, with the events recorded
BOOST_FIXTURE_TEST_CASE( a_connection_event_end_is_recorded_with_its_events, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, delta_time::msec( 10 ), delta_time::msec( 12 ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    const bluetoe::link_layer::connection_event_events events( true, false, true, false, true, false );
    rig.connection_end_event( abs_time( 20000 ), events );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 4u );
    BOOST_CHECK( records[ 3 ].callback == callback_kind::connection_end_event );
    BOOST_CHECK_EQUAL( records[ 3 ].when.data(), 20000u );
    BOOST_CHECK( records[ 3 ].events.unacknowledged_data );
    BOOST_CHECK( !records[ 3 ].events.last_received_not_empty );
    BOOST_CHECK( records[ 3 ].events.last_transmitted_not_empty );
    BOOST_CHECK( !records[ 3 ].events.last_received_had_more_data );
    BOOST_CHECK( records[ 3 ].events.pending_outgoing_data );
    BOOST_CHECK( !records[ 3 ].events.error_occured );
}

// a step can wait for a connection timeout, and schedules from the time it carried
BOOST_FIXTURE_TEST_CASE( a_step_runs_on_a_connection_timeout, fixture )
{
    const step program[] = {
        on( callback_kind::start,              start_advertising( 37, adv_ind ) ),
        on( callback_kind::adv_timeout,        schedule_connection_event( 5, delta_time::msec( 10 ), delta_time::msec( 12 ) ) ),
        on( callback_kind::connection_timeout, schedule_connection_event( 6, delta_time::msec( 30 ), delta_time::msec( 32 ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.connection_timeout( abs_time( 22000 ) );

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 3u );
    BOOST_CHECK_EQUAL( radio.calls[ 2 ].channel, 6u );
    BOOST_CHECK_EQUAL( radio.calls[ 2 ].when.data(), 52000u );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
}

// a queued PDU is what the radio transmits, with the SN the buffer set
BOOST_FIXTURE_TEST_CASE( a_queued_pdu_reaches_the_radio, fixture )
{
    const std::uint8_t data[] = { 0x02, 0x03, 0x0a, 0x0b, 0x0c };

    BOOST_REQUIRE( remote.call< &rig_t::queue_pdu >( pdu( data ) ) );

    const auto transmit = rig.link_layer_pdu_buffer().next_transmit();

    BOOST_REQUIRE_EQUAL( transmit.size, sizeof( data ) );
    BOOST_CHECK_EQUAL( transmit.buffer[ 0 ] & 0x0f, 0x02 );
    BOOST_TEST( std::vector< std::uint8_t >( transmit.buffer + 1, transmit.buffer + transmit.size ) == std::vector< std::uint8_t >( data + 1, data + sizeof( data ) ), boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_full_buffer_refuses_a_queued_pdu, fixture )
{
    const std::array< std::uint8_t, 29 > largest = { 0x02, 27 };

    bool queued = true;
    for ( std::size_t i = 0; queued && i != 10; ++i )
        queued = remote.call< &rig_t::queue_pdu >( pdu( largest ) );

    BOOST_CHECK( !queued );
}

// what the radio stored is handed over in the order it was received, and is gone afterwards
BOOST_FIXTURE_TEST_CASE( a_received_pdu_is_collected, fixture )
{
    auto& buffer = rig.link_layer_pdu_buffer();

    for ( std::uint8_t sn = 0; sn != 2; ++sn )
    {
        auto room = buffer.allocate_receive_buffer();
        BOOST_REQUIRE( room.size != 0 );

        const std::uint8_t received[] = { static_cast< std::uint8_t >( 0x02 | ( sn ? 0x08 : 0 ) ), 0x01, sn };
        std::copy( std::begin( received ), std::end( received ), room.buffer );

        buffer.received( room );
    }

    const received_batch batch = remote.call< &rig_t::collect_received >();

    BOOST_REQUIRE_EQUAL( batch.count, 2u );
    BOOST_CHECK_EQUAL( batch.pdus[ 0 ].data[ 2 ], 0 );
    BOOST_CHECK_EQUAL( batch.pdus[ 1 ].data[ 2 ], 1 );

    BOOST_CHECK_EQUAL( remote.call< &rig_t::collect_received >().count, 0u );
}

BOOST_AUTO_TEST_CASE( a_full_record_batch_and_a_full_received_batch_fit_into_one_response )
{
    const std::array< std::uint8_t, max_advertising_pdu_size > largest_pdu = {};

    record largest;
    largest.data = pdu( largest_pdu );

    record_batch records;
    records.count = records_per_batch;
    records.records.fill( largest );

    received_batch received;
    received.count = received_per_batch;
    received.pdus.fill( pdu( largest_pdu ) );

    std::array< std::uint8_t, default_max_payload - 1 > response;

    buffer_sink records_out( response );
    BOOST_CHECK( serialize( records_out, records ) );

    buffer_sink received_out( response );
    BOOST_CHECK( serialize( received_out, received ) );
}
