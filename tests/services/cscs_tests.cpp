#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/services/csc.hpp>

#include "../test_servers.hpp"


typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<>
> csc_server;

struct discover_primary_service : request_with_reponse< csc_server, 100u >
{
    discover_primary_service()
    {
        // TheLowerTestersendsanATT_Find_By_Type_Value_Request(0x0001, 0xFFFF)
        // to the IUT, with type set to «Primary Service» and Value set to «Cycling Speed and Cadence Service».
        l2cap_input( {
            0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18
        });

        BOOST_REQUIRE_EQUAL( response_size, 5u );

        starting_handle = response[ 1 ] | ( response[ 2 ] << 8 );
        ending_handle   = response[ 3 ] | ( response[ 4 ] << 8 );
    }

    std::uint16_t starting_handle;
    std::uint16_t ending_handle;

    static std::uint8_t low( std::uint16_t h )
    {
        return h & 0xff;
    }

    static std::uint8_t high( std::uint16_t h )
    {
        return ( h >> 8 ) & 0xff;
    }
};

struct discover_secondary_service {};

BOOST_AUTO_TEST_SUITE( service_definition )

    /*
     * TP/SD/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( service_definition_over_le_as_primary_service, discover_primary_service )
    {
        BOOST_CHECK_EQUAL( response[ 0 ], 0x07 );
        BOOST_CHECK_LT( starting_handle, ending_handle );
    }

    /*
     * TP/SD/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( service_definition_over_le_as_secondary_service, discover_secondary_service )
    {
        /// TODO: Implement when "include" is implemented
    }

BOOST_AUTO_TEST_SUITE_END()

struct discover_all_characteristics : discover_primary_service
{
    discover_all_characteristics()
    {
        // read by type request
        l2cap_input( {
            0x08,
            low( starting_handle ), high( starting_handle ),
            low( ending_handle ), high( ending_handle ),
            0x03, 0x28 // <<characteristic>>
        });

        BOOST_REQUIRE_EQUAL( response_size, 2 + 4 * 7 );
        BOOST_REQUIRE_EQUAL( response[ 0 ], 0x09 ); // response opcode
        BOOST_REQUIRE_EQUAL( response[ 1 ], 7 );    // handle + value length
    }
};

BOOST_AUTO_TEST_SUITE( characteristic_declaration )


    /*
     * TP/DEC/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measuremen, discover_all_characteristics )
    {
        BOOST_CHECK_EQUAL( response[ 2 + 2 + 0 * 7 ], 0x10 );
    }

    /*
     * TP/DEC/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_feature, discover_all_characteristics )
    {
        BOOST_CHECK_EQUAL( response[ 2 + 2 + 1 * 7 ], 0x02 );
    }

    /*
     * TP/DEC/BV-03-C
     */
    BOOST_FIXTURE_TEST_CASE( sensor_location, discover_all_characteristics )
    {
        BOOST_CHECK_EQUAL( response[ 2 + 2 + 2 * 7 ], 0x02 );
    }

    /*
     * TP/DEC/BV-04-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point, discover_all_characteristics )
    {
        /// TODO Fix, when indications are implemeted
        //BOOST_CHECK_EQUAL( response[ 2 + 2 + 3 * 7 ], 0x28 );
    }

BOOST_AUTO_TEST_SUITE_END()

