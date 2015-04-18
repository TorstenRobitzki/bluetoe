
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
BOOST_FIXTURE_TEST_CASE( pdu_too_small, request_with_reponse< small_temperature_service > )
{
    static const std::uint8_t too_small[] = { 0x04, 0x00, 0x00, 0xff };

    BOOST_CHECK( check_error_response( too_small, 0x04, 0x0000, 0x04 ) );
}

/**
 * @test response with an error if the starting handle is zero
 */
BOOST_FIXTURE_TEST_CASE( start_handle_zero, request_with_reponse< small_temperature_service > )
{
    static const std::uint8_t start_zero[] = { 0x04, 0x00, 0x00, 0xff, 0xff };

    BOOST_CHECK( check_error_response( start_zero, 0x04, 0x0000, 0x01 ) );
}

/**
 * @test response with an invalid handle, if starting handle is larger than ending handle
 */
BOOST_FIXTURE_TEST_CASE( start_handle_larger_than_ending, request_with_reponse< small_temperature_service > )
{
    static const std::uint8_t larger_than[] = { 0x04, 0x06, 0x00, 0x05, 0x00 };

    BOOST_CHECK( check_error_response( larger_than, 0x04, 0x0006, 0x01 ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( find_small_temperature_service_valid_requests )

BOOST_FIXTURE_TEST_CASE( request_out_of_range, request_with_reponse< small_temperature_service > )
{
    static const std::uint8_t request[] = { 0x04, 0x04, 0x00, 0xff, 0xff };
    BOOST_CHECK( check_error_response( request, 0x04, 0x0004, 0x0A ) );
}

BOOST_FIXTURE_TEST_CASE( correct_opcode, request_with_reponse< small_temperature_service > )
{
    l2cap_input( request_all_attributes );

    BOOST_REQUIRE_GT( response_size, 0 );
    BOOST_CHECK_EQUAL( response[ 0 ], 0x05 );
}

BOOST_FIXTURE_TEST_CASE( correct_list_of_16bit_uuids, request_with_reponse< small_temperature_service > )
{
    l2cap_input( request_all_attributes );

    static const std::uint8_t expected_response[] = {
        0x05, 0x01,             // response opcode and format
        0x01, 0x00, 0x00, 0x28, // service definition
        0x02, 0x00, 0x03, 0x28  // Characteristic Declaration
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

BOOST_FIXTURE_TEST_CASE( correct_list_of_128bit_uuids, request_with_reponse< small_temperature_service > )
{
    static const std::uint8_t request_value_attribute[] = { 0x04, 0x03, 0x00, 0x03, 0x00 };
    l2cap_input( request_value_attribute );

    static const std::uint8_t expected_response[] = {
        0x05, 0x02,             // response opcode and format
        0x03, 0x00,             // handle
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAA
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( more_than_one_characteristics )

BOOST_FIXTURE_TEST_CASE( as_long_as_mtu_size_is_large_enough_all_16bit_uuids_will_be_served, request_with_reponse< three_apes_service > )
{
    l2cap_input( request_all_attributes );

    static const std::uint8_t expected_response[] = {
        0x05, 0x01,             // response opcode and format
        0x01, 0x00, 0x00, 0x28, // service definition
        0x02, 0x00, 0x03, 0x28, // Characteristic Declaration
        0x04, 0x00, 0x03, 0x28,
        0x06, 0x00, 0x03, 0x28
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

BOOST_FIXTURE_TEST_CASE( response_includes_the_starting_and_ending_handle, request_with_reponse< three_apes_service > )
{
    const std::uint8_t request[] = { 0x04, 0x02, 0x00, 0x04, 0x00 };
    l2cap_input( request );

    static const std::uint8_t expected_response[] = {
        0x05, 0x01,
        0x02, 0x00, 0x03, 0x28,
        0x04, 0x00, 0x03, 0x28
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

typedef request_with_reponse< three_apes_service, 56 > request_with_reponse_three_apes_service_56;

BOOST_FIXTURE_TEST_CASE( response_will_contain_only_128bit_uuids_if_the_first_one_is_a_128_bituuid, request_with_reponse_three_apes_service_56 )
{
    const std::uint8_t request[] = { 0x04, 0x03, 0x00, 0xff, 0xff };
    l2cap_input( request );

    static const std::uint8_t expected_response[] = {
        0x05, 0x02,
        0x03, 0x00,
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAA,
        0x05, 0x00,
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAB,
        0x07, 0x00,
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAC
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

typedef request_with_reponse< three_apes_service, 55 > request_with_reponse_three_apes_service_55;
BOOST_FIXTURE_TEST_CASE( response_will_contain_one_whole_tuples, request_with_reponse_three_apes_service_55 )
{
    const std::uint8_t request[] = { 0x04, 0x03, 0x00, 0xff, 0xff };
    l2cap_input( request );

    static const std::uint8_t expected_response[] = {
        0x05, 0x02,
        0x03, 0x00,
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAA,
        0x05, 0x00,
        0x8C, 0x8B, 0x40, 0x94,
        0x0D, 0xE2, 0x49, 0x9F,
        0xA2, 0x8A, 0x4E, 0xED,
        0x5B, 0xC7, 0x3C, 0xAB
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::begin( response ) + response_size, std::begin( expected_response ), std::end( expected_response ) );
}

BOOST_AUTO_TEST_SUITE_END()

