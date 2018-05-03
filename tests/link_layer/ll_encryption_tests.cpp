#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"

namespace test {
    std::uint16_t secret_value;

    using secret_service = bluetoe::server<
        bluetoe::service<
            bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
            bluetoe::characteristic<
                bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CAA >,
                bluetoe::bind_characteristic_value< decltype( secret_value ), &secret_value >,
                bluetoe::no_write_access
            >,
            bluetoe::requires_encryption
        >
    >;
}

struct link_layer_with_security : unconnected_base_t< test::secret_service, test::radio_with_encryption >
{

    link_layer_with_security()
    {
        respond_to( 37, valid_connection_request_pdu );
    }

    void expected_response( const std::initializer_list< std::uint8_t >& expected_response, std::size_t event = 1, std::size_t pdu = 0 )
    {
        auto response = connection_events().at( event ).transmitted_data.at( pdu );
        response[ 0 ] &= 0x03;

        BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::end( response ), std::begin( expected_response ), std::end( expected_response ) );
    }
};

BOOST_FIXTURE_TEST_CASE( response_to_an_feature_request_with_security_enabled, link_layer_with_security )
{
    ll_control_pdu({
        0x08,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    });
    ll_empty_pdu();

    run();

    expected_response( {
        0x03, 0x09,
        0x09,
        0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    } );
}

BOOST_FIXTURE_TEST_CASE( encryption_request_unknown_long_term_key, link_layer_with_security )
{
    ll_control_pdu({
        0x03,                                   // LL_ENC_REQ
        0x00, 0x11, 0x22, 0x33,                 // Rand
        0x44, 0x55, 0x66, 0x77,
        0x34, 0x12,                             // EDIV
        0x00, 0x10, 0x20, 0x30,                 // SKDm
        0x40, 0x50, 0x60, 0x70,
        0xab, 0xbc, 0x12, 0x34,                 // IVm
    });
    ll_empty_pdu();
    ll_empty_pdu();

    run( 2 );

    expected_response( {
        0x03, 0x0d,
        0x04,                                   // LL_ENC_RSP
        0x56, 0xaa, 0x55, 0x78,                 // SKDm
        0x10, 0x22, 0xac, 0x3f,
        0x12, 0x34, 0x56, 0x78,                 // IVm
    } );

    expected_response( {
        0x03, 0x02,
        0x0D,                                   // LL_REJECT_IND
        0x06                                    // ErrorCode
    }, 1, 1 );
}
