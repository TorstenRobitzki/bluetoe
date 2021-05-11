#include <bluetoe/custom_advertising.hpp>
#include <bluetoe/adv_service_list.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

template < class Server >
void expected_advertising( std::initializer_list< std::uint8_t > expected, const Server& server, std::size_t buffer_size = 31 )
{
    std::vector< std::uint8_t > buffer( buffer_size );
    const std::size_t size = server.advertising_data( &*buffer.begin(), buffer.size() );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, expected.begin(), expected.end() );
}

template < class Server >
void expected_scan_response( std::initializer_list< std::uint8_t > expected, const Server& server, std::size_t buffer_size = 31 )
{
    std::vector< std::uint8_t > buffer( buffer_size );
    const std::size_t size = server.scan_response_data( &*buffer.begin(), buffer.size() );

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( buffer ), std::begin( buffer ) + size, expected.begin(), expected.end() );
}

BOOST_AUTO_TEST_SUITE( server_name )

static constexpr char name[] = "Test Name";

typedef bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::no_list_of_service_uuids,
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

typedef bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::no_list_of_service_uuids
> unnamed_temperature_service;

BOOST_FIXTURE_TEST_CASE( unnamed_server, unnamed_temperature_service )
{
    expected_advertising( {
        0x02, 0x01, 0x06, 0x00, 0x00
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( service_list )

using no_uuids_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1212 >
    >,
    bluetoe::no_list_of_service_uuids
>;

BOOST_FIXTURE_TEST_CASE( no_uuid_list, no_uuids_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06, 0x00, 0x00
    }, *this );
}

using one_uuid_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1234 >
    >,
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

using two_uuid_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1234 >
    >,
    bluetoe::service<
        bluetoe::service_uuid16< 0xabcd >
    >,
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

using no_element_server = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1212 >
    >,
    bluetoe::list_of_16_bit_service_uuids<
    >
>;


BOOST_FIXTURE_TEST_CASE( uuid_with_no_element, no_element_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x00, 0x00
    }, *this );
}

using three_uuid_server = bluetoe::extend_server<
    test::small_temperature_service,
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

using server_with_multiple_services = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid16< 0x1122 >
    >
>;

BOOST_FIXTURE_TEST_CASE( implicit_service_list, server_with_multiple_services )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x05, 0x03, 0x22, 0x11, 0x00, 0x18,
        0x00, 0x00
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( service_list_128 )

using no_uuids_server = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::no_list_of_service_uuids
>;

BOOST_FIXTURE_TEST_CASE( no_uuid_list, no_uuids_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06, 0x00, 0x00
    }, *this );
}

using one_uuid_server = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::list_of_128_bit_service_uuids<
        bluetoe::service_uuid< 0x111393DD, 0x01D2, 0x40D6, 0xA0A0, 0xE9B1A56A1191 >
    >
>;

BOOST_FIXTURE_TEST_CASE( uuid_with_one_element, one_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x11, 0x07,
        0x91, 0x11, 0x6A, 0xA5,
        0xB1, 0xE9, 0xA0, 0xA0,
        0xD6, 0x40, 0xD2, 0x01,
        0xDD, 0x93, 0x13, 0x11,
        0x00, 0x00
    }, *this );
}

using two_uuid_server = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::list_of_128_bit_service_uuids<
        bluetoe::service_uuid< 0x111393DD, 0x01D2, 0x40D6, 0xA0A0, 0xE9B1A56A1191 >,
        bluetoe::service_uuid< 0x221393DD, 0x01D2, 0x40D6, 0xA0A0, 0xE9B1A56A1177 >
    >
>;

/*
 * This is more or less a theoretical test, as there will never fit two 16 byte UUIDs into the advertising data
 */
BOOST_FIXTURE_TEST_CASE( uuid_with_two_element, two_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x21, 0x07,
        0x91, 0x11, 0x6A, 0xA5,
        0xB1, 0xE9, 0xA0, 0xA0,
        0xD6, 0x40, 0xD2, 0x01,
        0xDD, 0x93, 0x13, 0x11,
        0x77, 0x11, 0x6A, 0xA5,
        0xB1, 0xE9, 0xA0, 0xA0,
        0xD6, 0x40, 0xD2, 0x01,
        0xDD, 0x93, 0x13, 0x22,
        0x00, 0x00
    }, *this, 100 );
}

using no_uuid_server = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::list_of_128_bit_service_uuids<
    >
>;

BOOST_FIXTURE_TEST_CASE( uuid_with_no_element, no_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x00, 0x00
    }, *this );
}

BOOST_FIXTURE_TEST_CASE( shortened_list, two_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x11, 0x06,
        0x91, 0x11, 0x6A, 0xA5,
        0xB1, 0xE9, 0xA0, 0xA0,
        0xD6, 0x40, 0xD2, 0x01,
        0xDD, 0x93, 0x13, 0x11,
        0x00, 0x00
    }, *this );
}

BOOST_FIXTURE_TEST_CASE( no_space_left_for_a_single_uuid, two_uuid_server )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x00, 0x00
    }, *this, 6 );
}

using server_with_multiple_services = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x111393DD, 0x01D2, 0x40D6, 0xA0A0, 0xE9B1A56A1191 >
    >
>;

BOOST_FIXTURE_TEST_CASE( implicit_service_list, server_with_multiple_services )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0x00, 0x18,
        0x11, 0x07,
        0x91, 0x11, 0x6A, 0xA5,
        0xB1, 0xE9, 0xA0, 0xA0,
        0xD6, 0x40, 0xD2, 0x01,
        0xDD, 0x93, 0x13, 0x11,
        0x00, 0x00
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( slave_connection_interval_range )

using unspecifed_range = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::no_list_of_service_uuids,
    bluetoe::slave_connection_interval_range<>
>;

BOOST_FIXTURE_TEST_CASE( unspecified_range_encoding, unspecifed_range )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x05, 0x12, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00
    }, *this );
}

using specifed_range = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::no_list_of_service_uuids,
    bluetoe::slave_connection_interval_range< 0x0102, 0x203 >
>;

BOOST_FIXTURE_TEST_CASE( specific_range_encoding, specifed_range )
{
    expected_advertising( {
        0x02, 0x01, 0x06,
        0x05, 0x12, 0x02, 0x01, 0x03, 0x02,
        0x00, 0x00
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( slave_connection_interval_range )

static const std::uint8_t custom_advertising_data[] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};

static const std::uint8_t custom_response_data[] = {
    0x11, 0x22, 0x33, 0x44, 0x55
};

using custom_advertising = bluetoe::extend_server<
    test::small_temperature_service,
    bluetoe::custom_advertising_data< sizeof( custom_advertising_data ), custom_advertising_data >,
    bluetoe::custom_scan_response_data< sizeof( custom_response_data ), custom_response_data >
>;

BOOST_FIXTURE_TEST_CASE( custom_advertising_test, custom_advertising )
{
    expected_advertising( {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    }, *this );
}

BOOST_FIXTURE_TEST_CASE( custom_scan_response_test, custom_advertising )
{
    expected_scan_response( {
        0x11, 0x22, 0x33, 0x44, 0x55
    }, *this );
}

BOOST_AUTO_TEST_SUITE_END()
