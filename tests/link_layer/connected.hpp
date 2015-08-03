#ifndef BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP
#define BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP

#include <boost/mpl/list.hpp>

#include <initializer_list>
#include <cstdint>
#include <cassert>

#include <bluetoe/link_layer/link_layer.hpp>

#include "test_radio.hpp"
#include "../test_servers.hpp"


static const std::initializer_list< std::uint8_t > valid_connection_request_pdu =
{
    0xc5, 0x22,                         // header
    0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
    0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
    0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
    0x08, 0x81, 0xf6,                   // CRC Init
    0x03,                               // transmit window size
    0x0b, 0x00,                         // window offset
    0x18, 0x00,                         // interval
    0x00, 0x00,                         // slave latency
    0x48, 0x00,                         // connection timeout
    0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
    0xaa                                // hop increment and sleep clock accuracy
};

template < typename ... Options >
struct unconnected_base : bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... >
{
    typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

    void run()
    {
        small_temperature_service gatt_server_;
        base::run( gatt_server_ );
    }

    void check_not_connected( const char* test ) const
    {
        if ( !this->connection_events().empty() )
        {
            boost::test_tools::predicate_result result( false );
            result.message() << "in " << test << " check_not_connected failed.";
            BOOST_CHECK( result );
        }
    }

    void respond_with_connection_request( std::uint8_t window_size, std::uint16_t window_offset, std::uint16_t interval )
    {
        const std::vector< std::uint8_t > pdu =
        {
            0xc5, 0x22,                         // header
            0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
            0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
            0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
            0x08, 0x81, 0xf6,                   // CRC Init
            window_size,                        // transmit window size
            static_cast< std::uint8_t >( window_offset & 0xff ),  // window offset
            static_cast< std::uint8_t >( window_offset >> 8 ),
            static_cast< std::uint8_t >( interval & 0xff ), // interval
            static_cast< std::uint8_t >( interval >> 8 ),
            0x00, 0x00,                         // slave latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        };

        this->respond_to( 37, pdu );
    }

};

struct unconnected : unconnected_base<> {};

template < typename ... Options >
struct connecting_base : unconnected_base< Options... >
{
    typedef bluetoe::link_layer::link_layer< small_temperature_service, test::radio, Options... > base;

    connecting_base()
    {
        this->respond_to( 37, valid_connection_request_pdu );

        small_temperature_service gatt_server_;
        base::run( gatt_server_ );
    }
};

using connecting = connecting_base<>;

struct connected {};

typedef boost::mpl::list<
    std::integral_constant< unsigned, 37u >,
    std::integral_constant< unsigned, 38u >,
    std::integral_constant< unsigned, 39u > > advertising_channels;

#endif