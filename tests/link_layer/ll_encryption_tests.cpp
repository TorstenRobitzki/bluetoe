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

using link_layer_with_security = unconnected_base_t< test::secret_service, test::radio_with_encryption >;

BOOST_FIXTURE_TEST_CASE( response_to_an_feature_request_with_security_enabled, link_layer_with_security )
{
    respond_to( 37, valid_connection_request_pdu );
    ll_control_pdu({
        0x08,
        0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    });
    ll_empty_pdu();

    run();

    auto response = connection_events().at( 1 ).transmitted_data.at( 0 );
    response[ 0 ] &= 0x03;

    static const std::uint8_t expected_response[] = {
        0x03, 0x09,
        0x09,
        0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( response ), std::end( response ), std::begin( expected_response ), std::end( expected_response ) );
}

