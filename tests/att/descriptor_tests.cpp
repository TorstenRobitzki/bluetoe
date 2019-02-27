#include <bluetoe/descriptor.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

std::uint16_t dummy_value;

static const std::uint8_t descriptor_value[ 4 ] = { 0x01, 0x02, 0x03, 0x04 };

using server_with_single_descriptor_characterstic = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( dummy_value ), &dummy_value >,
            bluetoe::descriptor< 0x4711, descriptor_value, sizeof( descriptor_value ) >
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( single_descriptor, test::request_with_reponse< server_with_single_descriptor_characterstic > )
{
    l2cap_input({
        0x0A,                       // read request
        0x04, 0x00                  // handle
    });

    expected_result( {
        0x0B,                       // read response
        0x01, 0x02, 0x03, 0x04
    });
}

BOOST_FIXTURE_TEST_CASE( read_by_type, test::request_with_reponse< server_with_single_descriptor_characterstic > )
{
    l2cap_input({
        0x08,                       // Read By Type Request
        0x01, 0x00,                 // First requested handle number
        0xff, 0xff,                 // Last requested handle number
        0x11, 0x47                  // UUID
    });

    expected_result( {
        0x09,                       // Read By Type Response
        0x06,                       // The size of each attribute handle- value pair
        0x04, 0x00,                 // Handle
        0x01, 0x02, 0x03, 0x04      // Value
    });
}

BOOST_FIXTURE_TEST_CASE( read_blob, test::request_with_reponse< server_with_single_descriptor_characterstic > )
{
    l2cap_input({
        0x0C,                       // Read Blob Request
        0x04, 0x00,                 // handle
        0x03, 0x00                  // Offset
    });

    expected_result( {
        0x0D,                       // Read Blob Response
        0x04                        // Value
    });
}

BOOST_FIXTURE_TEST_CASE( write_only, test::request_with_reponse< server_with_single_descriptor_characterstic > )
{
    l2cap_input({
        0x12,                       // Write Request
        0x04, 0x00,                 // handle
        0x01, 0x02, 0x03, 0x05      // new Value
    });

    expected_result( {
        0x01,                       // Error Response
        0x12,                       // request opcode
        0x04, 0x00,                 // handle
        0x03                        // Write Not Permitted
    });
}

using server_with_multiple_characterstics = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( dummy_value ), &dummy_value >,
            bluetoe::descriptor< 0x4711, descriptor_value, sizeof( descriptor_value ) >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAB >,
            bluetoe::bind_characteristic_value< decltype( dummy_value ), &dummy_value >
        >
    >
>;

/*
 * Make sure, that the enumeration of the attributes works even for characteristics that follow the
 * characteristic with the descriptor
 */
BOOST_FIXTURE_TEST_CASE( read_second_char, test::request_with_reponse< server_with_multiple_characterstics > )
{
    // Characteristic Declaration of the second characteristic
    l2cap_input({
        0x0A,                       // read request
        0x05, 0x00                  // handle
    });

    expected_result( {
        0x0B,                       // read response
        0x0A,                       // Properties: read, write
        0x06, 0x00,                 // Value Attribute Handle
        0xAB, 0x3C, 0xC7, 0x5B,     // Characteristic UUID
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });
}

/* Currently not supported
static const std::uint8_t descriptor_value2[ 5 ] = { 0x10, 0x20, 0x30, 0x40, 0x50 };
using server_with_multiple_descriptor_characterstic = bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
            bluetoe::bind_characteristic_value< decltype( dummy_value ), &dummy_value >,
            bluetoe::descriptor< 0x4711, descriptor_value, sizeof( descriptor_value ) >,
            bluetoe::descriptor< 0x4712, descriptor_value2, sizeof( descriptor_value2 ) >
        >
    >
>;

BOOST_FIXTURE_TEST_CASE( multiple_descriptors, test::request_with_reponse< server_with_single_descriptor_characterstic > )
{
    l2cap_input({
        0x0A,                       // read request
        0x04, 0x00                  // handle
    });

    expected_result( {
        0x0B,                       // read response
        0x01, 0x02, 0x03, 0x04
    });

    l2cap_input({
        0x0A,                       // read request
        0x05, 0x00                  // handle
    });

    expected_result( {
        0x0B,                       // read response
        0x01, 0x02, 0x03, 0x04, 0x05
    });
}
*/

//BOOST_FIXTURE_TEST_CASE( offset_out_of_range )