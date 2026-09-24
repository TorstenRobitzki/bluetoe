#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/dummy_radio.hpp"
#include "host/dut_functions.hpp"
#include "host/program_builders.hpp"
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
using namespace std::chrono_literals;
using bluetoe::link_layer::abs_time;
using bluetoe::link_layer::delta_time;
using bluetoe::link_layer::device_address;
using bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t;
using bluetoe::link_layer::phy_ll_encoding::le_1m_phy;
using bluetoe::link_layer::phy_ll_encoding::le_2m_phy;

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
        phy_ll_encoding_t               receiving = le_1m_phy;
        phy_ll_encoding_t               transmitting = le_1m_phy;
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

        void set_phy( phy_ll_encoding_t receiving, phy_ll_encoding_t transmitting )
        {
            calls.push_back( { .kind = call_kind::set_phy, .receiving = receiving, .transmitting = transmitting } );
        }

        void start_advertising_event( std::uint32_t channel, const bluetoe::link_layer::write_buffer& transmit,
            const bluetoe::link_layer::write_buffer& response, const bluetoe::link_layer::read_buffer& receive )
        {
            remember( call_kind::start_advertising_event, channel, abs_time(), transmit, response, receive );
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
        frame_sender< erased_buffer >                           sender{ port.receive() };
        frame_receiver< default_max_payload, erased_buffer >     receiver{ port.transmit() };

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

    struct fixture
    {
        rig_transport                               transport;
        proxy< rig_t::functions, rig_transport >    remote{ transport };
        rig_t&                                      rig   = transport.rig;
        scripted_radio< rig_t >&                    radio = transport.radio;

        void load( std::span< const step > steps )
        {
            for ( const step& s : steps )
            {
                BOOST_REQUIRE( remote.call< &rig_t::add_step >( s.on, s.repeat ) );

                for ( const call& c : s.calls )
                    BOOST_REQUIRE( remote.call< &rig_t::add_call >( c ) );
            }
        }

        std::vector< record > collect_all()
        {
            std::vector< record > result;

            for ( ;; )
            {
                const record_batch batch = remote.call< &rig_t::collect_records >();

                BOOST_REQUIRE_EQUAL( batch.first, result.size() );
                result.insert( result.end(), batch.items.begin(), batch.items.begin() + batch.count );

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
    const step program[] = { on( callback_kind::start, start_advertising_event( 37, adv_ind, scan_rsp ) ) };
    load( program );

    BOOST_CHECK( radio.calls.empty() );

    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 1u );
    BOOST_CHECK( radio.calls[ 0 ].kind == call_kind::start_advertising_event );
    BOOST_CHECK_EQUAL( radio.calls[ 0 ].channel, 37u );
    BOOST_TEST( radio.calls[ 0 ].transmit == std::vector< std::uint8_t >( std::begin( adv_ind ), std::end( adv_ind ) ), boost::test_tools::per_element() );
    BOOST_TEST( radio.calls[ 0 ].response == std::vector< std::uint8_t >( std::begin( scan_rsp ), std::end( scan_rsp ) ), boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( radio.calls[ 0 ].receive_size, max_advertising_pdu_size );
}

BOOST_FIXTURE_TEST_CASE( the_call_is_recorded_with_its_result, fixture )
{
    const step program[] = { on( callback_kind::start, start_advertising_event( 38, adv_ind ) ) };
    load( program );
    radio.answer = false;

    remote.call< &rig_t::start_program >();

    const auto records = collect_all();

    // the start of a program is the host's doing, not the radio's, and is not recorded
    BOOST_REQUIRE_EQUAL( records.size(), 1u );
    BOOST_CHECK( records[ 0 ].kind == record_kind::call );
    BOOST_CHECK( records[ 0 ].call == call_kind::start_advertising_event );
    BOOST_CHECK_EQUAL( records[ 0 ].channel, 38u );
    BOOST_CHECK( !records[ 0 ].result );
}

BOOST_FIXTURE_TEST_CASE( a_timed_call_is_placed_relative_to_the_callback, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, 100ms, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_timer( 500us ) ) };
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
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::user_timer,  schedule_advertising_event( 37, 1ms, adv_ind ) ) };
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
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ) ) };
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
 * start_advertising_event() cannot refuse, so the call that can is the one to check: a program
 * whose last step was refused waits for no callback, since none is coming.
 */
BOOST_FIXTURE_TEST_CASE( a_refused_call_leaves_nothing_pending, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    radio.answer = false;
    rig.adv_timeout( abs_time( 1000 ) );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

BOOST_FIXTURE_TEST_CASE( a_cancel_that_succeeds_ends_the_pending_action, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ), cancel_radio_event() ) };
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
    const step program[] = { on( callback_kind::start, start_advertising_event( 37, adv_ind ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    std::uint8_t scan_req[] = { 0x03, 0x0c, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0xc0 };
    rig.adv_received( abs_time( 4242 ), { scan_req, sizeof( scan_req ) } );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 2u );
    BOOST_CHECK( records[ 1 ].callback == callback_kind::adv_received );
    BOOST_CHECK_EQUAL( records[ 1 ].when.data(), 4242u );
    BOOST_CHECK( records[ 1 ].data == adv_pdu( scan_req ) );
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
    BOOST_CHECK_EQUAL( third.items[ 0 ].when.data(), 8u );

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
        kept.insert( kept.end(), batch.items.begin(), batch.items.begin() + batch.count );
    }
    while ( batch.count != 0 );

    BOOST_CHECK_EQUAL( batch.produced, record_queue_size + 5 );
    BOOST_CHECK_EQUAL( kept.size(), record_queue_size );
    BOOST_CHECK_EQUAL( kept.back().when.data(), record_queue_size - 1 );
}

BOOST_FIXTURE_TEST_CASE( a_full_program_refuses_another_step, fixture )
{
    for ( std::size_t i = 0; i != max_steps; ++i )
        BOOST_CHECK( remote.call< &rig_t::add_step >( callback_kind::adv_timeout, 1 ) );

    BOOST_CHECK( !remote.call< &rig_t::add_step >( callback_kind::adv_timeout, 1 ) );
}

BOOST_FIXTURE_TEST_CASE( a_step_on_radio_ready_is_refused, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::add_step >( callback_kind::radio_ready, 1 ) );
}

BOOST_FIXTURE_TEST_CASE( a_step_that_runs_no_times_is_refused, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::add_step >( callback_kind::adv_timeout, 0 ) );
}

BOOST_FIXTURE_TEST_CASE( a_timed_call_on_start_is_refused, fixture )
{
    BOOST_REQUIRE( remote.call< &rig_t::add_step >( callback_kind::start, 1 ) );

    BOOST_CHECK( !remote.call< &rig_t::add_call >( schedule_advertising_event( 37, 1ms, adv_ind ) ) );
    BOOST_CHECK( !remote.call< &rig_t::add_call >( schedule_timer( 1ms ) ) );
    BOOST_CHECK( remote.call< &rig_t::add_call >( cancel_radio_event() ) );
}

BOOST_FIXTURE_TEST_CASE( a_call_without_a_step_is_refused, fixture )
{
    BOOST_CHECK( !remote.call< &rig_t::add_call >( cancel_radio_event() ) );
}

// the steps share the calls: one step may use all of them
BOOST_FIXTURE_TEST_CASE( a_full_program_refuses_another_call, fixture )
{
    BOOST_REQUIRE( remote.call< &rig_t::add_step >( callback_kind::adv_timeout, 1 ) );

    for ( std::size_t i = 0; i != max_calls; ++i )
        BOOST_CHECK( remote.call< &rig_t::add_call >( cancel_radio_event() ) );

    BOOST_CHECK( !remote.call< &rig_t::add_call >( cancel_radio_event() ) );
    BOOST_CHECK( remote.call< &rig_t::add_step >( callback_kind::user_timer, 1 ) );
    BOOST_CHECK( !remote.call< &rig_t::add_call >( cancel_radio_event() ) );
}

// the calls of a step run in the order they were added, each step its own
BOOST_FIXTURE_TEST_CASE( a_step_makes_all_its_calls, fixture )
{
    const device_address address{ { 1, 2, 3, 4, 5, 6 }, true };

    const step program[] = {
        on( callback_kind::start,
            set_local_address( address ),
            set_access_address_and_crc_init( 0x12345678, 0xabcdef ),
            start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout,
            cancel_radio_event() ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 3u );
    BOOST_CHECK( radio.calls[ 0 ].kind == call_kind::set_local_address );
    BOOST_CHECK( radio.calls[ 1 ].kind == call_kind::set_access_address_and_crc_init );
    BOOST_CHECK( radio.calls[ 2 ].kind == call_kind::start_advertising_event );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );
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

// the one PHY of a set_phy call is used in both directions
BOOST_FIXTURE_TEST_CASE( a_set_phy_call_reaches_the_radio_for_both_directions, fixture )
{
    const step program[] = {
        on( callback_kind::start, call{ .kind = call_kind::set_phy, .phy = le_2m_phy } ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 1u );
    BOOST_CHECK( radio.calls[ 0 ].kind == call_kind::set_phy );
    BOOST_CHECK( radio.calls[ 0 ].receiving == le_2m_phy );
    BOOST_CHECK( radio.calls[ 0 ].transmitting == le_2m_phy );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );
}

/*
 * A call goes in one request behind a one byte opcode, so the largest a call can be has to
 * fit: every parameter at its largest.
 */
BOOST_AUTO_TEST_CASE( the_largest_call_fits_into_one_request )
{
    const std::array< std::uint8_t, max_pdu_size >             largest_pdu = {};
    const std::array< std::uint8_t, max_advertising_pdu_size > largest_advertising_pdu = {};

    const call largest{
        .kind           = call_kind::schedule_advertising_event,
        .channel        = 39,
        .delay          = delta_time::msec( 10 ),
        .transmit       = pdu( largest_pdu ),
        .response       = adv_pdu( largest_advertising_pdu ),
        .address        = device_address{ { 1, 2, 3, 4, 5, 6 }, true },
        .access_address = 0xffffffff,
        .crc_init       = 0xffffff,
        .phy            = le_2m_phy };

    std::array< std::uint8_t, default_max_payload - 1 > request;
    buffer_sink out( request );

    BOOST_CHECK( serialize( out, largest ) );
}

BOOST_FIXTURE_TEST_CASE( a_connection_event_is_placed_relative_to_the_callback, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, 10ms, 12ms ) ) };
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
    BOOST_REQUIRE( remote.call< &rig_t::add_step >( callback_kind::start, 1 ) );
    BOOST_CHECK( !remote.call< &rig_t::add_call >( schedule_connection_event( 5, 10ms, 12ms ) ) );
}

// the program is finished once the connection event reported its end, with the events recorded
BOOST_FIXTURE_TEST_CASE( a_connection_event_end_is_recorded_with_its_events, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, 10ms, 12ms ) ) };
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
        on( callback_kind::start,              start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout,        schedule_connection_event( 5, 10ms, 12ms ) ),
        on( callback_kind::connection_timeout, schedule_connection_event( 6, 30ms, 32ms ) ) };
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

// what a link layer does from its callback: fill the buffer for the next event
BOOST_FIXTURE_TEST_CASE( a_step_queues_a_pdu, fixture )
{
    const std::uint8_t data[] = { 0x02, 0x03, 0x0a, 0x0b, 0x0c };

    const step program[] = {
        on( callback_kind::start, call{ .kind = call_kind::queue_pdu, .transmit = pdu( data ) } ) };
    load( program );
    remote.call< &rig_t::start_program >();

    const auto transmit = rig.link_layer_pdu_buffer().next_transmit();

    BOOST_REQUIRE_EQUAL( transmit.size, sizeof( data ) );
    BOOST_TEST( std::vector< std::uint8_t >( transmit.buffer + 1, transmit.buffer + transmit.size ) == std::vector< std::uint8_t >( data + 1, data + sizeof( data ) ), boost::test_tools::per_element() );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 1u );
    BOOST_CHECK( records[ 0 ].call == call_kind::queue_pdu );
    BOOST_CHECK( records[ 0 ].result );
}

namespace {

    // what the radio does with a PDU of the largest size received with a valid CRC, `sn`
    // alternating as for new PDUs
    bool receive_largest_pdu( rig_t& rig, bool sn )
    {
        auto& buffer = rig.link_layer_pdu_buffer();
        auto  room   = buffer.allocate_receive_buffer();

        if ( room.size == 0 )
            return false;

        std::array< std::uint8_t, max_data_pdu_size > largest = {
            static_cast< std::uint8_t >( 0x02 | ( sn ? 0x08 : 0 ) ), max_data_pdu_size - 2 };
        std::copy( largest.begin(), largest.end(), room.buffer );
        buffer.received( room );

        return true;
    }
}

namespace {

    // a response hands over one PDU, so a test that expects several asks until none is left
    template < typename Remote >
    std::vector< pdu > collect_all_received( Remote& remote )
    {
        std::vector< pdu > result;

        for ( ;; )
        {
            const received_batch batch = remote.template call< &rig_t::collect_received >();

            if ( batch.count == 0 )
                return result;

            result.insert( result.end(), batch.pdus.begin(), batch.pdus.begin() + batch.count );
        }
    }
}

// the radio tests of a full receive buffer are written against this number
BOOST_FIXTURE_TEST_CASE( the_receive_buffer_holds_received_pdus_until_full, fixture )
{
    for ( std::size_t stored = 0; stored != received_pdus_until_full; ++stored )
        BOOST_REQUIRE( receive_largest_pdu( rig, stored % 2 ) );

    BOOST_CHECK( !receive_largest_pdu( rig, received_pdus_until_full % 2 ) );
}

// what a link layer does from its callback: take what was received, so that there is room again
BOOST_FIXTURE_TEST_CASE( a_step_reads_the_received_pdus_and_the_host_collects_them, fixture )
{
    for ( std::size_t stored = 0; stored != received_pdus_until_full; ++stored )
        BOOST_REQUIRE( receive_largest_pdu( rig, stored % 2 ) );

    const step program[] = { on( callback_kind::start, call{ .kind = call_kind::read_received } ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_CHECK( receive_largest_pdu( rig, received_pdus_until_full % 2 ) );

    BOOST_CHECK_EQUAL( collect_all_received( remote ).size(), received_pdus_until_full + 1 );
}

namespace {

    const call switch_buffer{ .kind = call_kind::switch_pdu_buffer };

    // the payload of the PDU the radio would send next from the buffer handed out
    std::vector< std::uint8_t > next_payload( rig_t& rig )
    {
        const auto next = rig.link_layer_pdu_buffer().next_transmit();

        return std::vector< std::uint8_t >( next.buffer + 2, next.buffer + next.size );
    }
}

// as a link layer with a second connection would; the radio takes the buffer anew every event
BOOST_FIXTURE_TEST_CASE( a_step_hands_the_radio_the_other_pdu_buffer_and_back, fixture )
{
    const auto* first = &rig.link_layer_pdu_buffer();

    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ), switch_buffer ),
        on( callback_kind::adv_timeout, switch_buffer ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_CHECK( &rig.link_layer_pdu_buffer() != first );

    rig.adv_timeout( abs_time( 10000 ) );

    BOOST_CHECK( &rig.link_layer_pdu_buffer() == first );
}

// the host queues into the buffer handed out, and each keeps what was queued into it
BOOST_FIXTURE_TEST_CASE( a_queued_pdu_stays_with_its_buffer, fixture )
{
    const std::uint8_t first_pdu[]  = { 0x02, 0x01, 0xaa };
    const std::uint8_t second_pdu[] = { 0x02, 0x01, 0xbb };

    BOOST_REQUIRE( remote.call< &rig_t::queue_pdu >( pdu( first_pdu ) ) );

    const step program[] = {
        on( callback_kind::start,       switch_buffer ),
        on( callback_kind::adv_timeout, switch_buffer ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE( remote.call< &rig_t::queue_pdu >( pdu( second_pdu ) ) );
    BOOST_CHECK( next_payload( rig ) == std::vector< std::uint8_t >{ 0xbb } );

    rig.adv_timeout( abs_time( 10000 ) );

    BOOST_CHECK( next_payload( rig ) == std::vector< std::uint8_t >{ 0xaa } );
}

// what either buffer received is handed over, the first buffer's before the second's
BOOST_FIXTURE_TEST_CASE( the_pdus_received_into_both_buffers_are_collected, fixture )
{
    BOOST_REQUIRE( receive_largest_pdu( rig, false ) );

    const step program[] = { on( callback_kind::start, switch_buffer ) };
    load( program );
    remote.call< &rig_t::start_program >();

    BOOST_REQUIRE( receive_largest_pdu( rig, false ) );

    BOOST_CHECK_EQUAL( collect_all_received( remote ).size(), 2u );
}

BOOST_FIXTURE_TEST_CASE( a_full_buffer_refuses_a_queued_pdu, fixture )
{
    const std::array< std::uint8_t, max_data_pdu_size > largest = { 0x02, max_data_pdu_size - 2 };

    for ( std::size_t queued = 0; queued != received_pdus_until_full; ++queued )
        BOOST_REQUIRE( remote.call< &rig_t::queue_pdu >( pdu( largest ) ) );

    BOOST_CHECK( !remote.call< &rig_t::queue_pdu >( pdu( largest ) ) );
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

    const std::vector< pdu > collected = collect_all_received( remote );

    BOOST_REQUIRE_EQUAL( collected.size(), 2u );
    BOOST_CHECK_EQUAL( collected[ 0 ].data[ 2 ], 0 );
    BOOST_CHECK_EQUAL( collected[ 1 ].data[ 2 ], 1 );
}

BOOST_AUTO_TEST_CASE( a_full_record_batch_and_a_full_received_batch_fit_into_one_response )
{
    const std::array< std::uint8_t, max_pdu_size >             largest_pdu = {};
    const std::array< std::uint8_t, max_advertising_pdu_size > largest_advertising_pdu = {};

    record largest;
    largest.data = adv_pdu( largest_advertising_pdu );

    record_batch records;
    records.count = records_per_batch;
    records.items.fill( largest );

    received_batch received;
    received.count = received_per_batch;
    received.pdus.fill( pdu( largest_pdu ) );

    std::array< std::uint8_t, default_max_payload - 1 > response;

    buffer_sink records_out( response );
    BOOST_CHECK( serialize( records_out, records ) );

    buffer_sink received_out( response );
    BOOST_CHECK( serialize( received_out, received ) );
}

/*
 * A step that runs many times: on each callback of its kind its calls are made anew, placed
 * from that callback, and only when it ran as often as it says does the next step wait.
 */
BOOST_FIXTURE_TEST_CASE( a_repeated_step_runs_on_each_of_its_callbacks_before_the_next_step_waits, fixture )
{
    const step program[] = {
        on( callback_kind::start, start_advertising_event( 37, adv_ind ) ),
        repeated( 3, on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ) ) ),
        on( callback_kind::adv_timeout, schedule_timer( 2ms ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.adv_timeout( abs_time( 20000 ) );
    rig.adv_timeout( abs_time( 30000 ) );
    BOOST_CHECK( !remote.call< &rig_t::program_finished >() );

    rig.adv_timeout( abs_time( 40000 ) );

    BOOST_REQUIRE_EQUAL( radio.calls.size(), 5u );
    BOOST_CHECK( radio.calls[ 1 ].kind == call_kind::schedule_advertising_event );
    BOOST_CHECK_EQUAL( radio.calls[ 1 ].when.data(), 11000u );
    BOOST_CHECK( radio.calls[ 3 ].kind == call_kind::schedule_advertising_event );
    BOOST_CHECK_EQUAL( radio.calls[ 3 ].when.data(), 31000u );
    BOOST_CHECK( radio.calls[ 4 ].kind == call_kind::schedule_timer );
    BOOST_CHECK_EQUAL( radio.calls[ 4 ].when.data(), 42000u );
}

// the first run is on record, callback and call; the later ones are only in the numbers
BOOST_FIXTURE_TEST_CASE( a_repeated_step_is_recorded_once_and_counted_from_then_on, fixture )
{
    const step program[] = {
        on( callback_kind::start, start_advertising_event( 37, adv_ind ) ),
        repeated( 3, on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.adv_timeout( abs_time( 20000 ) );
    rig.adv_timeout( abs_time( 30000 ) );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 3u );
    BOOST_CHECK( records[ 0 ].call == call_kind::start_advertising_event );
    BOOST_CHECK( records[ 1 ].callback == callback_kind::adv_timeout );
    BOOST_CHECK_EQUAL( records[ 1 ].when.data(), 10000u );
    BOOST_CHECK( records[ 2 ].call == call_kind::schedule_advertising_event );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( count_of( summary, callback_kind::adv_timeout ), 3u );
    BOOST_CHECK_EQUAL( count_of( summary, callback_kind::start ), 0u );
    BOOST_CHECK_EQUAL( summary.calls, 4u );
    BOOST_CHECK_EQUAL( summary.refused_calls, 0u );
}

// a callback the repeated step does not wait for is not one of its runs
BOOST_FIXTURE_TEST_CASE( another_callback_during_a_repeated_step_is_recorded_and_counted, fixture )
{
    const step program[] = {
        on( callback_kind::start, start_advertising_event( 37, adv_ind ) ),
        repeated( 2, on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ) ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.user_timer( abs_time( 15000 ) );
    rig.adv_timeout( abs_time( 20000 ) );

    const auto records = collect_all();

    BOOST_REQUIRE_EQUAL( records.size(), 4u );
    BOOST_CHECK( records[ 3 ].callback == callback_kind::user_timer );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( count_of( summary, callback_kind::user_timer ), 1u );
    BOOST_CHECK_EQUAL( count_of( summary, callback_kind::adv_timeout ), 2u );
    BOOST_CHECK( remote.call< &rig_t::program_finished >() || radio.calls.size() == 3u );
}

BOOST_FIXTURE_TEST_CASE( the_summary_counts_the_calls_the_radio_refused, fixture )
{
    const step program[] = {
        on( callback_kind::start, start_advertising_event( 37, adv_ind ), set_local_address( device_address{} ) ),
        on( callback_kind::adv_timeout, schedule_advertising_event( 37, 1ms, adv_ind ), cancel_timer() ) };
    load( program );
    remote.call< &rig_t::start_program >();

    radio.answer = false;
    rig.adv_timeout( abs_time( 10000 ) );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    // a setup call and a start cannot be refused; the schedule and the cancel were
    BOOST_CHECK_EQUAL( summary.calls, 4u );
    BOOST_CHECK_EQUAL( summary.refused_calls, 2u );
}

/*
 * The anchor error is the anchor the end event carries minus the centre of the window the step
 * asked for, of the events placed from an anchor: the sleep clock's drift over the interval
 * and the placement. The first event, placed from the advertising, has no anchor before it.
 */
BOOST_FIXTURE_TEST_CASE( the_summary_has_the_anchor_errors_of_the_connection_events, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, 10ms, 12ms ) ),
        repeated( 2, on( callback_kind::connection_end_event, schedule_connection_event( 5, 9750us, 10250us ) ) ),
        on( callback_kind::connection_end_event ) };
    load( program );
    remote.call< &rig_t::start_program >();

    // from the advertising, not measured; then centred on 30005, seen 15 µs early; then on 39990, seen 310 µs late
    rig.adv_timeout( abs_time( 10000 ) );
    rig.connection_end_event( abs_time( 20005 ), {} );
    rig.connection_end_event( abs_time( 29990 ), {} );
    rig.connection_end_event( abs_time( 40300 ), {} );

    BOOST_CHECK( remote.call< &rig_t::program_finished >() );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( summary.anchors, 2u );
    BOOST_CHECK_EQUAL( summary.anchor_error_min, -15 );
    BOOST_CHECK_EQUAL( summary.anchor_error_max, 310 );
    BOOST_CHECK_EQUAL( summary.anchor_errors[ 0 ], 0u );
    BOOST_CHECK_EQUAL( summary.anchor_errors[ 1 ], 1u );
    BOOST_CHECK_EQUAL( summary.anchor_errors[ 2 ], 0u );
    BOOST_CHECK_EQUAL( summary.anchor_errors[ 3 ], 0u );
    BOOST_CHECK_EQUAL( summary.anchor_errors[ 4 ], 1u );
}

// abs_time is a ring: an anchor just past the wrap is still a small error
BOOST_FIXTURE_TEST_CASE( an_anchor_error_across_the_wrap_of_the_time_is_small, fixture )
{
    const step program[] = {
        on( callback_kind::start,                start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout,          schedule_connection_event( 5, 10ms, 12ms ) ),
        on( callback_kind::connection_end_event, schedule_connection_event( 5, 9750us, 10250us ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 0xffffffff - 20000 ) );
    rig.connection_end_event( abs_time( 0xffffffff - 5000 ), {} );
    rig.connection_end_event( abs_time( 4997 ), {} );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( summary.anchors, 1u );
    BOOST_CHECK_EQUAL( summary.anchor_error_min, -2 );
    BOOST_CHECK_EQUAL( summary.anchor_error_max, -2 );
}

// nothing was received, so there is no anchor to compare; the timeout is counted
BOOST_FIXTURE_TEST_CASE( a_connection_timeout_is_counted_and_has_no_anchor, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, 10ms, 12ms ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.connection_timeout( abs_time( 22000 ) );

    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( count_of( summary, callback_kind::connection_timeout ), 1u );
    BOOST_CHECK_EQUAL( summary.anchors, 0u );
}

// an end event the program did not schedule for, a cancelled event's for example, has no error
BOOST_FIXTURE_TEST_CASE( a_cancelled_connection_event_has_no_anchor, fixture )
{
    const step program[] = {
        on( callback_kind::start,                start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout,          schedule_connection_event( 5, 10ms, 12ms ) ),
        on( callback_kind::connection_end_event, schedule_connection_event( 5, 10ms, 12ms ), cancel_radio_event() ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.connection_end_event( abs_time( 20000 ), {} );
    rig.connection_end_event( abs_time( 30000 ), {} );

    BOOST_CHECK_EQUAL( remote.call< &rig_t::collect_summary >().anchors, 0u );
}

// the first event is placed from the advertising: nothing to measure its anchor against
BOOST_FIXTURE_TEST_CASE( a_connection_event_placed_from_the_advertising_has_no_anchor_error, fixture )
{
    const step program[] = {
        on( callback_kind::start,       start_advertising_event( 37, adv_ind ) ),
        on( callback_kind::adv_timeout, schedule_connection_event( 5, 10ms, 12ms ) ) };
    load( program );
    remote.call< &rig_t::start_program >();

    rig.adv_timeout( abs_time( 10000 ) );
    rig.connection_end_event( abs_time( 20500 ), {} );

    BOOST_CHECK_EQUAL( remote.call< &rig_t::collect_summary >().anchors, 0u );
}

BOOST_FIXTURE_TEST_CASE( the_summary_starts_afresh_with_the_program, fixture )
{
    rig.radio_ready();
    rig.user_timer( abs_time( 1 ) );

    BOOST_CHECK_EQUAL( count_of( remote.call< &rig_t::collect_summary >(), callback_kind::radio_ready ), 1u );
    BOOST_CHECK_EQUAL( count_of( remote.call< &rig_t::collect_summary >(), callback_kind::user_timer ), 1u );

    load( {} );
    remote.call< &rig_t::start_program >();

    BOOST_CHECK( remote.call< &rig_t::collect_summary >() == program_summary() );
}

// the host has no radio with statistics, so the summary reports none
BOOST_FIXTURE_TEST_CASE( a_radio_without_clock_statistics_reports_zeros, fixture )
{
    const program_summary summary = remote.call< &rig_t::collect_summary >();

    BOOST_CHECK_EQUAL( summary.crystal_starts, 0u );
    BOOST_CHECK_EQUAL( summary.crystal_ticks, 0u );
    BOOST_CHECK_EQUAL( summary.calibrations, 0u );
}

namespace {

    struct clock_statistics_t
    {
        std::uint32_t crystal_starts;
        std::uint32_t crystal_ticks;
        std::uint32_t calibrations;
    };

    // a radio that keeps the clock statistics the nRF52 radio keeps when asked to
    template < typename CallBacks >
    class radio_with_statistics : public scripted_radio< CallBacks >
    {
    public:
        clock_statistics_t clock_statistics() const
        {
            return { 12, 3456, 7 };
        }
    };

    using rig_with_statistics = dut_rig< radio_with_statistics, observed_port >;
}

BOOST_AUTO_TEST_CASE( the_clock_statistics_of_the_radio_are_in_the_summary )
{
    rig_with_statistics rig( "radio with clock statistics", "unit test build" );

    const program_summary summary = rig.collect_summary();

    BOOST_CHECK_EQUAL( summary.crystal_starts, 12u );
    BOOST_CHECK_EQUAL( summary.crystal_ticks, 3456u );
    BOOST_CHECK_EQUAL( summary.calibrations, 7u );
}

BOOST_AUTO_TEST_CASE( a_summary_round_trips_with_its_signed_errors )
{
    program_summary sent;
    sent.callbacks[ 6 ]     = 6000;
    sent.calls              = 12001;
    sent.refused_calls      = 1;
    sent.anchors            = 5999;
    sent.anchor_error_min   = -47;
    sent.anchor_error_max   = 12;
    sent.anchor_errors[ 1 ] = 5999;
    sent.crystal_starts     = 6010;
    sent.crystal_ticks      = 0x80000001;
    sent.calibrations       = 150;

    std::array< std::uint8_t, default_max_payload - 1 > storage = {};
    buffer_sink out( storage );
    BOOST_REQUIRE( serialize( out, sent ) );

    buffer_source   in( storage.data(), out.size() );
    program_summary decoded;
    BOOST_REQUIRE( deserialize( in, decoded ) );

    BOOST_CHECK( decoded == sent );
    BOOST_CHECK_EQUAL( in.remaining(), 0u );
}
