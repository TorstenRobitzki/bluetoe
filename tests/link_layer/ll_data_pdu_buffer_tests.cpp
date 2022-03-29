#include <iostream>
#include <buffer_io.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <initializer_list>
#include <random>
#include <tuple>
#include <type_traits>

#include "buffer_io.hpp"

static bool radio_locked = false;

class lock_guard
{
public:
    lock_guard()
    {
        assert( !radio_locked );
        radio_locked = true;
    }

    ~lock_guard()
    {
        radio_locked = false;
    }

private:
    lock_guard( const lock_guard& ) = delete;
    lock_guard& operator=( const lock_guard& ) = delete;
};

template < std::size_t TransmitSize, std::size_t ReceiveSize >
struct mock_radio : bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, mock_radio< TransmitSize, ReceiveSize > > {
    using lock_guard = ::lock_guard;

    void increment_receive_packet_counter()
    {
        ++receive_packet_counter_;
    }

    void increment_transmit_packet_counter()
    {
        ++transmit_packet_counter_;
    }

    int receive_packet_counter() const
    {
        return receive_packet_counter_;
    }

    int transmit_packet_counter() const
    {
        return transmit_packet_counter_;
    }

    int receive_packet_counter_;
    int transmit_packet_counter_;

    mock_radio()
        : receive_packet_counter_( 0 )
        , transmit_packet_counter_( 0 )
    {
    }
};

using buffer = mock_radio< 100, 100 >;

template < std::size_t TransmitSize, std::size_t ReceiveSize, template < std::size_t, std::size_t > class Radio >
struct running_mode_impl : Radio< TransmitSize, ReceiveSize >
{
    using layout = typename bluetoe::link_layer::pdu_layout_by_radio< Radio< TransmitSize, ReceiveSize > >::pdu_layout;

    running_mode_impl()
        : random( 42 ) // for testing, deterministic pseudo random is cool
    {
        this->reset();
    }

    template < class Iter >
    void transmit_pdu( Iter begin, Iter end )
    {
        const std::size_t size = std::distance( begin, end );

        auto buffer = this->allocate_transmit_buffer( layout::data_channel_pdu_memory_size( size ) );
        assert( buffer.size == layout::data_channel_pdu_memory_size( size ) );

        layout::header( buffer, 1 | ( size << 8 ) );

        std::copy( begin, end, layout::body( buffer ).first );

        this->commit_transmit_buffer( buffer );
    }

    void transmit_pdu( std::initializer_list< std::uint8_t > pdu )
    {
        transmit_pdu( std::begin( pdu ), std::end( pdu ) );
    }

    template < class Iter >
    void receive_pdu( Iter begin, Iter end, bool sn, bool nesn )
    {
        const auto size = std::distance( begin, end );
        auto pdu = this->allocate_receive_buffer();

        std::uint16_t header = 1 | ( size << 8 );

        if ( sn )
            header |= 8;

        if ( nesn )
            header |= 4;

        layout::header( pdu, header );
        std::copy( begin, end, layout::body( pdu ).first );

        this->received( pdu );
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
        std::uniform_int_distribution< std::uint8_t > dist( 1, this->max_rx_size() - 2 );
        return dist( random );
    }

    std::size_t random_value( std::size_t begin, std::size_t end )
    {
        std::uniform_int_distribution< std::uint8_t > dist( begin, end );
        return dist( random );
    }

    std::mt19937        random;
};

using running_mode = running_mode_impl< 100, 100, mock_radio >;

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

BOOST_FIXTURE_TEST_CASE( raw_accessable_in_stopped_mode, buffer )
{
    BOOST_REQUIRE( raw() );

    // if this doesn't cause a core dump, it's at least a hint that the access was valid
    std::fill( raw(), raw() + size, 0 );
}

BOOST_FIXTURE_TEST_CASE( layout_overhead_is_zero, buffer )
{
    BOOST_CHECK_EQUAL( std::size_t{ layout_overhead }, 0u );
}

BOOST_FIXTURE_TEST_CASE( default_max_rx_size_is_29, buffer )
{
    BOOST_CHECK_EQUAL( max_rx_size(), 29u );
}

BOOST_FIXTURE_TEST_CASE( max_rx_size_can_be_changed, buffer )
{
    max_rx_size( 100u );
    BOOST_CHECK_EQUAL( max_rx_size(), 100u );
}

BOOST_FIXTURE_TEST_CASE( max_rx_is_reset_to_29, buffer )
{
    max_rx_size( 100u );
    reset();

    BOOST_CHECK_EQUAL( max_rx_size(), 29u );
}

BOOST_FIXTURE_TEST_CASE( an_allocated_receive_buffer_must_be_max_rx_in_size, running_mode )
{
    const auto pdu = allocate_receive_buffer();
    BOOST_CHECK_EQUAL( pdu.size, max_rx_size() );
}

BOOST_FIXTURE_TEST_CASE( default_max_tx_size_is_29, buffer )
{
    BOOST_CHECK_EQUAL( max_tx_size(), 29u );
}

BOOST_FIXTURE_TEST_CASE( max_tx_size_can_be_changed, buffer )
{
    max_tx_size( 100u );
    BOOST_CHECK_EQUAL( max_tx_size(), 100u );
}

BOOST_FIXTURE_TEST_CASE( max_tx_is_reset_to_29, buffer )
{
    max_tx_size( 100u );
    reset();

    BOOST_CHECK_EQUAL( max_tx_size(), 29u );
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
        BOOST_CHECK_GE( pdu.size, 2u );

        pdu.buffer[ 0 ] = 1;
        pdu.buffer[ 1 ] = 0;

        received( pdu );
    }
}

BOOST_FIXTURE_TEST_CASE( at_startup_the_receive_buffer_should_be_empty, running_mode )
{
    BOOST_CHECK_EQUAL( next_received().size, 0u );
}

struct received_pdu : running_mode
{
    received_pdu()
    {
        const auto pdu = allocate_receive_buffer();
        std::copy( std::begin( simple_pdu ), std::end( simple_pdu ), pdu.buffer );

        received( pdu );
    }

};

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
    std::tuple< intt< 29 >,  intt< 1 >,   intt< 25 > >
    // std::tuple< intt< 50 >,  intt< 1 >,   intt< 48 > >,
    // std::tuple< intt< 29 >,  intt< 1 >,   intt< 1 > >,
    // std::tuple< intt< 29 >,  intt< 0 >,   intt< 25 > >,
    // std::tuple< intt< 29 >,  intt< 25 >,  intt< 25 > >
> test_sizes;

BOOST_AUTO_TEST_CASE_TEMPLATE( move_random_data_through_the_buffer, sizes, test_sizes )
{
    std::vector< std::uint8_t >   test_data = this->random_data( 20 );
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

        // if there is room in the receive buffer, I allocate that memory and simulate a received PDU
        if ( read.size )
        {
            const std::size_t size = std::min< std::size_t >( random_value( min_size, max_size ), test_data.size() - send_size );

            if ( size != 0 )
            {
                transmit_sizes.push_back( size );
            }

            read.buffer[ 1 ] = size;
            read.buffer[ 0 ] = 2;

            if ( sequence_number )
                read.buffer[ 0 ] |= 8;

            sequence_number = !sequence_number;

            std::copy( test_data.begin() + send_size, test_data.begin() + send_size + size, &read.buffer[ 2 ] );
            received( read );

            send_size += size;
        }
        // if there is no more room left, I simulate the receiving of an pdu
        else
        {
            auto next = next_received();
            BOOST_REQUIRE_NE( next.size, 0u );
            BOOST_REQUIRE_NE( next.buffer[ 1 ], 0u );

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
        BOOST_REQUIRE_LE( receive_sizes.size(), transmit_sizes.size() );
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

    BOOST_CHECK_EQUAL( write.size, 2u );
    BOOST_CHECK_EQUAL( write.buffer[ 0 ] & 0x03, 1 );
    BOOST_CHECK_EQUAL( write.buffer[ 1 ], 0u );
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
        BOOST_REQUIRE_EQUAL( trans.size, 3u );
        BOOST_CHECK_EQUAL( trans.buffer[ 1 ], 1u );
        BOOST_CHECK_EQUAL( trans.buffer[ 2 ], 0x34u );
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
    write1.buffer[ 0 ] = 0;
    write1.buffer[ 1 ] = 98;
    commit_transmit_buffer( write1 );

    auto write2 = allocate_transmit_buffer();
    BOOST_CHECK_EQUAL( write2.size, 0u );
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

    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 1u );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 1u );

    // incomming PDU acknowledges
    auto incomming = allocate_receive_buffer();
    incomming.buffer[ 0 ] = 1 | 4;
    incomming.buffer[ 1 ] = 0;
    received( incomming );

    // now the next pdu to be transmitted
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 2u );

    // next incomming PDU acknowledges, this time with NESN = 0
    incomming = allocate_receive_buffer();
    incomming.buffer[ 0 ] = 1;
    incomming.buffer[ 1 ] = 25;
    received( incomming );

    // now the next pdu to be transmitted
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 2 ], 3u );
}

BOOST_FIXTURE_TEST_CASE( received_pdu_with_LLID_0_is_ignored, running_mode )
{
    auto pdu = allocate_receive_buffer();
    pdu.buffer[ 0 ] = 0;
    pdu.buffer[ 1 ] = 1;

    received( pdu );

    BOOST_CHECK_EQUAL( next_received().size, 0u );
}

BOOST_FIXTURE_TEST_CASE( received_pdus_are_ignored_when_they_are_resent, running_mode )
{
    receive_pdu( { 1 }, false, false );
    receive_pdu( { 2 }, false, false );

    BOOST_CHECK_EQUAL( next_received().buffer[ 2 ], 1u );
    free_received();

    BOOST_CHECK_EQUAL( next_received().size, 0u );
}

BOOST_FIXTURE_TEST_CASE( with_every_new_received_pdu_a_new_sequence_is_expected, running_mode )
{
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0 );

    receive_pdu( { 1 }, false, false );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0x4 );

    receive_pdu( { 2 }, true, false );
    BOOST_CHECK_EQUAL( next_transmit().buffer[ 0 ] & 0x4, 0 );
}

BOOST_FIXTURE_TEST_CASE( getting_an_empty_pdu_must_not_result_in_changing_allocated_transmit_buffer, running_mode )
{
    auto trans = allocate_transmit_buffer();
    std::fill( trans.buffer, trans.buffer + trans.size, 0x22 );

    // this call should not change the allocated buffer
    next_transmit();

    BOOST_CHECK( std::find_if( trans.buffer, trans.buffer + trans.size, []( std::uint8_t b ) { return b != 0x22; } ) == trans.buffer + trans.size );
}

// buffer with default sizes.
struct default_buffer : mock_radio< 3 * 29, 3 * 29 >
{
    default_buffer()
    {
        reset();
        buffer = allocate_receive_buffer();
    }

    void receive_and_consume( std::initializer_list< std::uint8_t > pdu )
    {
        BOOST_REQUIRE_GE( buffer.size, pdu.size() );

        std::copy( pdu.begin(), pdu.end(), buffer.buffer );
        received( buffer );
        buffer = allocate_receive_buffer();
        BOOST_REQUIRE_GE( buffer.size, 29u );

        if ( pdu.size() == 2 )
        {
            BOOST_CHECK( !next_received().size );
        }
        else
        {
            BOOST_CHECK( next_received().size );
            free_received();
        }

        BOOST_CHECK( !next_received().size );
    }

    bluetoe::read_buffer buffer;
};

BOOST_FIXTURE_TEST_CASE( receive_wrap_around, default_buffer )
{
    receive_and_consume( { 0x03, 0x06, 0x0c, 0x07, 0x0f, 0x00, 0x0d, 0x41 } );
    receive_and_consume( { 0x0e, 0x07, 0x03, 0x00, 0x04, 0x00, 0x02 ,0x9e, 0x00 } );
    receive_and_consume( { 0x01, 0x00 } );
    receive_and_consume( { 0x0e, 0x0b, 0x07, 0x00, 0x04, 0x00, 0x10, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28 } );
    receive_and_consume( { 0x01, 0x00 } );
    receive_and_consume( { 0x0e, 0x0b, 0x07, 0x00, 0x04, 0x00, 0x10, 0x05, 0x00, 0xff, 0xff, 0x00, 0x28 } );
}

namespace changed_pdu_layout
{
    /*
     * lets invert all bits in the header and use an extra byte in front and behind the payload
     */
    struct pdu_layout : bluetoe::link_layer::details::layout_base< pdu_layout > {

        using bluetoe::link_layer::details::layout_base< pdu_layout >::header;

        /**
         * @brief returns the header for advertising channel and for data channel PDUs.
         */
        static std::uint16_t header( const std::uint8_t* pdu )
        {
            return bluetoe::details::read_16bit( pdu ) ^ 0xffff;
        }

        /**
         * @brief writes to the header of the given PDU
         */
        static void header( std::uint8_t* pdu, std::uint16_t header_value )
        {
            bluetoe::details::write_16bit( pdu, header_value ^ 0xffff );
        }

        /**
         * @brief returns the writable body for advertising channel or for data channel PDUs.
         */
        static std::pair< std::uint8_t*, std::uint8_t* > body( const bluetoe::read_buffer& pdu )
        {
            return {
                &pdu.buffer[ sizeof( std::uint16_t ) + 1 ],
                &pdu.buffer[ pdu.size - 1 ]
            };
        }

        /**
         * @brief returns the readonly body for advertising channel or for data channel PDUs.
         */
        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const bluetoe::write_buffer& pdu )
        {
            return {
                &pdu.buffer[ sizeof( std::uint16_t ) + 1 ],
                &pdu.buffer[ pdu.size - 1 ]
            };
        }

        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
        {
            return sizeof( std::uint16_t ) + 2 + payload_size;
        }
    };

    template < std::size_t TransmitSize, std::size_t ReceiveSize >
    struct mock_radio : bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, mock_radio< TransmitSize, ReceiveSize > >
    {
        using lock_guard = ::lock_guard;

        void increment_receive_packet_counter() {}

        void increment_transmit_packet_counter() {}
    };
}

/*
 * specialize pdu_layout_by_radio to apply the changed layout to the mocked radio
 */
namespace bluetoe
{
    namespace link_layer
    {
        template < std::size_t TransmitSize, std::size_t ReceiveSize >
        struct pdu_layout_by_radio< changed_pdu_layout::mock_radio< TransmitSize, ReceiveSize > > {
            using pdu_layout = changed_pdu_layout::pdu_layout;
        };
    }
}

/**
 * Tests to make sure, that the buffer is using the types and functions of the pdu_layout provided by the radio
 */
BOOST_AUTO_TEST_SUITE( layout_tests )

    using buffer_under_test = running_mode_impl< 31, 31, changed_pdu_layout::mock_radio >;
    using large_buffer_under_test = running_mode_impl< 200, 200, changed_pdu_layout::mock_radio >;

    BOOST_FIXTURE_TEST_CASE( make_sure_the_layout_is_used, buffer_under_test )
    {
        BOOST_CHECK( ( std::is_same< changed_pdu_layout::pdu_layout, layout >::value ) );
        BOOST_CHECK_EQUAL( std::size_t{ layout_overhead }, 2u );
    }

    BOOST_FIXTURE_TEST_CASE( default_buffer_sizes, buffer_under_test )
    {
        BOOST_CHECK_EQUAL( max_max_tx_size(), 29u );
        BOOST_CHECK_EQUAL( max_tx_size(), 29u );
        BOOST_CHECK_EQUAL( max_max_rx_size(), 29u );
        BOOST_CHECK_EQUAL( max_rx_size(), 29u );
    }

    BOOST_FIXTURE_TEST_CASE( default_large_buffer_sizes, large_buffer_under_test )
    {
        BOOST_CHECK_EQUAL( max_max_tx_size(), 198u );
        BOOST_CHECK_EQUAL( max_tx_size(), 29u );
        BOOST_CHECK_EQUAL( max_max_rx_size(), 198u );
        BOOST_CHECK_EQUAL( max_rx_size(), 29u );
    }

    BOOST_FIXTURE_TEST_CASE( lower_bound_sizes, large_buffer_under_test )
    {
        max_tx_size( 29 );
        max_rx_size( 29 );
        BOOST_CHECK_EQUAL( max_tx_size(), 29u );
        BOOST_CHECK_EQUAL( max_rx_size(), 29u );
    }

    BOOST_FIXTURE_TEST_CASE( upper_bound_sizes, large_buffer_under_test )
    {
        max_tx_size( 198 );
        max_rx_size( 198 );
        BOOST_CHECK_EQUAL( max_tx_size(), 198u );
        BOOST_CHECK_EQUAL( max_rx_size(), 198u );
    }

    BOOST_FIXTURE_TEST_CASE( allocating_lower_bound_buffers, large_buffer_under_test )
    {
        const auto receive  = allocate_receive_buffer();
        const auto transmit = allocate_transmit_buffer();
        BOOST_CHECK( receive.buffer );
        BOOST_CHECK( transmit.buffer );
        BOOST_CHECK_EQUAL( receive.size, 31u );
        BOOST_CHECK_EQUAL( transmit.size, 31u );
    }

    BOOST_FIXTURE_TEST_CASE( allocating_upper_bound_buffers, large_buffer_under_test )
    {
        max_tx_size( 198u );
        max_rx_size( 198u );

        const auto receive  = allocate_receive_buffer();
        const auto transmit = allocate_transmit_buffer();
        BOOST_CHECK( receive.buffer );
        BOOST_CHECK( transmit.buffer );
        BOOST_CHECK_EQUAL( receive.size, 200u );
        BOOST_CHECK_EQUAL( transmit.size, 200u );
    }

    BOOST_FIXTURE_TEST_CASE( make_sure_the_layout_is_applied_as_expected, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };
        const std::size_t size = sizeof( pattern_a );

        auto buffer = this->allocate_transmit_buffer( size + 4 );
        BOOST_REQUIRE_EQUAL( buffer.size, size + 4 );

        layout::header( buffer, 1 | ( size << 8 ) );

        std::copy( std::begin( pattern_a ), std::end( pattern_a ), layout::body( buffer ).first );

        // header at the begining
        BOOST_CHECK_EQUAL( bluetoe::details::read_16bit( buffer.buffer ), 0xFAFE );

        // body begins 1 byte behind the header and ends 1 byte before the end of the buffer
        BOOST_CHECK( layout::body( buffer ).first == &buffer.buffer[ 3 ] );
        BOOST_CHECK_EQUAL_COLLECTIONS(
            std::begin( pattern_a ), std::end( pattern_a ),
            &buffer.buffer[ 3 ], &buffer.buffer[ buffer.size - 1 ] );
    }

    BOOST_FIXTURE_TEST_CASE( sending_data, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };

        transmit_pdu( std::begin( pattern_a ), std::end( pattern_a ) );

        auto transmit = next_transmit();

        BOOST_CHECK_EQUAL( transmit.buffer[ 1 ] ^ 0xff , 5 );
        BOOST_CHECK_EQUAL_COLLECTIONS(
            std::begin( pattern_a ), std::end( pattern_a ),
            &transmit.buffer[ 3 ], &transmit.buffer[ 3 + 5 ] );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_data, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };

        receive_pdu( std::begin( pattern_a ), std::end( pattern_a ), false, true );

        auto received = next_received();

        BOOST_REQUIRE( received.size );
        BOOST_CHECK_EQUAL( received.buffer[ 1 ] ^ 0xff , 5 );
        BOOST_CHECK_EQUAL_COLLECTIONS(
            std::begin( pattern_a ), std::end( pattern_a ),
            &received.buffer[ 3 ], &received.buffer[ 3 + 5 ] );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_multiple_data, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };

        receive_pdu( std::begin( pattern_a ), std::end( pattern_a ), false, true );

        auto received = next_received();
        BOOST_REQUIRE( received.size );

        auto next_received = allocate_receive_buffer();
        BOOST_REQUIRE( !next_received.size );
    }

    BOOST_FIXTURE_TEST_CASE( receiving_multiple_data_large, large_buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };
        static const std::uint8_t pattern_b[] = { 'f', 'g', 'h', 'i', 'j' };

        receive_pdu( std::begin( pattern_a ), std::end( pattern_a ), false, true );
        receive_pdu( std::begin( pattern_b ), std::end( pattern_b ), false, true );

        auto received = next_received();
        BOOST_REQUIRE( received.size );
    }

    BOOST_FIXTURE_TEST_CASE( acknowlage_send_data, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };

        transmit_pdu( std::begin( pattern_a ), std::end( pattern_a ) );
        BOOST_CHECK_EQUAL( next_transmit().size, 5u + 2u + 2u );
        receive_pdu( std::begin( pattern_a ), std::end( pattern_a ), false, true );
        BOOST_CHECK_EQUAL( next_transmit().size, 2u + 2u );
    }

    BOOST_FIXTURE_TEST_CASE( not_acknowlage_send_data, buffer_under_test )
    {
        static const std::uint8_t pattern_a[] = { 'a', 'b', 'c', 'd', 'e' };

        transmit_pdu( std::begin( pattern_a ), std::end( pattern_a ) );
        BOOST_CHECK_EQUAL( next_transmit().size, 5u + 2u + 2u );
        receive_pdu( std::begin( pattern_a ), std::end( pattern_a ), false, false );
        BOOST_CHECK_EQUAL( next_transmit().size, 5u + 2u + 2u );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( packet_counter_tests )

    BOOST_FIXTURE_TEST_CASE( do_not_increment_when_receiving_empty_pdu, running_mode )
    {
        receive_pdu( {}, false, false );
        receive_pdu( {}, true, true );
        BOOST_CHECK_EQUAL( receive_packet_counter(), 0 );
    }

    BOOST_FIXTURE_TEST_CASE( increment_when_receiving_none_empty_pdu, running_mode )
    {
        receive_pdu( { 0x11 }, false, false );
        BOOST_CHECK_EQUAL( receive_packet_counter(), 1 );
    }

    BOOST_FIXTURE_TEST_CASE( do_not_increment_when_receiving_unexpected_pdu, running_mode )
    {
        receive_pdu( { 0x11 }, false, false );
        receive_pdu( { 0x11 }, false, true );
        BOOST_CHECK_EQUAL( receive_packet_counter(), 1 );
    }

    BOOST_FIXTURE_TEST_CASE( multiple_receive_increments, running_mode )
    {
        receive_pdu( { 0x11 }, false, false ); // increment
        receive_pdu( { 0x11 }, true, false );  // increment
        receive_pdu( {}, false, false );       // not incremented because it's empty
        receive_pdu( { 0x11 }, true, false );  // increment
        receive_pdu( { 0x11 }, true, false );  // not incremented because it's resend
        BOOST_CHECK_EQUAL( receive_packet_counter(), 3 );
    }

    BOOST_FIXTURE_TEST_CASE( do_not_increment_when_sending_empty_pdu, running_mode )
    {
        const auto empty1 = next_transmit();
        static_cast< void >( empty1 );

        // incomming PDU acknowledges
        auto incomming = allocate_receive_buffer();
        incomming.buffer[ 0 ] = 1 | 4;
        incomming.buffer[ 1 ] = 0;
        received( incomming );

        BOOST_CHECK_EQUAL( transmit_packet_counter(), 0 );
    }

    BOOST_FIXTURE_TEST_CASE( do_increment_when_send_pdu_was_acknowlaged, running_mode )
    {
        transmit_pdu( { 1 } );

        BOOST_CHECK_EQUAL( transmit_packet_counter(), 0 );

        // incomming PDU acknowledges
        auto incomming = allocate_receive_buffer();
        incomming.buffer[ 0 ] = 1 | 4;
        incomming.buffer[ 1 ] = 0;
        received( incomming );

        BOOST_CHECK_EQUAL( transmit_packet_counter(), 1 );
    }

    BOOST_FIXTURE_TEST_CASE( do_increment_when_resending_pdu, running_mode )
    {
        transmit_pdu( { 1 } );

        BOOST_CHECK_EQUAL( transmit_packet_counter(), 0 );

        // incomming PDU acknowledges
        auto incomming = allocate_receive_buffer();
        incomming.buffer[ 0 ] = 1;
        incomming.buffer[ 1 ] = 0;
        received( incomming );

        transmit_pdu( { 1 } );

        BOOST_CHECK_EQUAL( transmit_packet_counter(), 0 );
    }

BOOST_AUTO_TEST_SUITE_END()
