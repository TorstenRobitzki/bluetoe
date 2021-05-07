
#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_servers.hpp"

namespace {
    const std::uint8_t request_all_attributes[] = { 0x04, 0x01, 0x00, 0xff, 0xff };
}

BOOST_AUTO_TEST_SUITE( find_information_errors )

/**
 * @test response with an error if PDU size is to small
 */
BOOST_FIXTURE_TEST_CASE( pdu_too_small, test::request_with_reponse< test::small_temperature_service > )
{
    static const std::uint8_t too_small[] = { 0x04, 0x00, 0x00, 0xff };

    BOOST_CHECK( check_error_response( too_small, 0x04, 0x0000, 0x04 ) );
}

/**
 * @test response with an error if the starting handle is zero
 */
BOOST_FIXTURE_TEST_CASE( start_handle_zero, test::request_with_reponse< test::small_temperature_service > )
{
    static const std::uint8_t start_zero[] = { 0x04, 0x00, 0x00, 0xff, 0xff };

    BOOST_CHECK( check_error_response( start_zero, 0x04, 0x0000, 0x01 ) );
}

/**
 * @test response with an invalid handle, if starting handle is larger than ending handle
 */
BOOST_FIXTURE_TEST_CASE( start_handle_larger_than_ending, test::request_with_reponse< test::small_temperature_service > )
{
    static const std::uint8_t larger_than[] = { 0x04, 0x06, 0x00, 0x05, 0x00 };

    BOOST_CHECK( check_error_response( larger_than, 0x04, 0x0006, 0x01 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( find_small_temperature_service_valid_requests )

BOOST_FIXTURE_TEST_CASE( request_out_of_range, test::request_with_reponse< test::small_temperature_service > )
{
    static const std::uint8_t request[] = { 0x04, 0x04, 0x00, 0xff, 0xff };
    BOOST_CHECK( check_error_response( request, 0x04, 0x0004, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( correct_opcode, test::request_with_reponse< test::small_temperature_service > )
{
    l2cap_input( request_all_attributes );

    BOOST_REQUIRE_GT( response_size, 0u );
    BOOST_CHECK_EQUAL( response[ 0 ], 0x05 );
}

BOOST_FIXTURE_TEST_CASE( correct_list_of_16bit_uuids, test::request_with_reponse< test::small_temperature_service > )
{
    l2cap_input( request_all_attributes );

    expected_result({
        0x05, 0x01,             // response opcode and format
        0x01, 0x00, 0x00, 0x28, // service definition
        0x02, 0x00, 0x03, 0x28  // Characteristic Declaration
    });
}

BOOST_FIXTURE_TEST_CASE( correct_list_of_128bit_uuids, test::request_with_reponse< test::small_temperature_service > )
{
    static const std::uint8_t request_value_attribute[] = { 0x04, 0x03, 0x00, 0x03, 0x00 };
    l2cap_input( request_value_attribute );

    expected_result({
        0x05, 0x02,             // response opcode and format
        0x03, 0x00,             // handle
        0xAA, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( more_than_one_characteristics )

BOOST_FIXTURE_TEST_CASE( as_long_as_mtu_size_is_large_enough_all_16bit_uuids_will_be_served, test::request_with_reponse< test::three_apes_service > )
{
    l2cap_input( request_all_attributes );

    expected_result({
        0x05, 0x01,             // response opcode and format
        0x01, 0x00, 0x00, 0x28, // service definition
        0x02, 0x00, 0x03, 0x28, // Characteristic Declaration
        0x04, 0x00, 0x03, 0x28,
        0x06, 0x00, 0x03, 0x28
    });
}

BOOST_FIXTURE_TEST_CASE( response_includes_the_starting_and_ending_handle, test::request_with_reponse< test::three_apes_service > )
{
    const std::uint8_t request[] = { 0x04, 0x02, 0x00, 0x04, 0x00 };
    l2cap_input( request );

    expected_result({
        0x05, 0x01,
        0x02, 0x00, 0x03, 0x28,
        0x04, 0x00, 0x03, 0x28
    });
}

typedef test::request_with_reponse< test::three_apes_service, 56 > request_with_reponse_three_apes_service_56;

BOOST_FIXTURE_TEST_CASE( response_will_contain_only_128bit_uuids_if_the_first_one_is_a_128_bituuid, request_with_reponse_three_apes_service_56 )
{
    const std::uint8_t request[] = { 0x04, 0x03, 0x00, 0xff, 0xff };
    l2cap_input( request );

    expected_result({
        0x05, 0x02,
        0x03, 0x00,
        0xAA, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x05, 0x00,
        0xAB, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x07, 0x00,
        0xAC, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });
}

typedef test::request_with_reponse< test::three_apes_service, 55 > request_with_reponse_three_apes_service_55;
BOOST_FIXTURE_TEST_CASE( response_will_contain_one_whole_tuples, request_with_reponse_three_apes_service_55 )
{
    const std::uint8_t request[] = { 0x04, 0x03, 0x00, 0xff, 0xff };
    l2cap_input( request );

    expected_result({
        0x05, 0x02,
        0x03, 0x00,
        0xAA, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C,
        0x05, 0x00,
        0xAB, 0x3C, 0xC7, 0x5B,
        0xED, 0x4E, 0x8A, 0xA2,
        0x9F, 0x49, 0xE2, 0x0D,
        0x94, 0x40, 0x8B, 0x8C
    });
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( server_with_fixed_attributes )

    static const char char_name_foo[] = "Foo";
    static const char char_name_bar[] = "Bar";
    static const char char_name_baz[] = "Baz";
    static const std::uint8_t descriptor_data[] = { 0x08, 0x15 };

    using server_with_additional_descriptors = bluetoe::server<
        bluetoe::no_gap_service_for_gatt_servers,
        bluetoe::service<
            bluetoe::attribute_handle< 0x020 >,
            bluetoe::service_uuid16< 0x0815 >,

            // Characteristic with CCCD and Characteristic User Description without fixed CCCD
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1516 >,
                bluetoe::attribute_handles< 0x50, 0x52 >,
                bluetoe::fixed_uint8_value< 0x42 >,
                bluetoe::characteristic_name< char_name_foo >,
                bluetoe::notify
            >,
            // Characteristic with CCCD and Characteristic User Description and user descriptor with fixed CCCD
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1517 >,
                bluetoe::attribute_handles< 0x60, 0x62, 0x64 >,
                bluetoe::fixed_uint8_value< 0x43 >,
                bluetoe::characteristic_name< char_name_bar >,
                bluetoe::descriptor< 0x1722, descriptor_data, sizeof( descriptor_data ) >,
                bluetoe::notify
            >,
            // No descriptors, no fixup
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1518 >,
                bluetoe::fixed_uint8_value< 0x44 >
            >,
            // Characteristic Characteristic User Description without fixed CCCD
            bluetoe::characteristic<
                bluetoe::characteristic_uuid16< 0x1519 >,
                bluetoe::attribute_handles< 0x70, 0x80 >,
                bluetoe::fixed_uint8_value< 0x45 >,
                bluetoe::characteristic_name< char_name_baz >
            >
        >
    >;

    using fixture = test::request_with_reponse< server_with_additional_descriptors, 200 >;
    BOOST_FIXTURE_TEST_CASE( find_all, fixture )
    {
        l2cap_input({
            0x04,           // Find Information Request
            0x01, 0x00,     // 0x0001
            0xff, 0xff      // 0xffff
        });

        expected_result({
            0x05, 0x01,
            // first characteristic
            0x20, 0x00,     // 0x0020 ->
            0x00, 0x28,     // 0x2800
            0x50, 0x00,     // 0x0050 ->
            0x03, 0x28,     // 0x2803 Characteristic Declaration
            0x52, 0x00,     // 0x0052 ->
            0x16, 0x15,     // 0x1516
            0x53, 0x00,     // 0x0053 ->
            0x02, 0x29,     // 0x2902 «Client Characteristic Configuration»
            0x54, 0x00,     // 0x0054 ->
            0x01, 0x29,     // 0x2901 «Characteristic User Description»
            // second characteristic
            0x60, 0x00,     // 0x0060 ->
            0x03, 0x28,     // 0x2803 Characteristic Declaration
            0x62, 0x00,     // 0x0062 ->
            0x17, 0x15,     // 0x1517
            0x64, 0x00,     // 0x0064 ->
            0x02, 0x29,     // 0x2902 «Client Characteristic Configuration»
            0x65, 0x00,     // 0x0065 ->
            0x01, 0x29,     // 0x2901 «Characteristic User Description»
            0x66, 0x00,     // 0x0066 ->
            0x22, 0x17,     // 0x1722
            // third characteristic
            0x67, 0x00,     // 0x0067 ->
            0x03, 0x28,     // 0x2803 Characteristic Declaration
            0x68, 0x00,     // 0x0068 ->
            0x18, 0x15,     // 0x1518
            // forth characteristic
            0x70, 0x00,     // 0x0070 ->
            0x03, 0x28,     // 0x2803 Characteristic Declaration
            0x80, 0x00,     // 0x0080 ->
            0x19, 0x15,     // 0x1519
            0x81, 0x00,     // 0x0065 ->
            0x01, 0x29,     // 0x2901 «Characteristic User Description»


        });
    }

BOOST_AUTO_TEST_SUITE_END()

