#include <bluetoe/link_layer/buffer.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <initializer_list>
#include <random>
#include <tuple>
#include <type_traits>

#include "buffer_io.hpp"

struct mock_radio {

};

struct buffer : bluetoe::link_layer::ll_data_pdu_buffer< 100, 100, mock_radio > {};

struct running_mode : bluetoe::link_layer::ll_data_pdu_buffer< 100, 100, mock_radio >
{
    running_mode()
        : random( 42 ) // for testing, deterministic pseudo random is cool
    {
        reset();
    }

    template < class Iter >
    void transmit_pdu( Iter begin, Iter end )
    {
        const auto size = std::distance( begin, end );

        auto buffer = allocate_transmit_buffer( size + 2 );
        assert( buffer.size == size +2 );

        buffer.buffer[ 0 ] = 1;
        buffer.buffer[ 1 ] = size;
        std::copy( begin, end, &buffer.buffer[ 2 ] );

        commit_transmit_buffer( buffer );
    }

    void transmit_pdu( std::initializer_list< std::uint8_t > pdu )
    {
        transmit_pdu( std::begin( pdu ), std::end( pdu ) );
    }

    template < class Iter >
    void receive_pdu( Iter begin, Iter end, bool sn, bool nesn )
    {
        const auto size = std::distance( begin, end );
        auto pdu = allocate_receive_buffer();

        pdu.buffer[ 0 ] = 1;
        pdu.buffer[ 1 ] = size;

        if ( sn )
            pdu.buffer[ 0 ] |= 8;

        if ( nesn )
            pdu.buffer[ 0 ] |= 4;

        std::copy( begin, end, &pdu.buffer[ 2 ] );

        received( pdu );
    }

    void receive_pdu( std::initializer_list< std::uint8_t > pdu, bool sn, bool nesn )
    {
        receive_pdu( std::begin( pdu ), std::end( pdu ), sn, nesn );
    }

    std::vector< std::uint8_t > random_data( std::size_t s )
    {
        std::uniform_int_distribution< std::uint8_t > dist;

        std::vector< std::uint8_t > result;

        for ( ; s; --s )
            result.push_back( dist( random ) );

        return result;
    }

    std::size_t random_size()
    {
        std::uniform_int_distribution< std::uint8_t > dist( 1, max_rx_size() - 2 );
        return dist( random );
    }

    std::size_t random_value( std::size_t begin, std::size_t end )
    {
        std::uniform_int_distribution< std::uint8_t > dist( begin, end );
        return dist( random );
    }

    std::mt19937        random;
};

struct one_element_in_transmit_buffer : running_mode
{
    one_element_in_transmit_buffer()
    {
       transmit_pdu( { 0x34 } );
    }
};

constexpr std::uint8_t simple_pdu[] = {
    0x02, 0x04, 0x12, 0x34, 0x56, 0x78
};

struct received_pdu : running_mode
{
    received_pdu()
    {
        const auto pdu = allocate_receive_buffer();
        std::copy( std::begin( simple_pdu ), std::end( simple_pdu ), pdu.buffer );

        received( pdu );
    }

};

BOOST_FIXTURE_TEST_CASE( raw_accessable_in_stopped_mode, buffer )
{
    BOOST_REQUIRE( raw() );

    // if this doesn't cause a core dump, it's at least a hint that the access was valid
    std::fill( raw(), raw() + size, 0 );
}

BOOST_FIXTURE_TEST_CASE( default_max_rx_size_is_27, buffer )
{
    BOOST_CHECK_EQUAL( max_rx_size(), 27u );
}

BOOST_FIXTURE_TEST_CASE( max_rx_size_can_be_changed, buffer )
{
    max_rx_size( 100u );
    BOOST_CHECK_EQUAL( max_rx_size(), 100u );
}

BOOST_FIXTURE_TEST_CASE( max_rx_is_reset_to_27, buffer )
{
    max_rx_size( 100u );
    reset();

    BOOST_CHECK_EQUAL( max_rx_size(), 27u );
}

BOOST_FIXTURE_TEST_CASE( an_allocated_receive_buffer_must_be_max_rx_in_size, running_mode )
{
    const auto pdu = allocate_receive_buffer();
    BOOST_CHECK_EQUAL( pdu.size, max_rx_size() );
}

BOOST_FIXTURE_TEST_CASE( default_max_tx_size_is_27, buffer )
{
    BOOST_CHECK_EQUAL( max_tx_size(), 27u );
}

BOOST_FIXTURE_TEST_CASE( max_tx_size_can_be_changed, buffer )
{
    max_tx_size( 100u );
    BOOST_CHECK_EQUAL( max_tx_size(), 100u );
}

BOOST_FIXTURE_TEST_CASE( max_tx_is_reset_to_27, buffer )
{
    max_tx_size( 100u );
    reset();

    BOOST_CHECK_EQUAL( max_tx_size(), 27u );
}

BOOST_FIXTURE_TEST_CASE( an_allocated_transmit_buffer_must_be_max_tx_in_size, running_mode )
{
    const auto pdu = allocate_transmit_buffer();
    BOOST_CHECK_EQUAL( pdu.size, max_tx_size() );
}


BOOST_FIXTURE_TEST_CASE( if_only_empty_pdus_are_received_the_buffer_will_never_overflow, running_mode )
{
    // 1/500 == never (by definition)
    for ( int i = 0; i != 500; ++i )
    {
        const auto pdu = allocate_receive_buffer();
        BOOST_REQUIRE( pdu.buffer );
        BOOST_CHECK_GE( pdu.size, 2 );

        pdu.buffer[ 0 ] = 1;
        pdu.buffer[ 1 ] = 0;

        received( pdu );
    }
}

BOOST_FIXTURE_TEST_CASE( at_startup_the_receive_buffer_should_be_empty, running_mode )
{
    BOOST_CHECK_EQUAL( next_received().size, 0 );
}

BOOST_FIXTURE_TEST_CASE( a_received_not_empty_pdu_is_accessable_from_the_link_layer, received_pdu )
{
    const auto received = next_received();

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( simple_pdu ), std::end( simple_pdu ), received.buffer, received.buffer + received.size );
}

BOOST_FIXTURE_TEST_CASE( if_half_buffer_size_is_used_as_max_rx_size_two_pdu_should_fit_into_the_buffer, running_mode )
{
    max_rx_size( max_max_rx_size() / 2 );

    auto read = allocate_receive_buffer();
    BOOST_CHECK_EQUAL( read.size, max_max_rx_size() / 2 );

    // make buffer as used
    read.buffer[ 1 ] = max_max_rx_size() / 2 - 2;
    received( read );

    BOOST_CHECK_EQUAL( allocate_receive_buffer().size, max_max_rx_size() / 2 );
}

BOOST_FIXTURE_TEST_CASE( if_buffer_size_is_used_as_max_rx_size_one_pdu_should_fit_into_the_buffer, running_mode )
{
    max_rx_size( max_max_rx_size() );

    BOOST_CHECK_EQUAL( allocate_receive_buffer().size, max_max_rx_size() );
}

BOOST_FIXTURE_TEST_SUITE( move_random_data_through_the_buffer, running_mode )

template < std::size_t V >
using intt = std::integral_constant< std::size_t, V >;

typedef boost::mpl::list<
    //          max_rx_size  min payload  max payload
    std::tuple< intt< 27 >,  intt< 1 >,   intt< 25 > >,
    std::tuple< intt< 50 >,  intt< 1 >,   intt< 48 > >,
    std::tuple< intt< 100 >, intt< 1 >,   intt< 98 > >,
    std::tuple< intt< 27 >,  intt< 1 >,   intt< 1 > >,
    std::tuple< intt< 27 >,  intt< 0 >,   intt< 25 > >,
    std::tuple< intt< 27 >,  intt< 25 >,  intt< 25 > >,
    std::tuple< intt< 100 >, intt< 98 >,  intt< 98 > >
> test_sizes;

BOOST_AUTO_TEST_CASE_TEMPLATE( move_random_data_through_the_buffer, sizes, test_sizes )
{
    std::vector< std::uint8_t >   test_data = this->random_data( 2048 );
    std::vector< std::uint8_t >   received_data;
    std::vector< std::size_t >    transmit_sizes;
    std::vector< std::size_t >    receive_sizes;

    const std::size_t max_rx_size_value = std::tuple_element< 0, sizes >::type::value;
    const std::size_t min_size          = std::tuple_element< 1, sizes >::type::value;
    const std::size_t max_size          = std::tuple_element< 2, sizes >::type::value;

    max_rx_size( max_rx_size_value );

    auto emergency_counter = 2 * test_data.size();
    bool sequence_number   = false;

    for ( std::size_t send_size = 0; send_size < test_data.size(); --emergency_counter )
    {
        BOOST_REQUIRE( emergency_counter );
        auto read = allocate_receive_buffer();

        if ( read.size )
        {
            const std::size_t size = std::min< std::size_t >( random_value( min_size, max_size ), test_data.size() - send_size );

            if ( size != 0 )
                transmit_sizes.push_back( size );

            read.buffer[ 1 ] = size;
            read.buffer[ 0 ] = 2;

            if ( sequence_number )
                read.buffer[ 0 ] |= 8;

            sequence_number = !sequence_number;

            std::copy( test_data.begin() + send_size, test_data.begin() + send_size + size, &read.buffer[ 2 ] );
            received( read );

            send_size += size;
        }
        else
        {
            auto next = next_received();
            BOOST_REQUIRE( next.size );

            receive_sizes.push_back( next.buffer[ 1 ] );
            BOOST_REQUIRE_LE( receive_sizes.size(), transmit_sizes.size() );
            BOOST_REQUIRE_EQUAL( receive_sizes.back(), transmit_sizes[ receive_sizes.size() - 1 ] );

            received_data.insert( received_data.end(), &next.buffer[ 2 ], &next.buffer[ 2 ] + next.buffer[ 1 ] );
            free_received();
        }
    }

    for ( auto next = next_received(); next.size; next = next_received(), --emergency_counter )
    {
        BOOST_REQUIRE( emergency_counter );

        receive_sizes.push_back( next.buffer[ 1 ] );
        BOOST_REQUIRE_EQUAL( receive_sizes.back(), transmit_sizes[ receive_sizes.size() - 1 ] );

        received_data.insert( received_data.end(), &next.buffer[ 2 ], &next.buffer[ 2 ] + next.buffer[ 1 ] );
        free_received();
    }

    BOOST_CHECK_EQUAL_COLLECTIONS( test_data.begin(), test_data.end(), received_data.begin(), received_data.end() );
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_FIXTURE_TEST_CASE( the_transmitbuffer_will_yield_an_empty_pdu_if_the_buffer_is_empty, running_mode )
{
    auto write = next_transmit();

    BOOST_CHECK_EQUAL( write.size, 2 );
    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x03, 1 );
    BOOST_CHECK_EQUAL( write.buffer[ 1 ], 0 );
}

BOOST_FIXTURE_TEST_CASE( if_transmit_buffer_is_empty_mode_data_flag_is_not_set, running_mode )
{
    auto write = next_transmit();

    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x10, 0 );
}

BOOST_FIXTURE_TEST_CASE( as_long_as_an_pdu_is_not_acknowlaged_it_will_be_retransmited, one_element_in_transmit_buffer )
{
    for ( int i = 0; i != 3; ++i )
    {
        auto trans = next_transmit();
        BOOST_REQUIRE_EQUAL( trans.size, 3 );
        BOOST_CHECK_EQUAL( trans.buffer[ 1 ], 1 );
        BOOST_CHECK_EQUAL( trans.buffer[ 2 ], 0x34 );
    }
}

BOOST_FIXTURE_TEST_CASE( sequence_number_and_next_sequence_number_must_be_0_for_the_first_empty_pdu, one_element_in_transmit_buffer )
{
    auto write = next_transmit();

    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x4, 0 );
    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x8, 0 );
}

BOOST_FIXTURE_TEST_CASE( sequence_number_and_next_sequence_number_must_be_0_for_the_first_pdu, one_element_in_transmit_buffer )
{
    auto write = next_transmit();

    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x4, 0 );
    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x8, 0 );
}

BOOST_FIXTURE_TEST_CASE( only_one_transmitbuffer_entry_allocatable, running_mode )
{
    max_tx_size( 100 );

    auto write1 = allocate_transmit_buffer();
    write1.buffer[ 1 ] = 98;
    commit_transmit_buffer( write1 );

    auto write2 = allocate_transmit_buffer();
    BOOST_CHECK_EQUAL( write2.size, 0 );
}

BOOST_FIXTURE_TEST_CASE( more_data_flag_is_not_set_if_only_one_element_is_in_the_transmit_buffer, one_element_in_transmit_buffer )
{
    auto write = next_transmit();
    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x10, 0 );
}

BOOST_FIXTURE_TEST_CASE( more_data_flag_is_set_if_there_is_more_than_one_element_in_the_transmit_buffer, one_element_in_transmit_buffer )
{
    transmit_pdu( { 0x01 } );

    auto transmit = next_transmit();
    BOOST_CHECK_EQUAL( transmit.buffer[ 0 ] & 0x10, 0x10 );
}

BOOST_FIXTURE_TEST_CASE( more_data_flag_is_added_if_pdu_is_added, running_mode )
{
    // empty PDU without MD flag
    auto first = next_transmit();
    BOOST_CHECK_EQUAL( first.buffer[ 0 ] & 0x10, 0 );

    transmit_pdu( { 0x01, 0x02, 0x03, 0x04 } );

    auto next = next_transmit();

    // must be the same PDU, as it was not acknowladged
    BOOST_CHECK_EQUAL_COLLECTIONS( &first.buffer[ 2 ], &first.buffer[ first.size ], &next.buffer[ 2 ], &next.buffer[ next.size ] );

    // sequence numbers and LLID must be equal
    BOOST_CHECK_EQUAL( first.buffer[ 0 ] & 0xf, first.buffer[ 0 ] & 0xf );
}

BOOST_FIXTURE_TEST_CASE( a_new_pdu_will_be_transmitted_if_the_last_was_acknowladged, running_mode )
{
    transmit_pdu( { 1 } );
    transmit_pdu( { 2 } );
    transmit_pdu( { 3 } );
    transmit_pdu( { 4 } );

    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 1 );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 1 );

    // incomming PDU acknowledges
    auto incomming = allocate_receive_buffer();
    incomming.buffer[ 0 ] = 1 | 4;
    incomming.buffer[ 1 ] = 0;
    received( incomming );

    // now the next pdu to be transmitted
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 2 );

    // next incomming PDU acknowledges, this time with NESN = 0
    incomming = allocate_receive_buffer();
    incomming.buffer[ 0 ] = 1;
    incomming.buffer[ 1 ] = 25;
    received( incomming );

    // now the next pdu to be transmitted
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 3 );
}

BOOST_FIXTURE_TEST_CASE( received_pdu_with_LLID_0_is_ignored, running_mode )
{
    auto pdu = allocate_receive_buffer();
    pdu.buffer[ 0 ] = 0;
    pdu.buffer[ 1 ] = 1;

    received( pdu );

    BOOST_CHECK_EQUAL( next_received().size, 0 );
}

BOOST_FIXTURE_TEST_CASE( received_pdus_are_ignored_when_they_are_resent, running_mode )
{
    receive_pdu( { 1 }, false, false );
    receive_pdu( { 2 }, false, false );

    BOOST_CHECK_EQUAL( next_received().buffer[ 2 ], 1 );
    free_received();

    BOOST_CHECK_EQUAL( next_received().size, 0 );
}

BOOST_FIXTURE_TEST_CASE( with_every_new_received_pdu_a_new_sequence_is_expected, running_mode )
{
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0 );

    receive_pdu( { 1 }, false, false );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0x4 );

    receive_pdu( { 2 }, true, false );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0 );
}