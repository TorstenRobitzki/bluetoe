/**
 * @file simulated_radio_tests.cpp
 *
 * The simulated radio of the scheduled_radio2 interface, tested against a model of what a
 * link layer is to it: that it satisfies the concept, and that what it simulates arrives as
 * the callbacks with the times the interface promises.
 *
 * What the radio records for a test is radio_base's and is covered by test_radio_tests.cpp.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "simulated_radio.hpp"

#include <bluetoe/ll_data_pdu_buffer.hpp>

#include <vector>

namespace {

    using namespace bluetoe::link_layer;

    struct model_link_layer;

    using radio_t = test::simulated_radio< model_link_layer >;

    /*
     * The PDU buffer as the link layer will own it: laid out and locked for the radio, with
     * the functions the radio calls made public. That the buffer needs to be a base of the
     * radio no longer holds, which is what lets the link layer own it.
     */
    struct pdu_buffer : ll_data_pdu_buffer< 100, 100, radio_t >
    {
        using base_t = ll_data_pdu_buffer< 100, 100, radio_t >;

        using base_t::allocate_receive_buffer;
        using base_t::received;
        using base_t::next_transmit;
    };

    /*
     * What the radio delivers to. A link layer in the shape of the concept and nothing more:
     * it records what it was told, so that a test can say what the radio did.
     */
    struct model_link_layer : radio_t
    {
        void radio_ready()
        {
            ++radio_ready_;
        }

        void adv_received( abs_time when, const read_buffer& received )
        {
            adv_received_.push_back( when );
            received_size_ = received.size;
        }

        void adv_timeout( abs_time when )
        {
            adv_timeout_.push_back( when );
        }

        void user_timer( abs_time when )
        {
            user_timer_.push_back( when );
        }

        void connection_timeout( abs_time when )
        {
            connection_timeout_.push_back( when );
        }

        void connection_end_event( abs_time when, connection_event_events )
        {
            connection_end_event_.push_back( when );
        }

        bool is_in_acceptance_filter( const device_address& )
        {
            return true;
        }

        pdu_buffer& link_layer_pdu_buffer()
        {
            return buffer_;
        }

        pdu_buffer              buffer_;

        int                     radio_ready_ = 0;
        std::vector< abs_time > adv_received_;
        std::vector< abs_time > adv_timeout_;
        std::vector< abs_time > user_timer_;
        std::vector< abs_time > connection_timeout_;
        std::vector< abs_time > connection_end_event_;
        std::size_t             received_size_ = 0;
    };

    static_assert( scheduled_radio_callbacks< model_link_layer > );
    static_assert( software_acceptance_filter< model_link_layer > );
    static_assert( scheduled_radio_pdu_buffer< pdu_buffer > );

    /*
     * A radio is a template over the type it delivers to, so that is how the concept asks
     * for it.
     */
    template < typename CallBacks >
    using radio_under_test = test::simulated_radio< CallBacks >;

    static_assert( scheduled_radio< radio_under_test, model_link_layer > );

    struct fixture : model_link_layer
    {
        fixture()
        {
            set_access_address_and_crc_init( 0x12345678, 0x123456 );
            buffer_.reset_pdu_buffer();
            end_of_simulation( delta_time::seconds( 10 ) );
        }

        /*
         * The advertising PDU every test here sends, in the layout the radio stores PDUs in
         * rather than in the layout it puts them on air.
         */
        static std::vector< std::uint8_t > advertising()
        {
            static constexpr std::size_t    payload_size = 2;
            static constexpr std::uint16_t  header       = 0x42 | ( payload_size << 8 );

            std::vector< std::uint8_t > pdu( test::pdu_layout::data_channel_pdu_memory_size( payload_size ) );

            test::pdu_layout::header( &pdu[ 0 ], header );

            const auto body = test::pdu_layout::body( read_buffer{ pdu.data(), pdu.size() } );
            body.first[ 0 ] = 0xaa;
            body.first[ 1 ] = 0xbb;

            return pdu;
        }
    };
}

BOOST_FIXTURE_TEST_CASE( the_radio_reports_that_it_is_ready, fixture )
{
    BOOST_CHECK_EQUAL( radio_ready_, 0 );

    run();

    BOOST_CHECK_EQUAL( radio_ready_, 1 );
}

BOOST_FIXTURE_TEST_CASE( the_radio_reports_ready_only_once, fixture )
{
    run();
    run();

    BOOST_CHECK_EQUAL( radio_ready_, 1 );
}

/*
 * start_advertising_event() names no time; what is scheduled is recorded and, with nobody
 * answering, times out.
 */
BOOST_FIXTURE_TEST_CASE( an_advertising_without_a_response_times_out, fixture )
{
    const auto pdu = advertising();

    start_advertising_event( 37, write_buffer( pdu.data(), pdu.size() ), write_buffer(), read_buffer() );

    run();

    BOOST_REQUIRE_EQUAL( advertisings().size(), 1u );
    BOOST_CHECK_EQUAL( advertisings()[ 0 ].channel, 37u );

    BOOST_CHECK( adv_received_.empty() );
    BOOST_REQUIRE_EQUAL( adv_timeout_.size(), 1u );
}

/*
 * An advertising event asks for a time, and the radio refuses one that has gone by.
 */
BOOST_FIXTURE_TEST_CASE( an_advertising_event_in_the_past_is_refused, fixture )
{
    const auto pdu = advertising();

    BOOST_CHECK( !schedule_advertising_event(
        37, now() - delta_time::msec( 1 ),
        write_buffer( pdu.data(), pdu.size() ), write_buffer(), read_buffer() ) );

    BOOST_CHECK( advertisings().empty() );
}

BOOST_FIXTURE_TEST_CASE( an_advertising_event_is_scheduled_for_the_time_it_names, fixture )
{
    const auto pdu = advertising();
    const auto when = now() + delta_time::msec( 100 );

    BOOST_REQUIRE( schedule_advertising_event(
        37, when, write_buffer( pdu.data(), pdu.size() ), write_buffer(), read_buffer() ) );

    run();

    BOOST_REQUIRE_EQUAL( adv_timeout_.size(), 1u );

    // the event was on air at the time it was scheduled for
    BOOST_CHECK( adv_timeout_[ 0 ] == when );
}

/*
 * A response arrives one inter frame space after the advertising, and the time the callback
 * carries is the time it was on air.
 */
BOOST_FIXTURE_TEST_CASE( an_answered_advertising_reports_the_response, fixture )
{
    const auto pdu = advertising();
    std::uint8_t receive[ 100 ] = { 0 };

    respond_to( 37, { 0x43, 0x02, 0x01, 0x02 } );

    start_advertising_event(
        37, write_buffer( pdu.data(), pdu.size() ), write_buffer(),
        read_buffer{ &receive[ 0 ], sizeof( receive ) } );

    run();

    BOOST_CHECK( adv_timeout_.empty() );
    BOOST_REQUIRE_EQUAL( adv_received_.size(), 1u );
    BOOST_CHECK( received_size_ != 0 );
}

/*
 * A connection event names its start and its end; a time too close to set the radio up is
 * refused, as hardware would.
 */
BOOST_FIXTURE_TEST_CASE( a_connection_event_too_close_is_refused, fixture )
{
    BOOST_CHECK( !schedule_connection_event(
        6, now() + delta_time::usec( 10 ), now() + delta_time::msec( 1 ) ) );

    BOOST_CHECK( connection_events().empty() );
}

BOOST_FIXTURE_TEST_CASE( a_connection_event_without_an_answer_times_out, fixture )
{
    const auto start = now() + delta_time::msec( 10 );
    const auto end   = start + delta_time::msec( 1 );

    BOOST_REQUIRE( schedule_connection_event( 6, start, end ) );

    run();

    BOOST_REQUIRE_EQUAL( connection_events().size(), 1u );
    BOOST_CHECK_EQUAL( connection_events()[ 0 ].channel, 6u );

    BOOST_REQUIRE_EQUAL( connection_timeout_.size(), 1u );
    BOOST_CHECK( connection_timeout_[ 0 ] == end );
}

/*
 * A connection event that is answered reports its anchor, which is where the PDU of the
 * central was received.
 */
BOOST_FIXTURE_TEST_CASE( an_answered_connection_event_reports_its_anchor, fixture )
{
    const auto start = now() + delta_time::msec( 10 );
    const auto end   = start + delta_time::msec( 1 );

    add_connection_event_respond( { 0x01, 0x00 } );

    BOOST_REQUIRE( schedule_connection_event( 6, start, end ) );

    run();

    BOOST_CHECK( connection_timeout_.empty() );
    BOOST_REQUIRE_EQUAL( connection_end_event_.size(), 1u );
    BOOST_CHECK( connection_end_event_[ 0 ] == start );

    BOOST_REQUIRE_EQUAL( connection_events().size(), 1u );
    BOOST_CHECK( !connection_events()[ 0 ].transmitted_data.empty() );
}

/*
 * An event that was scheduled and not yet simulated can be taken back.
 */
BOOST_FIXTURE_TEST_CASE( a_scheduled_connection_event_can_be_cancelled, fixture )
{
    BOOST_REQUIRE( schedule_connection_event(
        6, now() + delta_time::msec( 10 ), now() + delta_time::msec( 11 ) ) );

    BOOST_CHECK( cancel_radio_event() );
    BOOST_CHECK( connection_events().empty() );

    // and there is nothing left to cancel
    BOOST_CHECK( !cancel_radio_event() );
}

/*
 * The timer is named a time and reports that time back.
 */
BOOST_FIXTURE_TEST_CASE( the_timer_expires_at_the_time_it_was_given, fixture )
{
    const auto start = now() + delta_time::msec( 10 );
    const auto end   = start + delta_time::msec( 1 );
    const auto timer = now() + delta_time::msec( 5 );

    BOOST_REQUIRE( schedule_timer( timer ) );

    add_connection_event_respond( { 0x01, 0x00 } );
    BOOST_REQUIRE( schedule_connection_event( 6, start, end ) );

    run();

    BOOST_REQUIRE_EQUAL( user_timer_.size(), 1u );
    BOOST_CHECK( user_timer_[ 0 ] == timer );
}

BOOST_FIXTURE_TEST_CASE( a_timer_in_the_past_is_refused, fixture )
{
    BOOST_CHECK( !schedule_timer( now() - delta_time::msec( 1 ) ) );
}

BOOST_FIXTURE_TEST_CASE( a_scheduled_timer_can_be_cancelled, fixture )
{
    BOOST_REQUIRE( schedule_timer( now() + delta_time::msec( 5 ) ) );

    BOOST_CHECK( cancel_timer() );
    BOOST_CHECK( !cancel_timer() );
}
