#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/link_layer/notification_queue.hpp>
#include "test_servers.hpp"

BOOST_AUTO_TEST_SUITE( notifications_by_value )

    std::uint8_t value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value >,
                bluetoe::notify
            >
        >
    > simple_server;

    BOOST_FIXTURE_TEST_CASE( l2cap_layer_gets_notified, test::request_with_reponse< simple_server > )
    {
        notify( value );

        BOOST_CHECK( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 3 );
        BOOST_CHECK_EQUAL( notification_type, simple_server::notification );
    }

    BOOST_FIXTURE_TEST_CASE( no_output_when_notification_not_enabled, test::request_with_reponse< simple_server > )
    {
        expected_output( value, {} );
    }

    BOOST_FIXTURE_TEST_CASE( notification_if_enabled, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        value = 0xab;
        expected_output( value, { 0x1B, 0x03, 0x00, 0xab } );
    }

    std::uint8_t large_buffer[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29
    };

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< decltype( large_buffer ), &large_buffer >,
                bluetoe::notify
            >
        >
    > large_value_notify_server;

    BOOST_FIXTURE_TEST_CASE( notification_data_is_clipped_to_mtu_size_23, test::request_with_reponse< large_value_notify_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        expected_output( large_buffer, {
            0x1B, 0x03, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
            0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19
        } );
    }

    std::uint8_t value_a1 = 1;
    std::uint8_t value_a2 = 2;
    std::uint8_t value_b1 = 3;
    std::uint8_t value_c1 = 4;
    std::uint8_t value_c2 = 5;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_a1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8C >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_a2 >,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8C >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8D >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_b1 >,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8D >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8E >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_c1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8F >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value_c2 >,
                bluetoe::notify
            >
        >
    > server_with_multiple_char;

    BOOST_FIXTURE_TEST_CASE( all_values_notifable, test::request_with_reponse< server_with_multiple_char > )
    {
        notify( value_a1 );
        notify( value_a2 );
        notify( value_b1 );
        notify( value_c1 );
        notify( value_c2 );
    }

    BOOST_FIXTURE_TEST_CASE( notify_c1, test::request_with_reponse< server_with_multiple_char > )
    {
        l2cap_input( { 0x12, 0x0F, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        expected_output( value_c1, { 0x1B, 0x0E, 0x00, 0x04 } );
    }

    BOOST_FIXTURE_TEST_CASE( notify_b1_on_two_connections, test::request_with_reponse< server_with_multiple_char > )
    {
        connection_data con1( 23 ), con2( 23 );

        // enable notification on connection 1
        l2cap_input( { 0x12, 0x0B, 0x00, 0x01, 0x00 }, con1 );
        expected_result( { 0x13 } );

        //  expect output just on connection 1
        expected_output( value_b1, { 0x1B, 0x0A, 0x00, 0x03 }, con1 );
        expected_output( value_b1, {}, con2 );

    }
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( notifications_by_uuid )

    static const std::uint16_t value1 = 0x1111;
    static const std::uint16_t value2 = 0x2222;
    static const std::uint16_t value3 = 0x3333;
    static const std::uint16_t dummy  = 0x4444;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1111 >,
                bluetoe::bind_characteristic_value< decltype( value1 ), &value1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0xffff >,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x2222 >,
                bluetoe::bind_characteristic_value< decltype( value2 ), &value2 >,
                bluetoe::indicate,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8C >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0xffff >,
                bluetoe::bind_characteristic_value< decltype( dummy ), &dummy >
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x3333 >,
                bluetoe::bind_characteristic_value< decltype( value3 ), &value3 >,
                bluetoe::indicate
            >
        >
    > server;

    BOOST_FIXTURE_TEST_CASE( notify_first_16bit_uuid, test::request_with_reponse< server > )
    {
        notify< bluetoe::characteristic_uuid16< 0x1111 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 3 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 0u );
        BOOST_CHECK_EQUAL( notification_type, server::notification );
    }

    BOOST_FIXTURE_TEST_CASE( notify_second_16bit_uuid, test::request_with_reponse< server > )
    {
        notify< bluetoe::characteristic_uuid16< 0x2222 > >();

        BOOST_REQUIRE( notification.valid() );
        BOOST_CHECK_EQUAL( notification.handle(), 8 );
        BOOST_CHECK_EQUAL( notification.client_characteristic_configuration_index(), 1u );
        BOOST_CHECK_EQUAL( notification_type, server::notification );
    }

    // This test should not compile
    /*
    BOOST_FIXTURE_TEST_CASE( notify_third_16bit_uuid, request_with_reponse< server > )
    {
        notify< bluetoe::characteristic_uuid16< 0x3333 > >();
    }
    */
BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( access_client_characteristic_configuration )

    std::uint8_t value1 = 0, value2;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8C >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value2 >,
                bluetoe::notify
            >
        >
    > simple_server;

    BOOST_FIXTURE_TEST_CASE( read_default_values, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x0A, 0x04, 0x00 } );
        expected_result( { 0x0B, 0x00, 0x00 } );

        l2cap_input( { 0x0A, 0x07, 0x00 } );
        expected_result( { 0x0B, 0x00, 0x00 } );
    }

    BOOST_FIXTURE_TEST_CASE( read_blob_with_offset_1, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x0C, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x0D, 0x00 } );
    }

    BOOST_FIXTURE_TEST_CASE( set_and_read, test::request_with_reponse< simple_server > )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        l2cap_input( { 0x12, 0x07, 0x00, 0x00, 0x00 } );
        expected_result( { 0x13 } );

        // read by type
        l2cap_input( { 0x08, 0x01, 0x00, 0xFF, 0xFF, 0x02, 0x29 } );
        expected_result( {
            0x09, 0x04,              // opcode and size
            0x04, 0x00, 0x01, 0x00,  // handle and data
            0x07, 0x00, 0x00, 0x00   // handle and data
        } );
    }

    using server_decl_with_priorities = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8B >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value1 >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x8C8C >,
                bluetoe::bind_characteristic_value< std::uint8_t, &value2 >,
                bluetoe::indicate
            >,

            bluetoe::higher_outgoing_priority< bluetoe::characteristic_uuid16< 0x8C8C > >
        >
    >;

    using server_with_priorities = test::request_with_reponse< server_decl_with_priorities >;

    /*
     * reproducer for #20 Writing to a CCCD leads to wrong CCCD beeing configured (when priorities are used)
     */
    BOOST_FIXTURE_TEST_CASE( configure_and_trigger, server_with_priorities )
    {
        // configure the first characteristics for notifications
        l2cap_input( { 0x12, 0x04, 0x00, 0x01, 0x00 } );
        expected_result( { 0x13 } );

        notify< bluetoe::characteristic_uuid16< 0x8C8B > >();

        BOOST_CHECK_EQUAL(
            connection.client_configurations().flags( notification.client_characteristic_configuration_index() ),
            0x01 );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( checking_proper_configuration )

    struct handler
    {
        std::pair< std::uint8_t, bool > write_1( std::size_t, const std::uint8_t* )
        {
            return { bluetoe::error_codes::success, false };
        }

        std::uint8_t read_1( std::size_t, std::uint8_t*, std::size_t& )
        {
            return bluetoe::error_codes::success;
        }

    };

    using uuid1 = bluetoe::characteristic_uuid16< 0x8C8C >;
    using uuid2 = bluetoe::characteristic_uuid16< 0x8C8D >;

    using server_decl_with_priorities = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid16< 0x8C8B >,
            bluetoe::mixin< handler >,
            bluetoe::characteristic<
                uuid1,
                bluetoe::mixin_write_indication_control_point_handler<
                    handler,
                    &handler::write_1,
                    uuid1
                >,
                bluetoe::mixin_read_handler<
                    handler,
                    &handler::read_1
                >,
                bluetoe::no_read_access,
                bluetoe::indicate
            >,
            bluetoe::characteristic<
                uuid2,
                bluetoe::mixin_write_indication_control_point_handler<
                    handler,
                    &handler::write_1,
                    uuid2
                >,
                bluetoe::mixin_read_handler<
                    handler,
                    &handler::read_1
                >,
                bluetoe::no_read_access,
                bluetoe::indicate
            >,

            bluetoe::higher_outgoing_priority< uuid2 >
        >
    >;

    using server_with_priorities = test::request_with_reponse< server_decl_with_priorities >;

    BOOST_FIXTURE_TEST_CASE( check_unconfigured, server_with_priorities )
    {
        BOOST_CHECK( !configured_for_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_indications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications_or_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications_or_indications< uuid2 >( connection.client_configurations() ) );
    }

    BOOST_FIXTURE_TEST_CASE( configured_1_for_indications, server_with_priorities )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x02, 0x00 } );
        expected_result( { 0x13 } );

        BOOST_CHECK(  configured_for_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_indications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK(  configured_for_notifications_or_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications_or_indications< uuid2 >( connection.client_configurations() ) );
    }

    BOOST_FIXTURE_TEST_CASE( configured_2_for_indications, server_with_priorities )
    {
        l2cap_input( { 0x12, 0x07, 0x00, 0x02, 0x00 } );
        expected_result( { 0x13 } );

        BOOST_CHECK( !configured_for_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK(  configured_for_indications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications_or_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK(  configured_for_notifications_or_indications< uuid2 >( connection.client_configurations() ) );
    }

    BOOST_FIXTURE_TEST_CASE( configured_both_for_indications, server_with_priorities )
    {
        l2cap_input( { 0x12, 0x04, 0x00, 0x02, 0x00 } );
        expected_result( { 0x13 } );

        l2cap_input( { 0x12, 0x07, 0x00, 0x02, 0x00 } );
        expected_result( { 0x13 } );

        BOOST_CHECK( configured_for_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( configured_for_indications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( !configured_for_notifications< uuid2 >( connection.client_configurations() ) );
        BOOST_CHECK( configured_for_notifications_or_indications< uuid1 >( connection.client_configurations() ) );
        BOOST_CHECK( configured_for_notifications_or_indications< uuid2 >( connection.client_configurations() ) );
    }

    BOOST_FIXTURE_TEST_CASE( correct_uuid_beeing_tested_in_control_point_handler, server_with_priorities )
    {
        // configure the second characteristics for indication
        l2cap_input( { 0x12, 0x07, 0x00, 0x02, 0x00 } );
        expected_result( { 0x13 } );

        // write to second control point
        l2cap_input( { 0x12, 0x06, 0x00, 0x42 } );
        expected_result( { 0x13 } );

        // write to first control point
        l2cap_input( { 0x12, 0x03, 0x00, 0x42 } );
        expected_result( { 0x01, 0x12, 0x03, 0x00, 0xfd } );
    }

BOOST_AUTO_TEST_SUITE_END()

/*
 * While working on DTS, there showed up a bug...
 */
BOOST_AUTO_TEST_SUITE( priorites_charactieristics )

    struct dts_service
    {
        dts_service()
            : read_features_called( 0 )
            , read_parameters_called( 0 )
            , read_device_time_called( 0 )
            , write_control_point_called( 0 )
            , read_control_point_called( 0 )
            , read_log_data_called( 0 )
            , write_log_access_control_point_called( 0 )
            , read_log_access_control_point_called( 0 )
        {
        }

        std::uint8_t read_features( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_features_called;

            return bluetoe::error_codes::success;
        }

        std::uint8_t read_parameters( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_parameters_called;
            return bluetoe::error_codes::success;
        }

        std::uint8_t read_device_time( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_device_time_called;
            return bluetoe::error_codes::success;
        }

        std::pair< std::uint8_t, bool > write_control_point( std::size_t, const std::uint8_t* )
        {
            ++write_control_point_called;
            return { bluetoe::error_codes::success, true };
        }

        std::uint8_t read_control_point( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_control_point_called;
            return bluetoe::error_codes::success;
        }

        std::uint8_t read_log_data( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_log_data_called;
            return bluetoe::error_codes::success;
        }

        std::pair< std::uint8_t, bool > write_log_access_control_point( std::size_t, const std::uint8_t* )
        {
            ++write_log_access_control_point_called;
            return { bluetoe::error_codes::success, true };
        }

        std::uint8_t read_log_access_control_point( std::size_t, std::uint8_t*, std::size_t& )
        {
            ++read_log_access_control_point_called;
            return bluetoe::error_codes::success;
        }

        template < typename Server, typename ConData, typename Element >
        void read_output_handler( Server& srv, ConData& connection, Element notification )
        {
            BOOST_REQUIRE( notification.first != bluetoe::link_layer::details::empty );

            std::uint8_t    buffer[ 100 ];
            std::size_t     size = sizeof( buffer );

            if ( notification.first == bluetoe::link_layer::details::notification )
            {
                srv.notification_output( buffer, size, connection, notification.second );
            }
            else
            {
                srv.indication_output( buffer, size, connection, notification.second );
            }
        }

        unsigned read_features_called;
        unsigned read_parameters_called;
        unsigned read_device_time_called;
        unsigned write_control_point_called;
        unsigned read_control_point_called;
        unsigned read_log_data_called;
        unsigned write_log_access_control_point_called;
        unsigned read_log_access_control_point_called;
    };

    using dts_service_uuid                  = bluetoe::service_uuid16< 0xff00 >;

    using device_time_feature_uuid          = bluetoe::characteristic_uuid16< 0x1234 >;
    using device_time_parameters_uuid       = bluetoe::characteristic_uuid16< 0x1235 >;
    using device_time_uuid                  = bluetoe::characteristic_uuid16< 0x1236 >;
    using device_time_control_point_uuid    = bluetoe::characteristic_uuid16< 0x1237 >;
    using time_change_log_data_uuid         = bluetoe::characteristic_uuid16< 0x1238 >;
    using record_access_control_point_uuid  = bluetoe::characteristic_uuid16< 0x1239 >;

    /*
     * Definition of the GATT structure of the IDS server
     */
    template < class ServiceImpl >
    using dts_server_t = bluetoe::server<
        /*
         * One primary service
         */
        bluetoe::service<
            dts_service_uuid,

            /*
             * Mandatory Characteristics
             */
            bluetoe::characteristic<
                device_time_feature_uuid,
                bluetoe::mixin_read_handler< dts_service, &dts_service::read_features >
            >,
            bluetoe::characteristic<
                device_time_parameters_uuid,
                bluetoe::mixin_read_handler< dts_service, &dts_service::read_parameters >
            >,
            bluetoe::characteristic<
                device_time_uuid,
                bluetoe::mixin_read_handler< dts_service, &dts_service::read_device_time >,
                bluetoe::indicate
            >,

            /*
             * Control Point
             */
            bluetoe::characteristic<
                device_time_control_point_uuid,
                bluetoe::mixin_write_indication_control_point_handler<
                    dts_service,
                    &dts_service::write_control_point,
                    device_time_control_point_uuid
                >,
                bluetoe::mixin_read_handler<
                    dts_service,
                    &dts_service::read_control_point
                >,
                bluetoe::no_read_access,
                bluetoe::indicate
            >,

            /*
             * Log
             */
            bluetoe::characteristic<
                time_change_log_data_uuid,
                bluetoe::mixin_read_handler< dts_service, &dts_service::read_log_data >,
                bluetoe::notify,
                bluetoe::no_read_access
            >,
            bluetoe::characteristic<
                record_access_control_point_uuid,
                bluetoe::mixin_write_indication_control_point_handler<
                    dts_service,
                    &dts_service::write_log_access_control_point,
                    record_access_control_point_uuid
                >,
                bluetoe::mixin_read_handler<
                    dts_service,
                    &dts_service::read_log_access_control_point
                >,
                bluetoe::no_read_access,
                bluetoe::indicate
            >,

            /*
             * Mixin a dts_service implementation
             */
            bluetoe::mixin< ServiceImpl >,

            /*
             * Make sure, that log data is transmitted with higher priority
             */
            bluetoe::higher_outgoing_priority< time_change_log_data_uuid >
        >
    >;

    using test_server = test::request_with_reponse< dts_server_t< dts_service > >;

    using notification_queue_t = bluetoe::link_layer::notification_queue<
        typename test_server::notification_priority::template numbers< typename test_server::services >::type,
        typename test_server::connection_data >;

    notification_queue_t notification_queue( 23 );

    static bool lcap_notification_callback( const ::bluetoe::details::notification_data& item, void*, typename dts_server_t< dts_service >::notification_type type )
    {
        switch ( type )
        {
            case dts_server_t< dts_service >::notification:
                return notification_queue.queue_notification( item.client_characteristic_configuration_index() );
                break;
            case dts_server_t< dts_service >::indication:
                return notification_queue.queue_indication( item.client_characteristic_configuration_index() );
                break;
            case dts_server_t< dts_service >::confirmation:
                notification_queue.indication_confirmed();
                return true;
                break;
        }

        return true;
    }

    BOOST_FIXTURE_TEST_CASE( inidicating_control_point_leads_to_reading_control_point, test_server )
    {
        notification_queue = notification_queue_t( 23 );
        notification_callback( lcap_notification_callback, this );

        // connect and subscribe to all characteristics
        connection_data connection( 100 );
        for ( unsigned config = 0; config != connection_data::number_of_characteristics_with_configuration; ++config )
            connection.client_configurations().flags( config, 0x03 );

        // mark the log data characteristic to have data
        notify< time_change_log_data_uuid >();

        // simulate that there is one free output buffer on the link layer.
        read_output_handler( *this, connection, notification_queue.dequeue_indication_or_confirmation() );

        // the ATT layer should shoose the characteristic with the highest output priorit
        BOOST_CHECK_EQUAL( read_log_data_called, 1u );
        BOOST_CHECK_EQUAL( read_device_time_called, 0u );
        BOOST_CHECK_EQUAL( read_control_point_called, 0u );
        BOOST_CHECK_EQUAL( read_log_access_control_point_called, 0u );

        // same with the control point
        indicate< device_time_control_point_uuid >();
        read_output_handler( *this, connection, notification_queue.dequeue_indication_or_confirmation() );
        BOOST_CHECK_EQUAL( read_log_data_called, 1u );
        BOOST_CHECK_EQUAL( read_device_time_called, 0u );
        BOOST_CHECK_EQUAL( read_control_point_called, 1u );
        BOOST_CHECK_EQUAL( read_log_access_control_point_called, 0u );
    }

BOOST_AUTO_TEST_SUITE_END()
