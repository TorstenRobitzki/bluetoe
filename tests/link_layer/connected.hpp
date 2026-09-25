#ifndef BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP
#define BLUETOE_TESTS_LINK_LAYER_CONNECTED_HPP

#include <boost/mpl/list.hpp>

#include <initializer_list>
#include <cstdint>
#include <cassert>

#include <bluetoe/link_layer.hpp>

#include "ll_pdus.hpp"
#include "simulated_radio.hpp"
#include "test_servers.hpp"

/*
 * The connection the tests run on: the CONNECT_IND with the defaults of test::connection_parameters
 */
static const std::vector< std::uint8_t > valid_connection_request_pdu = test::connect_ind();

template < typename Server, template < typename > class Radio, typename ... Options >
class unconnected_base_t : public bluetoe::link_layer::link_layer< Server, Radio, Options... >
{
    // the link layer is what the l2cap layer requires of it
    static_assert( bluetoe::details::l2cap_link_layer< bluetoe::link_layer::link_layer< Server, Radio, Options... > > );

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

    void check_not_connected() const
    {
        BOOST_CHECK_MESSAGE( this->connection_events().empty(), "connected, but expected not to be" );
    }

    void add_connection_update_request( const test::connection_update& update )
    {
        ll_control_pdu( test::ll_connection_update_ind( update ) );
    }

    void add_connection_update_request(
        std::uint8_t win_size, std::uint16_t win_offset, std::uint16_t interval,
        std::uint16_t latency, std::uint16_t timeout, std::uint16_t instance )
    {
        add_connection_update_request( { win_size, win_offset, interval, latency, timeout, instance } );
    }

    /**
     * @brief the PDU the link layer transmitted `index`th in connection event `event`, its SN
     *        and NESN cleared, so that it compares to a PDU built by name
     */
    std::vector< std::uint8_t > transmitted( std::size_t event, std::size_t index = 0 ) const
    {
        BOOST_REQUIRE_GT( this->connection_events().size(), event );
        BOOST_REQUIRE_GT( this->connection_events()[ event ].transmitted_data.size(), index );

        auto pdu = this->connection_events()[ event ].transmitted_data[ index ].data;
        pdu[ 0 ] &= 0x03;

        return pdu;
    }

    /**
     * @brief requires the PDU transmitted `index`th in connection event `event` to be `expected`
     */
    void check_transmitted( std::size_t event, std::size_t index, const std::vector< std::uint8_t >& expected ) const
    {
        const auto pdu = transmitted( event, index );

        if ( pdu != expected )
        {
            boost::test_tools::predicate_result result( false );
            result.message() << "\nnot the expected PDU " << index << " in connection event " << event << ":\n";
            result.message() << "expected:\n" << hex_dump( expected.begin(), expected.end() );
            result.message() << "found:\n" << hex_dump( pdu.begin(), pdu.end() );

            BOOST_CHECK( result );
        }
    }

    void check_transmitted( std::size_t event, const std::vector< std::uint8_t >& expected ) const
    {
        check_transmitted( event, 0, expected );
    }

    /**
     * @brief requires the connection events to have used `expected` channels, in order
     */
    void check_channels( std::initializer_list< unsigned > expected ) const
    {
        BOOST_REQUIRE_GE( this->connection_events().size(), expected.size() );

        std::size_t index = 0;
        for ( const unsigned channel : expected )
        {
            BOOST_CHECK_EQUAL( this->connection_events()[ index ].channel, channel );
            ++index;
        }
    }

    void respond_with_connection_request( std::uint8_t window_size, std::uint16_t window_offset, std::uint16_t interval )
    {
        this->respond_to( 37, test::connect_ind( {
            .window_size   = window_size,
            .window_offset = window_offset,
            .interval      = interval } ) );
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

    /**
     * @brief connects, receives `pdu` in the first connection event and returns what the
     *        link layer answered in the second, its SN and NESN cleared
     */
    std::vector< std::uint8_t > run_single_ll_control_pdu( const std::vector< std::uint8_t >& pdu )
    {
        this->respond_to( 37, valid_connection_request_pdu );
        this->add_connection_event_respond( pdu );
        this->add_connection_event_respond( test::ll_empty() );

        this->run();

        BOOST_REQUIRE_GE( this->connection_events().size(), 2u );
        auto event = this->connection_events()[ 1 ];

        BOOST_REQUIRE_EQUAL( event.transmitted_data.size(), 1u );

        auto response = event.transmitted_data[ 0 ].data;
        response[ 0 ] &= 0x03;

        return response;
    }

    /**
     * @brief requires the answer to `pdu` to be `expected_response`, both whole data channel PDUs
     */
    void check_single_ll_control_pdu( const std::vector< std::uint8_t >& pdu, const std::vector< std::uint8_t >& expected_response )
    {
        const auto response = run_single_ll_control_pdu( pdu );

        if ( response != expected_response )
        {
            boost::test_tools::predicate_result result( false );
            result.message() << "\nnot the expected response: \n";
            result.message() << "PDU:\n" << hex_dump( pdu.begin(), pdu.end() );
            result.message() << "expected:\n" << hex_dump( expected_response.begin(), expected_response.end() );
            result.message() << "found:\n" << hex_dump( response.begin(), response.end() );

            BOOST_CHECK( result );
        }
    }

    void ll_pdu( std::uint8_t llid, const std::vector< std::uint8_t >& control )
    {
        std::vector< std::uint8_t > pdu = {
            static_cast< std::uint8_t >( llid | sequence_ | next_expected_sequence_ ),
            static_cast< std::uint8_t >( control.size() ) };
        pdu.insert( pdu.end(), control.begin(), control.end() );

        const test::connection_event_response response({ pdu });

        this->add_connection_event_respond( response );
        next_sequences();
    }

    void ll_control_pdu( const std::vector< std::uint8_t >& control )
    {
        ll_pdu( test::llid::control, control );
    }

    void ll_function_call( std::function< void() > func )
    {
        std::function< test::pdu_list_t () > callback =
            [=, this]() -> test::pdu_list_t
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

    void ll_data_pdu( const std::vector< std::uint8_t >& control )
    {
        ll_pdu( test::llid::start, control );
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
