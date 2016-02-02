#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/adv_service_list.hpp>
#include "test_servers.hpp"

template < class Server >
void expected_advertising( std::initializer_list< std::uint8_t > expected, const Server& server, std::size_t buffer_size = 31 )
{
    std::vector< std::uint8_t > buffer( buffer_size );
    const std::size_t size = server.advertising_data( &*buffer.begin(), buffer.size() );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_SUITE( server_name )

static constexpr char name[] = "Test Name";

typedef bluetoe::extend_server<
    small_temperature_service,
    bluetoe::server_name< name >
> named_temperature_service;

BOOST_FIXTURE_TEST_CASE( named_server, named_temperature_service )
{
    expected_advertising({
        0x02, 0x01, 0x06,
        0x0A, 0x09, 'T', 'e', 's', 't', ' ', 'N', 'a', 'm', 'e',
        0x00, 0x00
    }, *this );
}

BOOST_FIXTURE_TEST_CASE( short_named_server, named_temperature_service )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x06, 0x08, 'T', 'e', 's', 't', ' '
    }, *this, 10 );
}

BOOST_FIXTURE_TEST_CASE( unnamed_server, small_temperature_service )
{
    expected_advertising( {
        0x02, 0x01, 0x06, 0x00, 0x00
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( service_list )

using no_uuids_server = bluetoe::extend_server<
    small_temperature_service,
    bluetoe::no_list_of_service_uuids
>;

BOOST_FIXTURE_TEST_CASE( no_uuid_list, no_uuids_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06, 0x00, 0x00
    }, *this );
}

using one_uuid_server = bluetoe::extend_server<
    small_temperature_service,
    bluetoe::list_of_16_bit_service_uuids<
        bluetoe::service_uuid16< 0x1234 >
    >
>;

BOOST_FIXTURE_TEST_CASE( uuid_with_one_element, one_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x34, 0x12,
        0x00, 0x00
    }, *this );
}

using two_uuid_server = bluetoe::extend_server<
    small_temperature_service,
    bluetoe::list_of_16_bit_service_uuids<
        bluetoe::service_uuid16< 0x1234 >,
        bluetoe::service_uuid16< 0xabcd >
    >
>;

BOOST_FIXTURE_TEST_CASE( uuid_with_two_element, two_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x05, 0x03, 0x34, 0x12, 0xcd, 0xab,
        0x00, 0x00
    }, *this );
}

using no_uuid_server = bluetoe::extend_server<
    small_temperature_service,
    bluetoe::list_of_16_bit_service_uuids<
    >
>;

BOOST_FIXTURE_TEST_CASE( uuid_with_no_element, no_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x00, 0x00
    }, *this );
}

using three_uuid_server = bluetoe::extend_server<
    small_temperature_service,
    bluetoe::list_of_16_bit_service_uuids<
        bluetoe::service_uuid16< 0x1234 >,
        bluetoe::service_uuid16< 0xabcd >,
        bluetoe::service_uuid16< 0x0102 >
    >
>;

BOOST_FIXTURE_TEST_CASE( shortened_list, three_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x05, 0x02, 0x34, 0x12, 0xcd, 0xab
    }, *this, 9 );
}

BOOST_FIXTURE_TEST_CASE( no_space_left_for_a_single_uuid, three_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x00, 0x00
    }, *this, 6 );
}

BOOST_AUTO_TEST_SUITE_END()

