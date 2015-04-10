#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

#include <sstream>
#include <iomanip>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace {

    const char single_char_serivce_name[] = "Temperature / Bathroom";
    const char temp_characteristic_name[] = "Temperature in 10th degree celsius";

    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service_name< single_char_serivce_name >,
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::characteristic_name< temp_characteristic_name >,
            bluetoe::bind_characteristic_value< decltype( temperature_value ), &temperature_value >,
            bluetoe::read_only
        >
    > small_temperature_service;


    template < typename T >
    std::string param_to_text( const T& param )
    {
        std::ostringstream out;
        out << "0x" << std::hex << param;

        return out.str();
    }

    std::string param_to_text( std::uint8_t c )
    {
        return param_to_text( static_cast< int >( c ) );
    }

    template < typename T, typename U >
    std::string should_be_but( const char* text, const T& should, const U& but )
    {
        std::ostringstream out;
        out << "ATT: \"" << text << "\" should be " << param_to_text( should ) << " but is: " <<  param_to_text( but );
        return out.str();
    }

    template < class Server, std::size_t PDU_Size >
    bool check_error_response( Server& srv, const std::uint8_t(&input)[PDU_Size], std::uint8_t expected_request_opcode, std::uint16_t expected_attribute_handle, std::uint8_t expected_error_code )
    {
        std::uint8_t output[23];
        std::size_t  outlength = sizeof( output );

        srv.l2cap_input( input, sizeof( input ), output, outlength );

        const std::uint8_t  opcode           = output[ 0 ];
        const std::uint8_t  request_opcode   = output[ 1 ];
        const std::uint16_t attribute_handle = output[ 2 ] + ( output[ 3 ] << 8 );
        const std::uint8_t  error_code       = output[ 4 ];

        BOOST_CHECK_MESSAGE( outlength == 5, should_be_but( "PDU Size", 5, outlength ) );
        BOOST_CHECK_MESSAGE( opcode == 0x01, should_be_but( "Attribute Opcode", 0x01, opcode ) );
        BOOST_CHECK_MESSAGE( request_opcode == expected_request_opcode, should_be_but( "Request Opcode In Error", expected_request_opcode, request_opcode ) );
        BOOST_CHECK_MESSAGE( attribute_handle == expected_attribute_handle, should_be_but( "Attribute Handle In Error", expected_attribute_handle, attribute_handle ) );
        BOOST_CHECK_MESSAGE( error_code == expected_error_code, should_be_but( "Error Code", expected_error_code, error_code ) );

        return outlength == 5
            && opcode == 0x01
            && request_opcode == expected_request_opcode
            && attribute_handle == expected_attribute_handle
            && error_code == expected_error_code;
    }

}

BOOST_AUTO_TEST_SUITE( find_information_request_att )

/**
 * @test response with an error if PDU size is to small
 */
BOOST_FIXTURE_TEST_CASE( pdu_too_small, small_temperature_service )
{
    static const std::uint8_t too_small[] = { 0x04, 0x00, 0x00, 0xff };

    BOOST_CHECK( check_error_response( *this, too_small,  0x04, 0x0000, 0x04 ) );
}

/**
 * @test response with an error if the starting handle is zero
 */
BOOST_FIXTURE_TEST_CASE( start_handle_zero, small_temperature_service )
{
    static const std::uint8_t start_zero[] = { 0x04, 0x00, 0x00, 0xff, 0xff };

    BOOST_CHECK( check_error_response( *this, start_zero,  0x04, 0x0000, 0x01 ) );
}

/**
 * @test response with an invalid handle, if starting handle is larger than ending handle
 */
BOOST_FIXTURE_TEST_CASE( start_handle_larger_than_ending, small_temperature_service )
{
    static const std::uint8_t larger_than[] = { 0x04, 0x06, 0x00, 0x05, 0x00 };

    BOOST_CHECK( check_error_response( *this, larger_than,  0x04, 0x0006, 0x01 ) );
}


BOOST_AUTO_TEST_SUITE_END()
