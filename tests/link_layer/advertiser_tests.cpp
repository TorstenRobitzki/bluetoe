#include "buffer_io.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <bluetoe/advertising.hpp>
#include <bluetoe/white_list.hpp>

#include "simulated_radio.hpp"

#include <optional>
#include <vector>

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

    bool l2cap_adverting_data_or_scan_response_data_changed() const
    {
        return false;
    }

    /*
     * What the advertiser asks the radio for is recorded: the channel, and the time for an
     * event that names one.
     */
    struct scheduled_event
    {
        unsigned                                        channel;
        std::optional< bluetoe::link_layer::abs_time >  when;
    };

    void start_advertising_event(
        unsigned channel,
        const bluetoe::link_layer::write_buffer&,
        const bluetoe::link_layer::write_buffer&,
        const bluetoe::link_layer::read_buffer& )
    {
        events.push_back( { channel, std::nullopt } );
    }

    bool schedule_advertising_event(
        unsigned channel,
        bluetoe::link_layer::abs_time when,
        const bluetoe::link_layer::write_buffer&,
        const bluetoe::link_layer::write_buffer&,
        const bluetoe::link_layer::read_buffer& )
    {
        if ( refuse_scheduled_events )
            return false;

        events.push_back( { channel, when } );

        return true;
    }

    void set_access_address_and_crc_init( std::uint32_t, std::uint32_t )
    {
    }

    std::size_t fill_l2cap_advertising_data( std::uint8_t*, std::size_t ) const
    {
        return 0;
    }

    std::size_t fill_l2cap_scan_response_data( std::uint8_t*, std::size_t ) const
    {
        return 0;
    }

    const bluetoe::link_layer::device_address& local_address() const
    {
        static const bluetoe::link_layer::random_device_address addr( { 0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0 } );

        return addr;
    }

    std::uint8_t* raw_pdu_buffer()
    {
        return buffer_;
    }

    auto& link_layer_pdu_buffer()
    {
        return *this;
    }

    std::uint8_t buffer_[ 1024 ];
    bool advertisment_scheduled;

    std::vector< scheduled_event > events;
    bool                           refuse_scheduled_events = false;

    using radio_t = test::radio< link_layer_base< Connect, Respond > >;
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
    const bool result = advertiser.handle_adv_receive( bluetoe::link_layer::abs_time(), bluetoe::link_layer::read_buffer{ nullptr, 0 }, remote_address );

    BOOST_CHECK( !result );
    BOOST_CHECK_EQUAL( remote_address, bluetoe::link_layer::device_address() );
}

typedef boost::mpl::list<
    single_advertiser_without_white_list,
    multi_advertiser_without_white_list
> all_without_white_list;

bluetoe::link_layer::read_buffer valid_connection_request()
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
        0x00, 0x00,                         // peripheral latency
        0x80, 0x0c,                         // connection timeout
        0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
        0xaa                                // hop increment and sleep clock accuracy
    };

    return bluetoe::link_layer::read_buffer{ const_cast< std::uint8_t* >( data.begin() ), data.size() };
}

static const bluetoe::link_layer::random_device_address remote_address( { 0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x49 } );

BOOST_AUTO_TEST_CASE_TEMPLATE( accept_connection_request_in_white_list, Advertiser, all_without_white_list )
{
    bluetoe::link_layer::device_address remote;
    Advertiser advertiser;

    const bool result = advertiser.handle_adv_receive( bluetoe::link_layer::abs_time(), valid_connection_request(), remote );

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

    const bool result = advertiser.handle_adv_receive( bluetoe::link_layer::abs_time(), valid_connection_request(), remote );

    BOOST_CHECK( !result );
}

/*
 * An advertising event puts its PDU on each channel as soon as the radio can; the next event
 * is scheduled one interval and a delay of up to 10 ms after the time the last one was on air.
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( the_next_event_is_scheduled_from_the_time_of_the_last, Advertiser, all_without_white_list )
{
    using bluetoe::link_layer::abs_time;
    using bluetoe::link_layer::delta_time;

    Advertiser advertiser;
    const abs_time on_air( 0x10000 );

    advertiser.handle_start_advertising();
    advertiser.handle_adv_timeout( on_air );
    advertiser.handle_adv_timeout( on_air );
    advertiser.handle_adv_timeout( on_air );

    BOOST_REQUIRE_EQUAL( advertiser.events.size(), 4u );

    // one event: channels 37, 38 and 39, each without naming a time
    BOOST_CHECK_EQUAL( advertiser.events[ 0 ].channel, 37u );
    BOOST_CHECK( !advertiser.events[ 0 ].when );
    BOOST_CHECK_EQUAL( advertiser.events[ 1 ].channel, 38u );
    BOOST_CHECK( !advertiser.events[ 1 ].when );
    BOOST_CHECK_EQUAL( advertiser.events[ 2 ].channel, 39u );
    BOOST_CHECK( !advertiser.events[ 2 ].when );

    // the next event, 100 ms is the default interval
    BOOST_CHECK_EQUAL( advertiser.events[ 3 ].channel, 37u );
    BOOST_REQUIRE( advertiser.events[ 3 ].when );

    const delta_time distance = *advertiser.events[ 3 ].when - on_air;
    BOOST_CHECK( distance >= delta_time::msec( 100 ) );
    BOOST_CHECK( distance <= delta_time::msec( 110 ) );
}

/*
 * A radio that can not meet the time any more is asked to advertise right away instead.
 */
BOOST_AUTO_TEST_CASE_TEMPLATE( an_event_the_radio_refuses_is_started_instead, Advertiser, all_without_white_list )
{
    Advertiser advertiser;
    advertiser.refuse_scheduled_events = true;

    advertiser.handle_start_advertising();
    advertiser.handle_adv_timeout( bluetoe::link_layer::abs_time() );
    advertiser.handle_adv_timeout( bluetoe::link_layer::abs_time() );
    advertiser.handle_adv_timeout( bluetoe::link_layer::abs_time() );

    BOOST_REQUIRE_EQUAL( advertiser.events.size(), 4u );
    BOOST_CHECK_EQUAL( advertiser.events[ 3 ].channel, 37u );
    BOOST_CHECK( !advertiser.events[ 3 ].when );
}
