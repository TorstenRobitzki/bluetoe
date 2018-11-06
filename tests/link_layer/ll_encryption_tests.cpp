#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include <bluetoe/sm/pairing_status.hpp>

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

    // key, returned by find_key()
    std::pair< bool, bluetoe::details::uint128_t > key_vault;
    const bluetoe::details::uint128_t example_key = { {
        0x01, 0x80, 0x02, 0x70,
        0x03, 0x60, 0x04, 0x50,
        0x05, 0x40, 0x06, 0x30,
        0x07, 0x20, 0x08, 0x10
    } };

    std::uint16_t ediv;
    std::uint64_t rand;

    /**
     * A mocked security manager to be used by the link_layer under test to
     * easily fake the pairing state and the set of available keys.
     */
    struct security_manager
    {
        template < class OtherConnectionData >
        class connection_data : public OtherConnectionData
        {
        public:
            template < class ... Args >
            connection_data( Args&&... args )
                : OtherConnectionData( args... )
            {}

            std::pair< bool, bluetoe::details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                ::test::ediv = ediv;
                ::test::rand = rand;

                return key_vault;
            }

            void remote_connection_created( const bluetoe::link_layer::device_address& )
            {
            }

            bluetoe::device_pairing_status local_device_pairing_status() const
            {
                return bluetoe::device_pairing_status::no_key;
            }
        };

        template < class OtherConnectionData, class SecurityFunctions >
        void l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t*, std::size_t&, connection_data< OtherConnectionData >&, SecurityFunctions& )
        {
        }

        struct meta_type :
            bluetoe::details::security_manager_meta_type,
            bluetoe::link_layer::details::valid_link_layer_option_meta_type {};
    };
}

struct link_layer_with_security : unconnected_base_t< test::secret_service, test::radio_with_encryption, test::security_manager, test::buffer_sizes >
{

    link_layer_with_security()
    {
        respond_to( 37, valid_connection_request_pdu );
        test::key_vault = { false, { { 0x00 } } };
        test::ediv      = 0u;
        test::rand      = 0u;
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

BOOST_FIXTURE_TEST_CASE( skd_and_iv_stored, link_layer_with_security )
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

    run();

    BOOST_CHECK_EQUAL( skdm(), 0x7060504030201000u );
    BOOST_CHECK_EQUAL( ivm(), 0x3412bcabu );
}

BOOST_FIXTURE_TEST_CASE( ediv_and_rand_used, link_layer_with_security )
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

    run();

    BOOST_CHECK_EQUAL( test::ediv, 0x1234u );
    BOOST_CHECK_EQUAL( test::rand, 0x7766554433221100u );
}

BOOST_FIXTURE_TEST_CASE( still_unencrytped_after_IVs_exchanged, link_layer_with_security )
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

    run();

    BOOST_CHECK( !reception_encrypted() );
    BOOST_CHECK( !transmition_encrypted() );
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

    run();

    expected_response( {
        0x03, 0x0d,
        0x04,                                   // LL_ENC_RSP
        0x56, 0xaa, 0x55, 0x78,                 // SKDm
        0x10, 0x22, 0xac, 0x3f,
        0x12, 0x34, 0x56, 0x78                  // IVm
    } );

    expected_response( {
        0x03, 0x02,
        0x0D,                                   // LL_REJECT_IND
        0x06                                    // ErrorCode
    }, 1, 1 );

    BOOST_CHECK( !reception_encrypted() );
    BOOST_CHECK( !transmition_encrypted() );
}

BOOST_FIXTURE_TEST_CASE( encryption_request_known_key, link_layer_with_security )
{
    test::key_vault = std::make_pair( true, test::example_key );

    ll_control_pdu({
        0x03,                                   // LL_ENC_REQ
        0x00, 0x00, 0x00, 0x00,                 // Rand
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00,                             // EDIV
        0x00, 0x10, 0x20, 0x30,                 // SKDm
        0x40, 0x50, 0x60, 0x70,
        0xab, 0xbc, 0x12, 0x34,                 // IVm
    });
    ll_empty_pdu();

    run();

    const auto used_key = encryption_key();
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( used_key ), std::end( used_key ), std::begin( test::example_key ), std::end( test::example_key ) );

    expected_response( {
        0x03, 0x0d,
        0x04,                                   // LL_ENC_RSP
        0x56, 0xaa, 0x55, 0x78,                 // SKDm
        0x10, 0x22, 0xac, 0x3f,
        0x12, 0x34, 0x56, 0x78                  // IVm
    } );

    expected_response( {
        0x03, 0x01,
        0x05                                    // LL_START_ENC_REQ
    }, 1, 1 );

    BOOST_CHECK( reception_encrypted() );
    BOOST_CHECK( !transmition_encrypted() );
}

struct link_layer_with_encryption_setup : link_layer_with_security
{
    link_layer_with_encryption_setup()
    {
        test::key_vault = std::make_pair( true, test::example_key );

        ll_control_pdu({
            0x03,                                   // LL_ENC_REQ
            0x00, 0x00, 0x00, 0x00,                 // Rand
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00,                             // EDIV
            0x00, 0x10, 0x20, 0x30,                 // SKDm
            0x40, 0x50, 0x60, 0x70,
            0xab, 0xbc, 0x12, 0x34,                 // IVm
        });
        ll_empty_pdu();
    }
};

BOOST_FIXTURE_TEST_CASE( start_encryption, link_layer_with_encryption_setup )
{
    ll_control_pdu({
        0x06                                    // LL_START_ENC_RSP
    });
    ll_empty_pdu();

    run();

    expected_response( {
        0x03, 0x01,
        0x06                                    // LL_START_ENC_RSP
    }, 3 );

    BOOST_CHECK( reception_encrypted() );
    BOOST_CHECK( transmition_encrypted() );
}

struct link_layer_with_encryption : link_layer_with_encryption_setup
{
    link_layer_with_encryption()
    {
        ll_control_pdu({
            0x06                                    // LL_START_ENC_RSP
        });
        ll_empty_pdu();
    }
};

BOOST_FIXTURE_TEST_CASE( start_pause_encryption, link_layer_with_encryption )
{
    ll_control_pdu({
        0x0A                                    // LL_PAUSE_ENC_REQ
    });
    ll_empty_pdu();

    run();

    expected_response( {
        0x03, 0x01,
        0x0B                                    // LL_PAUSE_ENC_RSP
    }, 5 );

    BOOST_CHECK( !reception_encrypted() );
    BOOST_CHECK( transmition_encrypted() );
}

BOOST_FIXTURE_TEST_CASE( pause_encryption, link_layer_with_encryption )
{
    ll_control_pdu({
        0x0A                                    // LL_PAUSE_ENC_REQ
    });
    ll_empty_pdu();

    ll_control_pdu({
        0x0B                                    // LL_PAUSE_ENC_RSP
    });

    run();

    expected_response( {
        0x03, 0x01,
        0x0B                                    // LL_PAUSE_ENC_RSP
    }, 5 );

    BOOST_CHECK( !reception_encrypted() );
    BOOST_CHECK( !transmition_encrypted() );
}
