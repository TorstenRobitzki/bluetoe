#include "buffer_io.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <bluetoe/advertising.hpp>
#include <bluetoe/white_list.hpp>

#include "test_radio.hpp"

template < bool Connect, bool Respond >
struct link_layer_base
{
    link_layer_base()
        : advertisment_scheduled( false )
    {
        std::fill( std::begin( buffer_ ), std::end( buffer_ ), 0 );
    }

    bool is_connection_request_in_filter( const bluetoe::link_layer::device_address& ) const
    {
        return Connect;
    }

    bool is_scan_request_in_filter( const bluetoe::link_layer::device_address& ) const
    {
        return Respond;
    }

    void schedule_advertisment(
        unsigned,
        const bluetoe::write_buffer&,
        const bluetoe::write_buffer&,
        bluetoe::link_layer::delta_time,
        const bluetoe::read_buffer& )
    {
    }

    const bluetoe::link_layer::device_address& local_address() const
    {
        static const bluetoe::link_layer::random_device_address addr( { 0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0 } );

        return addr;
    }

    std::uint8_t* raw()
    {
        return buffer_;
    }

    std::uint8_t buffer_[ 1024 ];
    bool advertisment_scheduled;

    using radio_t = test::radio< 100, 100, link_layer_base< Connect, Respond > >;
};

struct single_advertiser_without_white_list :
    link_layer_base< true, true >,
    bluetoe::link_layer::details::select_advertiser_implementation<
        single_advertiser_without_white_list
    >
{
};

struct multi_advertiser_without_white_list :
    link_layer_base< true, true >,
    bluetoe::link_layer::details::select_advertiser_implementation<
        multi_advertiser_without_white_list,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::connectable_directed_advertising,
        bluetoe::link_layer::scannable_undirected_advertising,
        bluetoe::link_layer::non_connectable_undirected_advertising
    >
{
};

struct single_advertiser_with_white_list :
    link_layer_base< false, false >,
    bluetoe::link_layer::details::select_advertiser_implementation<
        single_advertiser_with_white_list
    >
{
};

struct multi_advertiser_with_white_list :
    link_layer_base< false, false >,
    bluetoe::link_layer::details::select_advertiser_implementation<
        multi_advertiser_with_white_list,
        bluetoe::link_layer::connectable_undirected_advertising,
        bluetoe::link_layer::connectable_directed_advertising,
        bluetoe::link_layer::scannable_undirected_advertising,
        bluetoe::link_layer::non_connectable_undirected_advertising
    >
{
};

typedef boost::mpl::list<
    single_advertiser_without_white_list,
    multi_advertiser_without_white_list,
    single_advertiser_with_white_list,
    multi_advertiser_with_white_list
> all_fixtures;

BOOST_AUTO_TEST_CASE_TEMPLATE( empty_request, Advertiser, all_fixtures )
{
    bluetoe::link_layer::device_address remote_address;
    Advertiser advertiser;
    const bool result = advertiser.handle_adv_receive( bluetoe::read_buffer{ nullptr, 0 }, remote_address );

    BOOST_CHECK( !result );
    BOOST_CHECK_EQUAL( remote_address, bluetoe::link_layer::device_address() );
}

typedef boost::mpl::list<
    single_advertiser_without_white_list,
    multi_advertiser_without_white_list
> all_without_white_list;

bluetoe::read_buffer valid_connection_request()
{
    // the test PDU layout has the header inverted and 2 extra octets between header and body
    static const std::initializer_list< std::uint8_t > data = {
        0xc5 ^ 0xff, 0x22 ^ 0xff,           // header
        0x12, 0x34,                         // extra bytes
        0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x49, // InitA: 49:f0:92:62:1c:3c (random)
        0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
        0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
        0x08, 0x81, 0xf6,                   // CRC Init
        0x03,                               // transmit window size
        0x18, 0x00,                         // window offset
        0x18, 0x00,                         // interval
        0x00, 0x00,                         // slave latency
        0x80, 0x0c,                         // connection timeout
        0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy
    };

    return bluetoe::read_buffer{ const_cast< std::uint8_t* >( data.begin() ), data.size() };
}

static const bluetoe::link_layer::random_device_address remote_address( { 0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x49 } );

BOOST_AUTO_TEST_CASE_TEMPLATE( accept_connection_request_in_white_list, Advertiser, all_without_white_list )
{
    bluetoe::link_layer::device_address remote;
    Advertiser advertiser;

    const bool result = advertiser.handle_adv_receive( valid_connection_request(), remote );

    BOOST_CHECK( result );
    BOOST_CHECK_EQUAL( remote, remote_address );
}

typedef boost::mpl::list<
    single_advertiser_with_white_list,
    multi_advertiser_with_white_list
> all_with_white_list;

BOOST_AUTO_TEST_CASE_TEMPLATE( no_connection_with_white_list, Advertiser, all_with_white_list )
{
    bluetoe::link_layer::device_address remote;
    Advertiser advertiser;

    const bool result = advertiser.handle_adv_receive( valid_connection_request(), remote );

    BOOST_CHECK( !result );
}
