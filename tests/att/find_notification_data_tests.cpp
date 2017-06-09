#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/server.hpp>
#include <bluetoe/attribute.hpp>

/*
 * A notification index is key to a characteristic that is configured to allow notifications or indications.
 * higher_outgoing_priority and lower_outgoing_priority lead to none linear distribution of the notification indexes over
 * all characeteristics. The characteristics are stable sorted by priority and then numerated (started by 0).
 */

const std::uint8_t value_Aa = 0xAa;
const std::uint8_t value_Ab = 0xAb;

const std::uint8_t value_Ba = 0xBa;
const std::uint8_t value_Bb = 0xBb;

using A = bluetoe::service_uuid16< 0x1234 >;

using A_a = bluetoe::characteristic_uuid16< 0xFF00 >;
using A_b = bluetoe::characteristic_uuid16< 0xFF01 >;
using A_c = bluetoe::characteristic_uuid16< 0xFF02 >;

using B = bluetoe::service_uuid16< 0x1235 >;

using B_a = bluetoe::characteristic_uuid16< 0xFF10 >;
using B_b = bluetoe::characteristic_uuid16< 0xFF11 >;
using B_c = bluetoe::characteristic_uuid16< 0xFF12 >;


template < class Server >
std::uint8_t read_value( std::size_t notification_index )
{
    // opcode + handle + 1 octed value (the std::uint8_t's from above)
    static constexpr std::size_t value_notification_pdu_min_size = 1 + 2 + 1;

    typename Server::connection_data data_per_connection( 23 );
    Server server;

    // subscribe the characteristic:
    data_per_connection.client_configurations().flags( notification_index, 1 );

    std::uint8_t out_buffer[ value_notification_pdu_min_size + 100 ];
    std::size_t  out_size = sizeof( out_buffer );

    server.notification_output( out_buffer, out_size, data_per_connection, notification_index );

    BOOST_CHECK_EQUAL( value_notification_pdu_min_size, out_size );

    return out_buffer[ value_notification_pdu_min_size - 1 ];
}

BOOST_AUTO_TEST_CASE( all_default_prio )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            bluetoe::characteristic<
                A_a,
                bluetoe::bind_characteristic_value< const std::uint8_t, &value_Aa >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                A_b,
                bluetoe::bind_characteristic_value< const std::uint8_t, &value_Ab >,
                bluetoe::notify
            >
        >,
        bluetoe::service<
            B,
            bluetoe::characteristic<
                B_a,
                bluetoe::bind_characteristic_value< const std::uint8_t, &value_Ba >,
                bluetoe::notify
            >,
            bluetoe::characteristic<
                B_b,
                bluetoe::bind_characteristic_value< const std::uint8_t, &value_Bb >,
                bluetoe::notify
            >
        >
    >;

    BOOST_CHECK_EQUAL( read_value< server >( 0 ), 0xAa );
    BOOST_CHECK_EQUAL( read_value< server >( 1 ), 0xAb );
    BOOST_CHECK_EQUAL( read_value< server >( 2 ), 0xBa );
    BOOST_CHECK_EQUAL( read_value< server >( 3 ), 0xBb );
}

BOOST_AUTO_TEST_CASE( service_with_higher_priority )
{
}