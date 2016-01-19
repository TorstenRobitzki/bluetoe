#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/services/csc.hpp>

#include "../test_servers.hpp"


typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<>
> csc_server;

struct service_discover_base {
    std::uint16_t service_starting_handle;
    std::uint16_t service_ending_handle;

    static std::uint8_t low( std::uint16_t h )
    {
        return h & 0xff;
    }

    static std::uint8_t high( std::uint16_t h )
    {
        return ( h >> 8 ) & 0xff;
    }

    static std::uint16_t uint16( const std::uint8_t* p )
    {
        return *p | ( *( p + 1 ) << 8 );
    }
};

struct discover_primary_service : request_with_reponse< csc_server, 100u >, service_discover_base
{
    discover_primary_service()
    {
        // TheLowerTestersendsanATT_Find_By_Type_Value_Request(0x0001, 0xFFFF)
        // to the IUT, with type set to «Primary Service» and Value set to «Cycling Speed and Cadence Service».
        l2cap_input( {
            0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18
        });

        BOOST_REQUIRE_EQUAL( response_size, 5u );

        service_starting_handle = response[ 1 ] | ( response[ 2 ] << 8 );
        service_ending_handle   = response[ 3 ] | ( response[ 4 ] << 8 );
    }
};

BOOST_AUTO_TEST_SUITE( service_definition )

    /*
     * TP/SD/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( service_definition_over_le_as_primary_service, discover_primary_service )
    {
        BOOST_CHECK_EQUAL( response[ 0 ], 0x07 );
        BOOST_CHECK_LT( service_starting_handle, service_ending_handle );
    }

    typedef bluetoe::server<
        bluetoe::no_gap_service_for_gatt_servers,
        bluetoe::service<
            bluetoe::service_uuid16< 0x1234 >,
            bluetoe::include_service< bluetoe::csc::service_uuid >
        >,
        bluetoe::cycling_speed_and_cadence<
            bluetoe::is_secondary_service
        >
    > csc_secondary_server;

    struct discover_secondary_service : request_with_reponse< csc_secondary_server, 100u >, service_discover_base
    {
        discover_secondary_service()
        {
            // If no instances of Cycling Speed and Cadence Service as a primary service are found, the Lower Tester
            // performs the included services procedure by sending an ATT_Read_By_Type_ Request
            // (0x0001, 0xFFFF) to the IUT, with type set to «Include».
            l2cap_input( {
                0x08, 0x01, 0x00, 0xff, 0xff, 0x02, 0x28
            });

            BOOST_REQUIRE_EQUAL( response_size, 10u );
            BOOST_REQUIRE_EQUAL( response[ 0 ], 0x09 ); // opcode
            BOOST_REQUIRE_EQUAL( response[ 1 ], 0x08 ); // length

            service_starting_handle = response[ 4 ] | ( response[ 5 ] << 8 );
            service_ending_handle   = response[ 6 ] | ( response[ 7 ] << 8 );
        }
    };

    /*
     * TP/SD/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( service_definition_over_le_as_secondary_service, discover_secondary_service )
    {
        BOOST_CHECK_EQUAL( response[ 0 ], 0x09 );
        BOOST_CHECK_LT( service_starting_handle, service_ending_handle );
    }

BOOST_AUTO_TEST_SUITE_END()

struct characteristic_declaration
{
    std::uint16_t   handle;
    std::uint8_t    properties;
    std::uint16_t   value_attribute_handle;
    std::uint16_t   uuid;
};

struct handle_uuid_pair
{
    std::uint16_t handle;
    std::uint16_t uuid;
};

struct discover_all_characteristics : discover_primary_service
{
    discover_all_characteristics()
    {
        // read by type request
        l2cap_input( {
            0x08,
            low( service_starting_handle ), high( service_starting_handle ),
            low( service_ending_handle ), high( service_ending_handle ),
            0x03, 0x28 // <<characteristic>>
        });

        BOOST_REQUIRE_EQUAL( response_size, 2 + 4 * 7 );
        BOOST_REQUIRE_EQUAL( response[ 0 ], 0x09 ); // response opcode
        BOOST_REQUIRE_EQUAL( response[ 1 ], 7 );    // handle + value length

        csc_measurement  = parse_characteristic_declaration( &response[ 2 + 0 * 7 ] );
        csc_feature      = parse_characteristic_declaration( &response[ 2 + 1 * 7 ] );
        sensor_location  = parse_characteristic_declaration( &response[ 2 + 2 * 7 ] );
        cs_control_point = parse_characteristic_declaration( &response[ 2 + 3 * 7 ] );
    }

    static characteristic_declaration parse_characteristic_declaration( const std::uint8_t* pos )
    {
        return characteristic_declaration{ uint16( pos ), *( pos + 2 ), uint16( pos + 3 ), uint16( pos + 5 ) };
    }

    characteristic_declaration csc_measurement;
    characteristic_declaration csc_feature;
    characteristic_declaration sensor_location;
    characteristic_declaration cs_control_point;
};

BOOST_AUTO_TEST_SUITE( characteristic_declaration_tests )


    /*
     * TP/DEC/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measurement_test, discover_all_characteristics )
    {
//        BOOST_CHECK_EQUAL( csc_measurement.properties, 0x10 );
        BOOST_CHECK_EQUAL( csc_measurement.uuid, 0x2A5B );
    }

    /*
     * TP/DEC/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_feature_test, discover_all_characteristics )
    {
        BOOST_CHECK_EQUAL( csc_feature.properties, 0x02 );
        BOOST_CHECK_EQUAL( csc_feature.uuid, 0x2A5C );
    }

    /*
     * TP/DEC/BV-03-C
     */
    BOOST_FIXTURE_TEST_CASE( sensor_location_test, discover_all_characteristics )
    {
        BOOST_CHECK_EQUAL( sensor_location.properties, 0x02 );
        BOOST_CHECK_EQUAL( sensor_location.uuid, 0x2A5D );
    }

    /*
     * TP/DEC/BV-04-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point_test, discover_all_characteristics )
    {
//        BOOST_CHECK_EQUAL( cs_control_point.properties, 0x28 );
        BOOST_CHECK_EQUAL( cs_control_point.uuid, 0x2A55 );
    }

BOOST_AUTO_TEST_SUITE_END()

struct discover_all_descriptors : discover_all_characteristics
{

    discover_all_descriptors()
    {
        csc_measurement_client_configuration = find_client_characteristic_configuration(
            csc_measurement.value_attribute_handle + 1, csc_feature.handle - 1);
        sc_control_point_client_configuration = find_client_characteristic_configuration(
            cs_control_point.value_attribute_handle + 1, service_ending_handle );
    }

    std::vector< handle_uuid_pair > find_information_request( std::uint16_t start_handle, std::uint16_t end_handle )
    {
        l2cap_input({
            // Find information request
            0x04, low( start_handle ), high( start_handle ), low( end_handle ), high( end_handle )
        });

        BOOST_REQUIRE_EQUAL( response[ 0 ], 0x05 ); // response opcode
        BOOST_REQUIRE_EQUAL( response[ 1 ], 0x01 ); // Format
        BOOST_REQUIRE_EQUAL( ( response_size - 2 ) % 4, 0 );

        std::vector< handle_uuid_pair > result;
        for ( const std::uint8_t* p = &response[ 2 ]; p != begin() + response_size; p += 4 )
        {
            result.push_back( handle_uuid_pair{ uint16( p ), uint16( p + 2 ) } );
        }

        return result;
    }

    handle_uuid_pair find_client_characteristic_configuration( std::uint16_t start_handle, std::uint16_t end_handle )
    {
        const auto descriptors = find_information_request( start_handle, end_handle );

        const auto pos = std::find_if( descriptors.begin(), descriptors.end(),
            []( handle_uuid_pair b ) -> bool {
                return b.uuid == 0x2902;
            });

        BOOST_REQUIRE( pos != descriptors.end() );
        return *pos;
    }

    std::vector< std::uint8_t > att_read( std::uint16_t handle )
    {
        l2cap_input({
            0x0a, low( handle ), high( handle )
        });

        BOOST_REQUIRE_EQUAL( response[ 0 ], 0x0b ); // response opcode
        BOOST_REQUIRE_GT( response_size, 1 );

        return std::vector< std::uint8_t >( begin() + 1, begin() + response_size );
    }

    handle_uuid_pair csc_measurement_client_configuration;
    handle_uuid_pair sc_control_point_client_configuration;
};

BOOST_AUTO_TEST_SUITE( characteristic_descriptors_tests )

    /*
     * TP/DES/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measurement_client_characteristic_configuration_descriptor, discover_all_descriptors )
    {
        const auto value = att_read( csc_measurement_client_configuration.handle );

        BOOST_CHECK_EQUAL( value.size(), 2 );
        const std::uint16_t config_value = uint16( &value[ 0 ] );

        BOOST_CHECK( config_value == 0x0000 || config_value == 0x0001 );
    }

    /*
     * TP/DES/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point_client_characteristic_configuration_descriptor, discover_all_descriptors )
    {
        const auto value = att_read( sc_control_point_client_configuration.handle );

        BOOST_CHECK_EQUAL( value.size(), 2 );
        const std::uint16_t config_value = uint16( &value[ 0 ] );

        BOOST_CHECK( config_value == 0x0000 || config_value == 0x0002 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( characteristic_read_value_test_cases )

    /*
     * TP/CR/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_feature_test, discover_all_descriptors )
    {
        l2cap_input({
            0x0A, low( csc_feature.value_attribute_handle ), high( csc_feature.value_attribute_handle )
        });

        BOOST_CHECK_EQUAL( response[ 0 ], 0x0b ); // response opcode

        // 2 octets with RFU bits set to 0.
        BOOST_CHECK_EQUAL( response_size, 3 );
        std::uint16_t value = uint16( &response[ 1 ] );
        BOOST_CHECK_EQUAL( value & 0xFFF8, 0 );
    }

    // BOOST_FIXTURE_TEST_CASE( sensor_location_test, discover_all_descriptors )
    // {
    //     l2cap_input({
    //         0x0A, low( sensor_location.value_attribute_handle ), high( sensor_location.value_attribute_handle )
    //     });

    //     BOOST_CHECK_EQUAL( response[ 0 ], 0x0b ); // response opcode

    //     // 1 octet with value other than RFU range.
    //     BOOST_CHECK_EQUAL( response_size, 2 );
    // }

BOOST_AUTO_TEST_SUITE_END()
