#ifndef BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP
#define BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP

#include <boost/mpl/list.hpp>

#include <initializer_list>
#include <cstdint>
#include <cassert>

#include <bluetoe/link_layer.hpp>

#include "test_radio.hpp"
#include "test_servers.hpp"


static const std::initializer_list< std::uint8_t > valid_connection_request_pdu =
{
    0xc5, 0x22,                         // header
    0x3c, 0x1c, 0x62, 0x92, 0xf0, 0x48, // InitA: 48:f0:92:62:1c:3c (random)
    0x47, 0x11, 0x08, 0x15, 0x0f, 0xc0, // AdvA:  c0:0f:15:08:11:47 (random)
    0x5a, 0xb3, 0x9a, 0xaf,             // Access Address
    0x08, 0x81, 0xf6,                   // CRC Init
    0x03,                               // transmit window size
    0x0b, 0x00,                         // window offset
    0x18, 0x00,                         // interval (30ms)
    0x00, 0x00,                         // peripheral latency
    0x48, 0x00,                         // connection timeout (720ms)
    0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
    0xaa                                // hop increment and sleep clock accuracy (10 and 50ppm)
};

template < typename Server, template < std::size_t, std::size_t, typename > class Radio, typename ... Options >
class unconnected_base_t : public bluetoe::link_layer::link_layer< Server, Radio, Options... >
{
public:
    typedef bluetoe::link_layer::link_layer< Server, Radio, Options... > base;

    unconnected_base_t()
        : sequence_( 0 )
        , next_expected_sequence_( 0 )
    {
    }

    void run( unsigned times = 1 )
    {
        for ( ; times; --times )
            base::run();
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

    void add_connection_update_request(
        std::uint8_t win_size, std::uint16_t win_offset, std::uint16_t interval,
        std::uint16_t latency, std::uint16_t timeout, std::uint16_t instance )
    {
        ll_control_pdu( {
            0x00,                                                   // opcode
            win_size,
            static_cast< std::uint8_t >( win_offset ),  static_cast< std::uint8_t >( win_offset >> 8 ),
            static_cast< std::uint8_t >( interval ),    static_cast< std::uint8_t >( interval >> 8 ),
            static_cast< std::uint8_t >( latency ),     static_cast< std::uint8_t >( latency >> 8 ),
            static_cast< std::uint8_t >( timeout ),     static_cast< std::uint8_t >( timeout >> 8 ),
            static_cast< std::uint8_t >( instance ),    static_cast< std::uint8_t >( instance >> 8 )
        } );

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
            0x00, 0x00,                         // peripheral latency
            0x48, 0x00,                         // connection timeout
            0xff, 0xff, 0xff, 0xff, 0x1f,       // used channel map
            0xaa                                // hop increment and sleep clock accuracy
        };

        this->respond_to( 37, pdu );
    }

    void add_empty_pdus( unsigned count )
    {
        for ( ; count; --count )
            ll_empty_pdu();
    }

    void add_ll_timeouts( unsigned count )
    {
        for ( ; count; --count )
            this->add_connection_event_respond_timeout();
    }

    std::vector< std::uint8_t > run_single_ll_control_pdu( std::initializer_list< std::uint8_t > pdu )
    {
        this->respond_to( 37, valid_connection_request_pdu );
        this->add_connection_event_respond( pdu );
        this->add_connection_event_respond( { 0x01, 0x00 } );

        this->run();

        BOOST_REQUIRE_GE( this->connection_events().size(), 2u );
        auto event = this->connection_events()[ 1 ];

        BOOST_REQUIRE_EQUAL( event.transmitted_data.size(), 1u );

        return event.transmitted_data[ 0 ].data;
    }

    void check_single_ll_control_pdu( std::initializer_list< std::uint8_t > pdu, std::initializer_list< std::uint8_t > expected_response, const char* label )
    {
        auto response = run_single_ll_control_pdu( pdu );
        response[ 0 ] &= 0x03;

        if ( response.size() != expected_response.size() || !std::equal( response.begin(), response.end(), expected_response.begin() ) )
        {
            boost::test_tools::predicate_result result( false );
            result.message() << "\n" << label << ": not expected response: \n";
            result.message() << "PDU:\n" << hex_dump( pdu.begin(), pdu.end() );
            result.message() << "expected:\n" << hex_dump( expected_response.begin(), expected_response.end() );
            result.message() << "found:\n" << hex_dump( response.begin(), response.end() );

            BOOST_CHECK( result );

        }
    }

    void ll_pdu( std::uint8_t llid, std::initializer_list< std::uint8_t > control )
    {
        std::vector< std::uint8_t > pdu = {
            static_cast< std::uint8_t >( llid | sequence_ | next_expected_sequence_ ),
            static_cast< std::uint8_t >( control.size() ) };
        pdu.insert( pdu.end(), control.begin(), control.end() );

        const test::connection_event_response response({ pdu });

        this->add_connection_event_respond( response );
        next_sequences();
    }

    void ll_control_pdu( std::initializer_list< std::uint8_t > control )
    {
        ll_pdu( 0x03, control );
    }

    void ll_function_call( std::function< void() > func )
    {
        std::function< test::pdu_list_t () > callback =
            [=]() -> test::pdu_list_t
            {
                func();
                const test::pdu_t empty{
                    static_cast< std::uint8_t >( 0x01 | sequence_ | next_expected_sequence_ ), 0 };

                return test::pdu_list_t( 1, empty );
            };

        this->add_connection_event_respond(
            test::connection_event_response( callback ) );

        next_sequences();
    }

    void ll_empty_pdu()
    {
        this->add_connection_event_respond( {
            static_cast< std::uint8_t >( 0x01 | sequence_ | next_expected_sequence_ ), 0 } );
        next_sequences();
    }

    void ll_empty_pdus( unsigned count )
    {
        for ( ; count; --count )
            ll_empty_pdu();
    }

    void ll_data_pdu( std::initializer_list< std::uint8_t > control )
    {
        ll_pdu( 0x02, control );
    }
private:
    void next_sequences()
    {
        sequence_ ^= 0x08;
        next_expected_sequence_ ^= 0x04;
    }

    std::uint8_t sequence_;
    std::uint8_t next_expected_sequence_;
};

template < typename ... Options >
using unconnected_base = unconnected_base_t< test::small_temperature_service, test::radio, Options... >;

struct unconnected : unconnected_base< bluetoe::link_layer::buffer_sizes< 61u, 61u > > {};

template < typename ... Options >
struct connecting_base : unconnected_base< Options... >
{
    using base = unconnected_base< Options... >;

    connecting_base()
    {
        this->respond_to( 37, valid_connection_request_pdu );

        base::run();
    }
};

using connecting = connecting_base< bluetoe::link_layer::buffer_sizes< 61u, 61u > >;


struct connected_and_timeout : unconnected
{
    connected_and_timeout()
    {
        this->respond_to( 37, valid_connection_request_pdu );
        this->add_connection_event_respond( { 1, 0 } );
        this->add_connection_event_respond( { 1, 0 } );

        base::run();
    }
};

typedef boost::mpl::list<
    std::integral_constant< unsigned, 37u >,
    std::integral_constant< unsigned, 38u >,
    std::integral_constant< unsigned, 39u > > advertising_channels;

#endif
