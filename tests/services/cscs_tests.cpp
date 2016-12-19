#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/services/csc.hpp>

#include "../test_servers.hpp"


class data_handler
{
public:
    void next_time( std::uint16_t time, std::uint32_t wheel, std::uint16_t crank )
    {
        time_  = time;
        wheel_ = wheel;
        crank_ = crank;
    }

    std::uint32_t cumulative_wheel_revolutions() const
    {
        return wheel_;
    }

    /*
     * cycling_speed_and_cadence_handler_prototype implementation
     */
    std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions_and_time()
    {
        return std::pair< std::uint32_t, std::uint16_t >( wheel_, time_ );
    }

    std::pair< std::uint16_t, std::uint16_t > cumulative_crank_revolutions_and_time()
    {
        return std::pair< std::uint16_t, std::uint16_t >( crank_, time_ );
    }

    void set_cumulative_wheel_revolutions( std::uint32_t new_value )
    {
        wheel_ = new_value;
    }


private:
    std::uint16_t time_;
    std::uint32_t wheel_;
    std::uint16_t crank_;
};

typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<
        bluetoe::sensor_location::top_of_shoe,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::crank_revolution_data_supported,
        bluetoe::csc::handler< data_handler >
    >
> csc_server;

template < typename SensorPosition >
using csc_server_with_sensorposition = bluetoe::server<
    bluetoe::cycling_speed_and_cadence<
        SensorPosition,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::crank_revolution_data_supported,
        bluetoe::csc::handler< data_handler >
    >
>;

typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<
        bluetoe::sensor_location::top_of_shoe,
        bluetoe::sensor_location::in_shoe,
        bluetoe::sensor_location::hip,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::crank_revolution_data_supported,
        bluetoe::csc::handler< data_handler >
    >
> csc_server_with_multiple_sensor_locations;

typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<
        // two sensor locations, to force the existence of the control point
        bluetoe::sensor_location::top_of_shoe,
        bluetoe::sensor_location::left_crank,
        bluetoe::csc::crank_revolution_data_supported,
        bluetoe::csc::handler< data_handler >
    >
> csc_crank_only_server;

typedef bluetoe::server<
    bluetoe::cycling_speed_and_cadence<
        bluetoe::sensor_location::top_of_shoe,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::handler< data_handler >
    >
> csc_wheel_only_server;

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

struct service_discover_base {
    std::uint16_t service_starting_handle;
    std::uint16_t service_ending_handle;
};

template < class Server >
struct discover_primary_service : test::request_with_reponse< Server, 100u >, service_discover_base
{
    discover_primary_service()
    {
        // TheLowerTestersendsanATT_Find_By_Type_Value_Request(0x0001, 0xFFFF)
        // to the IUT, with type set to «Primary Service» and Value set to «Cycling Speed and Cadence Service».
        this->l2cap_input( {
            0x06, 0x01, 0x00, 0xff, 0xff, 0x00, 0x28, 0x16, 0x18
        });

        BOOST_REQUIRE_EQUAL( this->response_size, 5u );

        service_starting_handle = this->response[ 1 ] | ( this->response[ 2 ] << 8 );
        service_ending_handle   = this->response[ 3 ] | ( this->response[ 4 ] << 8 );
    }
};

BOOST_AUTO_TEST_SUITE( service_definition )

    /*
     * TP/SD/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( service_definition_over_le_as_primary_service, discover_primary_service< csc_server > )
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
            bluetoe::is_secondary_service,
            bluetoe::csc::handler< data_handler >
        >
    > csc_secondary_server;

    struct discover_secondary_service : test::request_with_reponse< csc_secondary_server, 100u >, service_discover_base
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

template < class Server >
struct discover_all_characteristics : discover_primary_service< Server >
{
    discover_all_characteristics()
    {
        // read by type request
        this->l2cap_input( {
            0x08,
            low( this->service_starting_handle ), high( this->service_starting_handle ),
            low( this->service_ending_handle ), high( this->service_ending_handle ),
            0x03, 0x28 // <<characteristic>>
        });

        BOOST_REQUIRE_EQUAL( this->response_size, 2 + 4 * 7 );
        BOOST_REQUIRE_EQUAL( this->response[ 0 ], 0x09 ); // response opcode
        BOOST_REQUIRE_EQUAL( this->response[ 1 ], 7 );    // handle + value length

        csc_measurement  = parse_characteristic_declaration( &this->response[ 2 + 0 * 7 ] );
        csc_feature      = parse_characteristic_declaration( &this->response[ 2 + 1 * 7 ] );
        sensor_location  = parse_characteristic_declaration( &this->response[ 2 + 2 * 7 ] );
        cs_control_point = parse_characteristic_declaration( &this->response[ 2 + 3 * 7 ] );
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
    BOOST_FIXTURE_TEST_CASE( csc_measurement_test, discover_all_characteristics< csc_server > )
    {
        BOOST_CHECK_EQUAL( csc_measurement.properties, 0x10 );
        BOOST_CHECK_EQUAL( csc_measurement.uuid, 0x2A5B );
    }

    /*
     * TP/DEC/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_feature_test, discover_all_characteristics< csc_server > )
    {
        BOOST_CHECK_EQUAL( csc_feature.properties, 0x02 );
        BOOST_CHECK_EQUAL( csc_feature.uuid, 0x2A5C );
    }

    /*
     * TP/DEC/BV-03-C
     */
    BOOST_FIXTURE_TEST_CASE( sensor_location_test, discover_all_characteristics< csc_server > )
    {
        BOOST_CHECK_EQUAL( sensor_location.properties, 0x02 );
        BOOST_CHECK_EQUAL( sensor_location.uuid, 0x2A5D );
    }

    /*
     * TP/DEC/BV-04-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point_test, discover_all_characteristics< csc_server > )
    {
        BOOST_CHECK_EQUAL( cs_control_point.properties, 0x28 );
        BOOST_CHECK_EQUAL( cs_control_point.uuid, 0x2A55 );
    }

BOOST_AUTO_TEST_SUITE_END()

template < class Server >
struct discover_all_descriptors : discover_all_characteristics< Server >
{

    discover_all_descriptors()
    {
        csc_measurement_client_configuration = find_client_characteristic_configuration(
            this->csc_measurement.value_attribute_handle + 1, this->csc_feature.handle - 1);
        sc_control_point_client_configuration = find_client_characteristic_configuration(
            this->cs_control_point.value_attribute_handle + 1, this->service_ending_handle );
    }

    std::vector< handle_uuid_pair > find_information_request( std::uint16_t start_handle, std::uint16_t end_handle )
    {
        this->l2cap_input({
            // Find information request
            0x04,
            low( start_handle ),
            high( start_handle ),
            low( end_handle ),
            high( end_handle )
        });

        BOOST_REQUIRE_EQUAL( this->response[ 0 ], 0x05 ); // response opcode
        BOOST_REQUIRE_EQUAL( this->response[ 1 ], 0x01 ); // Format
        BOOST_REQUIRE_EQUAL( ( this->response_size - 2 ) % 4, 0 );

        std::vector< handle_uuid_pair > result;
        for ( const std::uint8_t* p = &this->response[ 2 ]; p != this->begin() + this->response_size; p += 4 )
        {
            result.push_back( handle_uuid_pair{ uint16( p ), uint16( p + 2 ) } );
        }

        return result;
    }

    handle_uuid_pair find_client_characteristic_configuration( std::uint16_t start_handle, std::uint16_t end_handle )
    {
        const auto descriptors = this->find_information_request( start_handle, end_handle );

        const auto pos = std::find_if( descriptors.begin(), descriptors.end(),
            []( handle_uuid_pair b ) -> bool {
                return b.uuid == 0x2902;
            });

        BOOST_REQUIRE( pos != descriptors.end() );
        return *pos;
    }

    std::vector< std::uint8_t > att_read( std::uint16_t handle )
    {
        this->l2cap_input({
            0x0a, low( handle ), high( handle )
        });

        BOOST_REQUIRE_EQUAL( this->response[ 0 ], 0x0b ); // response opcode
        BOOST_REQUIRE_GT( this->response_size, 1 );

        return std::vector< std::uint8_t >( this->begin() + 1, this->begin() + this->response_size );
    }

    handle_uuid_pair csc_measurement_client_configuration;
    handle_uuid_pair sc_control_point_client_configuration;
};

BOOST_AUTO_TEST_SUITE( characteristic_descriptors_tests )

    /*
     * TP/DES/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measurement_client_characteristic_configuration_descriptor, discover_all_descriptors< csc_server > )
    {
        const auto value = att_read( csc_measurement_client_configuration.handle );

        BOOST_CHECK_EQUAL( value.size(), 2 );
        const std::uint16_t config_value = uint16( &value[ 0 ] );

        BOOST_CHECK( config_value == 0x0000 || config_value == 0x0001 );
    }

    /*
     * TP/DES/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point_client_characteristic_configuration_descriptor, discover_all_descriptors< csc_server > )
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
    BOOST_FIXTURE_TEST_CASE( csc_feature_test, discover_all_descriptors< csc_server > )
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

    /*
     * TP/CR/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( sensor_location_test, discover_all_descriptors< csc_server > )
    {
        l2cap_input({
            0x0A, low( sensor_location.value_attribute_handle ), high( sensor_location.value_attribute_handle )
        });

        BOOST_CHECK_EQUAL( response[ 0 ], 0x0b ); // response opcode

        // 1 octet with value other than RFU range.
        BOOST_CHECK_EQUAL( response_size, 2 );
        BOOST_CHECK_LT( response[ 1 ], 15 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( configure_indication_and_notification )

    template < typename Fixture >
    void reset_handle( Fixture& fixture, uint16_t handle )
    {
        fixture.l2cap_input({
            0x12,
            low( handle ), high( handle ),
            0x00, 0x00
        });

        BOOST_CHECK_EQUAL( fixture.response[ 0 ], 0x13 ); // response opcode
    }

    /*
     * TP/CON/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measurement_test, discover_all_descriptors< csc_server > )
    {
        reset_handle( *this, csc_measurement_client_configuration.handle );

        l2cap_input({
            0x12,
            low( csc_measurement_client_configuration.handle ), high( csc_measurement_client_configuration.handle ),
            0x01, 0x00
        });

        expected_result({ 0x13 });

        // The Lower Tester reads the value of the client characteristic configuration descriptor.
        l2cap_input({
            0x0a,
            low( csc_measurement_client_configuration.handle ), high( csc_measurement_client_configuration.handle ),
        });

        expected_result({
            0x0b, 0x01, 0x00
        });
    }

    /*
     * TP/CON/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( sc_control_point_test, discover_all_descriptors< csc_server > )
    {
        reset_handle( *this, sc_control_point_client_configuration.handle );

        l2cap_input({
            0x12,
            low( sc_control_point_client_configuration.handle ), high( sc_control_point_client_configuration.handle ),
            0x02, 0x00
        });

        expected_result({ 0x13 });

        // The Lower Tester reads the value of the client characteristic configuration descriptor.
        l2cap_input({
            0x0a,
            low( sc_control_point_client_configuration.handle ), high( sc_control_point_client_configuration.handle ),
        });

        expected_result({
            0x0b, 0x02, 0x00
        });
    }

BOOST_AUTO_TEST_SUITE_END()

template < class Server >
struct discover_and_configure_all_descriptor : discover_all_descriptors< Server >
{
    discover_and_configure_all_descriptor()
    {
        this->l2cap_input({
            0x12,
            low( this->csc_measurement_client_configuration.handle ),
            high( this->csc_measurement_client_configuration.handle ),
            0x01, 0x00
        });
        this->expected_result({ 0x13 });

        this->l2cap_input({
            0x12,
            low( this->sc_control_point_client_configuration.handle ),
            high( this->sc_control_point_client_configuration.handle ),
            0x02, 0x00
        });
        this->expected_result({ 0x13 });
    }
};

BOOST_AUTO_TEST_SUITE( characteristic_notification )

    /*
     * TP/CN/BV-01-C
     * TP/CN/BV-02-C
     * TP/CN/BV-03-C
     * TP/CN/BV-04-C
     */
    BOOST_FIXTURE_TEST_CASE( csc_measurement_notifications__wheel_revolution_data, discover_and_configure_all_descriptor< csc_server > )
    {
        // update values
        next_time( 0x1234, 0x23456789, 0x3456 );
        notify_timed_update( *static_cast< csc_server* >( this ) );

        // notification?
        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification_type, csc_server::notification );

        // lets generate a notification pdu
        expected_output( notification, {
            0x1b, 0x03, 0x00,                   // indication < handle >
            0x03,                               // flags
            0x89, 0x67, 0x45, 0x23, 0x34, 0x12, // wheel and time
            0x56, 0x34, 0x34, 0x12              // crank and time
        });
    }

    BOOST_FIXTURE_TEST_CASE( only_crank_mesurement, discover_and_configure_all_descriptor< csc_crank_only_server > )
    {
        // update values
        next_time( 0x1234, 0x23456789, 0x3456 );
        notify_timed_update( *static_cast< csc_crank_only_server* >( this ) );

        // notification?
        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification_type, csc_server::notification );

        // lets generate a notification pdu
        expected_output( notification, {
            0x1b, 0x03, 0x00,                   // indication < handle >
            0x02,                               // flags
            0x56, 0x34, 0x34, 0x12              // crank and time
        });
    }

    BOOST_FIXTURE_TEST_CASE( only_wheel_mesurement, discover_and_configure_all_descriptor< csc_wheel_only_server > )
    {
        // update values
        next_time( 0x1234, 0x23456789, 0x3456 );
        notify_timed_update( *this );

        // notification?
        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification_type, csc_server::notification );

        // lets generate a notification pdu
        expected_output( notification, {
            0x1b, 0x03, 0x00,                   // indication < handle >
            0x01,                               // flags
            0x89, 0x67, 0x45, 0x23, 0x34, 0x12  // wheel and time
        });
    }


BOOST_AUTO_TEST_SUITE_END()

template < class Server >
void check_cp_response( Server& server, std::initializer_list< std::uint8_t > response )
{
    BOOST_REQUIRE( server.notification.valid() );
    BOOST_CHECK_EQUAL( server.notification_type, csc_server::indication );

    std::vector< std::uint8_t > expected = {
        0x1d,
        low( server.cs_control_point.value_attribute_handle ),
        high( server.cs_control_point.value_attribute_handle ) };

    expected.insert( expected.end(), response );

    server.expected_output( server.notification, expected.begin(), expected.end(), server.connection );
}

BOOST_AUTO_TEST_SUITE( service_procedures )

    /*
     * TP/SPS/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( set_cumulative_value__set_to_zero, discover_and_configure_all_descriptor< csc_server > )
    {
        // update values
        next_time( 0x1234, 0x23456789, 0x3456 );
        notify_timed_update( *this );

        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x00, 0x00, 0x00, 0x00   // 32 bit wheel value
        });
        expected_result({ 0x13 });

        // check that callback was called
        BOOST_CHECK_EQUAL( cumulative_wheel_revolutions(), 0 );

        // trigger indication
        confirm_cumulative_wheel_revolutions( *this );

        check_cp_response( *this, {
            0x10,  // response opcode
            0x01,  // resquest opcode (Set Cumulative Value)
            0x01   // success
        });
    }

    /*
     * TP/SPS/BV-02-C
     */
    BOOST_FIXTURE_TEST_CASE( set_cumulative_value__set_to_non_zero, discover_and_configure_all_descriptor< csc_server > )
    {
        // update values
        next_time( 0x1234, 0x23456789, 0x3456 );
        notify_timed_update( *this );

        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x01, 0x20, 0x30, 0x04   // 32 bit wheel value
        });
        expected_result({ 0x13 });

        // check that callback was called
        BOOST_CHECK_EQUAL( cumulative_wheel_revolutions(), 0x04302001 );

        // trigger indication
        confirm_cumulative_wheel_revolutions( *this );

        check_cp_response( *this, {
            0x10,  // response opcode
            0x01,  // resquest opcode (Set Cumulative Value)
            0x01   // success
        });
    }

    BOOST_FIXTURE_TEST_CASE( set_cumulative_value__invalid_paramter__to_large, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        check_error_response(
            {
                0x12,
                low( cs_control_point.value_attribute_handle ),
                high( cs_control_point.value_attribute_handle ),
                0x01,                    // resquest opcode (Set Cumulative Value)
                0x01, 0x20, 0x30, 0x04, 0x05
            },
            0x12, cs_control_point.value_attribute_handle, 0x04 // invalid PDU
        );
    }


    BOOST_FIXTURE_TEST_CASE( set_cumulative_value__invalid_paramter__to_small, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        check_error_response(
            {
                0x12,
                low( cs_control_point.value_attribute_handle ),
                high( cs_control_point.value_attribute_handle ),
                0x01,                    // resquest opcode (Set Cumulative Value)
                0x01, 0x20, 0x30
            },
            0x12, cs_control_point.value_attribute_handle, 0x04 // invalid PDU
        );
    }

    /*
     * TP/SPL/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( request_supported_sensor_locations, discover_and_configure_all_descriptor< csc_server_with_multiple_sensor_locations > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x04,     // resquest opcode (Request Supported Sensor Locations)
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x04,  // resquest opcode (Request Supported Sensor Locations)
            0x01,  // success
            0x01, 0x02, 0x03 // top_of_shoe, in_shoe, hip
        });
    }

    BOOST_FIXTURE_TEST_CASE( request_supported_sensor_locations__no_multiple_sensor_locations, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x04,     // resquest opcode (Request Supported Sensor Locations)
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x04,  // resquest opcode (Request Supported Sensor Locations)
            0x02   // Op Code not supported
        });
    }

    /*
     * TP/SPU/BV-01-C
     */
    BOOST_FIXTURE_TEST_CASE( update_sensor_location__multiple_sensor_locations, discover_and_configure_all_descriptor< csc_server_with_multiple_sensor_locations > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x03,     // resquest opcode (Update Sensor Locations)
            0x03      // hip
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x03,  // resquest opcode (Update Sensor Locations)
            0x01   // success
        });

        // check the position characteristic
        l2cap_input({
            0x0a,
            low( sensor_location.value_attribute_handle ),
            high( sensor_location.value_attribute_handle )
        });
        expected_result({ 0x0b, 0x03 });
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( service_procedures__general_error_handling )

    /*
     * TP/SPE/BI-01-C
     */
    BOOST_FIXTURE_TEST_CASE( op_code_not_supported_1, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x00     // resquest opcode (invalid)
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x00,  // resquest opcode (invalid)
            0x02   // Op Code not supported
        });
    }

    BOOST_FIXTURE_TEST_CASE( op_code_not_supported_2, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x42     // resquest opcode (invalid)
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x42,  // resquest opcode (invalid)
            0x02   // Op Code not supported
        });
    }

    /*
     * TP/SPE/BI-02-C
     */
    BOOST_FIXTURE_TEST_CASE( invalid_parameter, discover_and_configure_all_descriptor< csc_server_with_multiple_sensor_locations > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x03,     // resquest opcode (UpdateSensorLocation)
            0x42      // sensor location from the RFU range
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x03,  // resquest opcode (UpdateSensorLocation)
            0x03   // Invalid Parameter
        });
    }

    /*
     * TP/SPE/BI-03-C
     */
    BOOST_FIXTURE_TEST_CASE( client_characteristic_configuration_descriptor_improperly_configured, discover_all_descriptors< csc_server > )
    {
        // valid command to set the cumulative wheel
        check_error_response({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x00, 0x00, 0x00, 0x00   // 32 bit wheel value
        },
        // according to the TS/spec, 0x81 should be returned. But there is already an error code defined in Supplement to the Bluetooth Core Specification
        0x12, cs_control_point.value_attribute_handle, 0xfd );
    }

    /*
     * TP/SPE/BI-04-C
     *
     * This one is hard to test, because it requires the implementation to be asynchronous somewhere.
     * Currently the only asynchronous procedure is
     */
    BOOST_FIXTURE_TEST_CASE( procedure_already_in_progress, discover_and_configure_all_descriptor< csc_server > )
    {
        // write to control point
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x01, 0x20, 0x30, 0x04   // 32 bit wheel value
        });
        expected_result({ 0x13 });

        // write to control point
        check_error_response({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x01, 0x20, 0x30, 0x04   // 32 bit wheel value
        },
        // according to the TS/spec, 0x80 should be returned. But there is already an error code defined in Supplement to the Bluetooth Core Specification
        0x12, cs_control_point.value_attribute_handle, 0xfe );
    }

    /*
     * TP/SPE/BI-05-C [SC Control Point Procedure Timeout]
     *
     * Not implemented
     */

    BOOST_FIXTURE_TEST_CASE( multiple_control_point_procedures, discover_and_configure_all_descriptor< csc_server_with_multiple_sensor_locations > )
    {
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x01,                    // resquest opcode (Set Cumulative Value)
            0x01, 0x20, 0x30, 0x04   // 32 bit wheel value
        });
        expected_result({ 0x13 });

        // check that callback was called
        BOOST_CHECK_EQUAL( cumulative_wheel_revolutions(), 0x04302001 );

        // trigger indication
        confirm_cumulative_wheel_revolutions( *this );

        check_cp_response( *this, {
            0x10,  // response opcode
            0x01,  // resquest opcode (UpdateSensorLocation)
            0x01   // success
        });

        // set sensor location
        l2cap_input({
            0x12,
            low( cs_control_point.value_attribute_handle ),
            high( cs_control_point.value_attribute_handle ),
            0x03,     // resquest opcode (UpdateSensorLocation)
            0x02      // sensor location (in shoe)
        });
        expected_result({ 0x13 });

        check_cp_response( *this, {
            0x10,  // response opcode
            0x03,  // resquest opcode (UpdateSensorLocation)
            0x01   // success
        });
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( sensor_location )

    BOOST_FIXTURE_TEST_CASE( sensor_position__hip, discover_and_configure_all_descriptor< csc_server_with_sensorposition< bluetoe::sensor_location::hip > > )
    {
        l2cap_input({
            0x0a,
            low( sensor_location.value_attribute_handle ),
            high( sensor_location.value_attribute_handle )
        });
        expected_result({ 0x0b, 0x03 });
    }

    BOOST_FIXTURE_TEST_CASE( sensor_position__rear_dropout, discover_and_configure_all_descriptor< csc_server_with_sensorposition< bluetoe::sensor_location::rear_dropout > > )
    {
        l2cap_input({
            0x0a,
            low( sensor_location.value_attribute_handle ),
            high( sensor_location.value_attribute_handle )
        });
        expected_result({ 0x0b, 0x0a });
    }

BOOST_AUTO_TEST_SUITE_END()
