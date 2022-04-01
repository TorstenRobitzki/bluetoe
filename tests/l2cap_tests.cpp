#include <bluetoe/l2cap.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

class base_data
{
public:
    base_data() : base( 0 ) {}
    int base;
};

class channel_a
{
public:
    static constexpr std::uint16_t channel_id = 42;
    static constexpr std::size_t   minimum_channel_mtu_size = 19;
    static constexpr std::size_t   maximum_channel_mtu_size = 44;

    template < typename ConnectionData >
    void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        BOOST_REQUIRE( in_size >= 1 );
        BOOST_REQUIRE( out_size >= minimum_channel_mtu_size );

        output[ 0 ] = *input;
        output[ 1 ] = 'a';
        out_size = 2;

        connection.a = 'A';
    }

    template < class Next >
    struct private_data : Next {
        private_data() : a( 0 )
        {
        }

        int a;
    };

    template < class PreviousData >
    struct channel_data_t : private_data< PreviousData > {};
};

class channel_b
{
public:
    static constexpr std::uint16_t channel_id = 43;
    static constexpr std::size_t   minimum_channel_mtu_size = 22;
    static constexpr std::size_t   maximum_channel_mtu_size = 30;

    template < typename ConnectionData >
    void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, ConnectionData& connection )
    {
        BOOST_REQUIRE( in_size >= 1 );
        BOOST_REQUIRE( out_size >= minimum_channel_mtu_size );

        output[ 0 ] = *input;
        output[ 1 ] = 'b';
        out_size = 2;

        connection.b = 'B';
    }

    template < class Next >
    struct private_data {
        private_data() : b( 0 )
        {
        }

        int b;
    };

    template < class PreviousData >
    struct channel_data_t : private_data< PreviousData > {};
};

class radio_mock
{

};

class link_layer : public radio_mock, public bluetoe::details::l2cap< link_layer, base_data, channel_a, channel_b >
{
public:
    link_layer()
        : current_buffer_used_( 0 )
    {
    }

    using bluetoe::details::l2cap< link_layer, base_data, channel_a, channel_b >::handle_l2cap_input;

    bool handle_l2cap_input( std::uint16_t channel, std::initializer_list< std::uint8_t > input )
    {
        std::vector< std::uint8_t > pdu = {
            low( input.size() ), high( input.size() ),
            low( channel ), high( channel )
        };

        pdu.insert( pdu.end(), input.begin(), input.end() );

        return bluetoe::details::l2cap< link_layer, base_data, channel_a, channel_b >::handle_l2cap_input(
            pdu.data(), pdu.size(), connection_data_ );
    }

    void add_buffer( std::size_t size )
    {
        // add size of the Basic L2CAP header
        size += 4u;
        BOOST_REQUIRE( current_buffer_used_ <= current_buffer_used_ + size );

        buffers_.push_back( { size, &overall_buffer_[ current_buffer_used_ ] } );
        current_buffer_used_ += size;
    }

    /*
     * Interface to the l2cap
     */
    std::pair< std::size_t, std::uint8_t* > allocate_l2cap_output_buffer( std::size_t size )
    {
        if ( buffers_.empty() )
            return { 0, nullptr };

        const auto buf = buffers_.front();

        if ( buf.first < size )
            return { 0, nullptr };

        buffers_.erase( buffers_.begin() );

        return buf;
    }

    void commit_l2cap_output_buffer( std::pair< std::size_t, std::uint8_t* > buffer )
    {
        output_.push_back( std::vector< std::uint8_t >{ buffer.second, buffer.second + buffer.first } );
    }

    void check_next_output( std::initializer_list< std::uint8_t > expected )
    {
        std::vector< std::uint8_t > next;

        if ( !output_.empty() )
        {
            next = output_.front();
            output_.erase( output_.begin() );
        }

        BOOST_CHECK_EQUAL_COLLECTIONS(
            expected.begin(), expected.end(),
            next.begin(), next.end() );
    }

    connection_data_t connection_data_;

private:
    static std::uint8_t low( std::uint16_t b )
    {
        return b & 0xff;
    }

    static std::uint8_t high( std::uint16_t b )
    {
        return b >> 8;
    }

    std::uint8_t overall_buffer_[ 2048 ];
    std::size_t  current_buffer_used_;

    std::vector< std::pair< std::size_t, std::uint8_t*  > > buffers_;
    std::vector< std::vector< std::uint8_t > > output_;
};

struct link_layer_with_free_buffer : link_layer
{
    link_layer_with_free_buffer()
    {
        add_buffer( 22u );
    }
};

BOOST_FIXTURE_TEST_SUITE( invalid_input, link_layer_with_free_buffer )

BOOST_AUTO_TEST_CASE( invalid_channel )
{
    // invalid channel, id will be ignored
    BOOST_CHECK( handle_l2cap_input( 4711, { 0x01, 0x02 } ) );
    check_next_output( {} );
}

BOOST_AUTO_TEST_CASE( input_smaller_than_header )
{
    const std::initializer_list< std::uint8_t > input = { 0x01, 0x02, 42 };

    BOOST_CHECK( handle_l2cap_input( input.begin(), 3, connection_data_ ) );
    check_next_output( {} );
}

BOOST_AUTO_TEST_CASE( inconsistent_length_to_small )
{
    const std::initializer_list< std::uint8_t > input = { 0x01, 0x00, 42, 0x00 };

    BOOST_CHECK( handle_l2cap_input( input.begin(), 4, connection_data_ ) );
    check_next_output( {} );
}

BOOST_AUTO_TEST_CASE( inconsistent_length_to_large )
{
    const std::initializer_list< std::uint8_t > input = { 0x01, 0x00, 42, 0x00, 0x01, 0x02 };

    BOOST_CHECK( handle_l2cap_input( input.begin(), 6, connection_data_ ) );
    check_next_output( {} );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( test, link_layer )

BOOST_AUTO_TEST_CASE( calculated_min_mtu )
{
    BOOST_TEST( std::size_t{ minimum_mtu_size } == 22u );
}

BOOST_AUTO_TEST_CASE( calculated_max_mtu )
{
    BOOST_TEST( std::size_t{ maximum_mtu_size } == 44u );
}

BOOST_AUTO_TEST_CASE( if_minumum_mtu_size_can_not_be_allocated_pdu_will_not_be_handled )
{
    add_buffer( 21u );
    BOOST_TEST( handle_l2cap_input( 42, { 0x01, 0x02 } ) == false );
}

BOOST_AUTO_TEST_CASE( correct_frameing )
{
    add_buffer( 22u );
    BOOST_TEST( handle_l2cap_input( 42, { 0x01 } ) );

    check_next_output( { 0x02, 0x00, 42, 0x00, 0x01, 'a' } );
}

BOOST_AUTO_TEST_CASE( private_data_modifiable_a )
{
    add_buffer( 22u );
    BOOST_TEST( handle_l2cap_input( 42, { 0x01 } ) );

    BOOST_TEST( connection_data_.a == 'A' );
    BOOST_TEST( connection_data_.b == 0 );
}

BOOST_AUTO_TEST_CASE( private_data_modifiable_b )
{
    add_buffer( 22u );
    BOOST_TEST( handle_l2cap_input( 43, { 0x01 } ) );

    BOOST_TEST( connection_data_.a == 0 );
    BOOST_TEST( connection_data_.b == 'B' );
}

BOOST_AUTO_TEST_SUITE_END()
