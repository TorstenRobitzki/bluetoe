#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <server.hpp>
#include <bluetoe/attribute.hpp>
#include <outgoing_priority.hpp>

/*
 * A notification index is key to a characteristic that is configured to allow notifications or indications.
 * higher_outgoing_priority and lower_outgoing_priority lead to none linear distribution of the notification indexes over
 * all characeteristics. The characteristics are stable sorted by priority and then numerated (started by 0).
 */

const std::uint8_t value_Aa = 0xAa;
const std::uint8_t value_Ab = 0xAb;
const std::uint8_t value_Ac = 0xAc;

const std::uint8_t value_Ba = 0xBa;
const std::uint8_t value_Bb = 0xBb;
const std::uint8_t value_Bc = 0xBc;

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

template < class UUID, const std::uint8_t* Value >
using characteristic =
    bluetoe::characteristic<
        UUID,
        bluetoe::bind_characteristic_value< const std::uint8_t, Value >,
        bluetoe::notify
    >;

template < class UUID, const std::uint8_t* Value >
using characteristic_without_cccd =
    bluetoe::characteristic<
        UUID,
        bluetoe::bind_characteristic_value< const std::uint8_t, Value >
    >;

BOOST_AUTO_TEST_CASE( all_default_prio )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic< B_b, &value_Bb >
        >
    >;

    BOOST_CHECK_EQUAL( read_value< server >( 0 ), 0xAa );
    BOOST_CHECK_EQUAL( read_value< server >( 1 ), 0xAb );
    BOOST_CHECK_EQUAL( read_value< server >( 2 ), 0xBa );
    BOOST_CHECK_EQUAL( read_value< server >( 3 ), 0xBb );
}

/*
     Service:  | A       | B
     ------------------------------
     highest   |         | a, b
     lowest    | a, c    |
*/

BOOST_AUTO_TEST_CASE( service_with_higher_priority )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic< B_b, &value_Bb >
        >,
        bluetoe::higher_outgoing_priority< B >
    >;

    BOOST_CHECK_EQUAL( read_value< server >( 0 ), 0xBa );
    BOOST_CHECK_EQUAL( read_value< server >( 1 ), 0xBb );
    BOOST_CHECK_EQUAL( read_value< server >( 2 ), 0xAa );
    BOOST_CHECK_EQUAL( read_value< server >( 3 ), 0xAb );
}

/*
     Service:  | A       | B
     ------------------------------
     highest   | a       | a
               | b       | b
     lowest    | c       | c
*/
BOOST_AUTO_TEST_CASE( two_services_with_similar_characteristic_priorities )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >,
            characteristic< A_c, &value_Ac >,
            bluetoe::higher_outgoing_priority< A_a, A_b >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic< B_b, &value_Bb >,
            characteristic< B_c, &value_Bc >,
            bluetoe::higher_outgoing_priority< B_a, B_b >
        >
    >;

    BOOST_CHECK_EQUAL( read_value< server >( 0 ), 0xAa );
    BOOST_CHECK_EQUAL( read_value< server >( 1 ), 0xBa );
    BOOST_CHECK_EQUAL( read_value< server >( 2 ), 0xAb );
    BOOST_CHECK_EQUAL( read_value< server >( 3 ), 0xBb );
    BOOST_CHECK_EQUAL( read_value< server >( 4 ), 0xAc );
    BOOST_CHECK_EQUAL( read_value< server >( 5 ), 0xBc );
}

/*
     Service:  | A       | B
     ------------------------------
     highest   |         | c
               |         | a, b
               | c       |
               | b       |
     lowest    | a       |

*/
BOOST_AUTO_TEST_CASE( two_services_with_similar_characteristic_priorities_but_different_service_priority )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >,
            characteristic< A_c, &value_Ac >,
            bluetoe::higher_outgoing_priority< A_c, A_b >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic< B_b, &value_Bb >,
            characteristic< B_c, &value_Bc >,
            bluetoe::higher_outgoing_priority< B_c >
        >,
        bluetoe::higher_outgoing_priority< B >
    >;

    BOOST_CHECK_EQUAL( read_value< server >( 0 ), 0xBc );
    BOOST_CHECK_EQUAL( read_value< server >( 1 ), 0xBa );
    BOOST_CHECK_EQUAL( read_value< server >( 2 ), 0xBb );
    BOOST_CHECK_EQUAL( read_value< server >( 3 ), 0xAc );
    BOOST_CHECK_EQUAL( read_value< server >( 4 ), 0xAb );
    BOOST_CHECK_EQUAL( read_value< server >( 5 ), 0xAa );
}

template < int I >
using int_c = std::integral_constant< std::size_t, I >;

BOOST_AUTO_TEST_CASE( default_cccd_indices )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >,
            characteristic< A_c, &value_Ac >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic< B_b, &value_Bb >,
            characteristic< B_c, &value_Bc >
        >
    >;

    BOOST_CHECK( (
        std::is_same<
            server::cccd_indices,
            std::tuple< int_c< 0 >, int_c< 1 >, int_c< 2 >, int_c< 3 >, int_c< 4 >, int_c< 5 > > >::value
    ) );

}

/*
     Service:  | A       | B
     ------------------------------
     highest   |         | c
               |         | a
               | c       |
               | b       |
     lowest    | a       |

*/
BOOST_AUTO_TEST_CASE( cccd_indices_with_some_priorities )
{
    using server = bluetoe::server<
        bluetoe::service<
            A,
            characteristic< A_a, &value_Aa >,
            characteristic< A_b, &value_Ab >,
            characteristic< A_c, &value_Ac >,
            bluetoe::higher_outgoing_priority< A_c, A_b >
        >,
        bluetoe::service<
            B,
            characteristic< B_a, &value_Ba >,
            characteristic_without_cccd< B_b, &value_Bb >,
            characteristic< B_c, &value_Bc >,
            bluetoe::higher_outgoing_priority< B_c >
        >,
        bluetoe::higher_outgoing_priority< B >
    >;

    BOOST_CHECK( (
        std::is_same<
            server::cccd_indices,
            std::tuple< int_c< 4 >, int_c< 3 >, int_c< 2 >, int_c< 1 >, int_c< 0 > > >::value
    ) );
}
