#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "connected.hpp"
#include "security_manager_mock.hpp"

using namespace test;

// what the central sends in an LL_ENC_REQ, unless a test says otherwise
static constexpr std::uint64_t central_rand = 0x7766554433221100;
static constexpr std::uint16_t central_ediv = 0x1234;
static constexpr std::uint64_t central_skdm = 0x7060504030201000;
static constexpr std::uint32_t central_ivm  = 0x3412bcab;

// what the simulated radio answers with, unless a test sets up another response
static constexpr std::uint64_t simulated_skds = 0x3fac22107855aa56;
static constexpr std::uint32_t simulated_ivs  = 0x78563412;

/*
 * The link layer under test with the mocked security manager: connected, with no key in the
 * vault until a test puts one there.
 */
struct link_layer_with_security : unconnected_base_t< secret_service, radio_with_encryption, security_manager, buffer_sizes >
{
    link_layer_with_security()
    {
        respond_to( 37, valid_connection_request_pdu );
        key_vault        = { false, { { 0x00 } } };
        last_key_request = {};
    }

    // the central starts the encryption with the example key in the vault
    void request_encryption_with_known_key()
    {
        key_vault = std::make_pair( true, example_key );

        ll_control_pdu( ll_enc_req( 0, 0, central_skdm, central_ivm ) );
        ll_empty_pdu();
    }

    // the central completes the start of the encryption; not named after the test case that
    // calls it, which would name the test's own type
    void central_confirms_encryption()
    {
        ll_control_pdu( ll_start_enc_rsp() );
        ll_empty_pdu();
    }

    void check_never_encrypted() const
    {
        for ( const auto& event : connection_events() )
        {
            BOOST_CHECK( !event.receive_encryption_at_start_of_event );
            BOOST_CHECK( !event.transmit_encryption_at_start_of_event );
        }
    }

    void check_transmission_never_encrypted() const
    {
        for ( const auto& event : connection_events() )
            BOOST_CHECK( !event.transmit_encryption_at_start_of_event );
    }
};

// the features the link layer claims with a security manager: LE Encryption among them
static constexpr std::uint64_t bluetoe_features_with_encryption = 0x17;

BOOST_FIXTURE_TEST_CASE( response_to_an_feature_request_with_security_enabled, link_layer_with_security )
{
    ll_control_pdu( ll_feature_req( 0xff ) );
    ll_empty_pdu();

    run();

    check_transmitted( 1, ll_control( ll_feature_rsp( bluetoe_features_with_encryption ) ) );
}

BOOST_FIXTURE_TEST_CASE( skd_and_iv_stored, link_layer_with_security )
{
    ll_control_pdu( ll_enc_req( central_rand, central_ediv, central_skdm, central_ivm ) );

    run();

    BOOST_CHECK_EQUAL( skdm(), central_skdm );
    BOOST_CHECK_EQUAL( ivm(), central_ivm );
}

BOOST_FIXTURE_TEST_CASE( ediv_and_rand_used, link_layer_with_security )
{
    ll_control_pdu( ll_enc_req( central_rand, central_ediv, central_skdm, central_ivm ) );

    run();

    BOOST_CHECK_EQUAL( last_key_request.ediv, central_ediv );
    BOOST_CHECK_EQUAL( last_key_request.rand, central_rand );
}

BOOST_FIXTURE_TEST_CASE( still_unencrytped_after_IVs_exchanged, link_layer_with_security )
{
    ll_control_pdu( ll_enc_req( central_rand, central_ediv, central_skdm, central_ivm ) );

    run();

    check_never_encrypted();
}

BOOST_FIXTURE_TEST_CASE( encryption_request_unknown_long_term_key, link_layer_with_security )
{
    ll_control_pdu( ll_enc_req( central_rand, central_ediv, central_skdm, central_ivm ) );
    ll_empty_pdu();

    run();

    check_transmitted( 1, 0, ll_control( ll_enc_rsp( simulated_skds, simulated_ivs ) ) );
    check_transmitted( 1, 1, ll_control( ll_reject_ext_ind( 0x03, 0x06 ) ) );    // LL_ENC_REQ: PIN or Key Missing

    check_never_encrypted();
}

BOOST_FIXTURE_TEST_CASE( encryption_request_known_key, link_layer_with_security )
{
    request_encryption_with_known_key();

    run();

    const auto used_key = encryption_key();
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( used_key ), std::end( used_key ), std::begin( example_key ), std::end( example_key ) );

    check_transmitted( 1, 0, ll_control( ll_enc_rsp( simulated_skds, simulated_ivs ) ) );
    check_transmitted( 1, 1, ll_control( ll_start_enc_req() ) );

    BOOST_CHECK( connection_events().at( 1 ).receive_encryption_at_start_of_event );
    check_transmission_never_encrypted();
}

/*
  This is the "start encryption" example from the core spec (version 4.2); Vol. 6; Part C; 1

    The following parameters are set to the fixed values below:
    LTK = 0x4C68384139F574D836BCF34E9DFB01BF (MSO to LSO)
    EDIV = 0x2474 (MSO to LSO)
    RAND = 0xABCDEF1234567890 (MSO to LSO)
    SKDm = 0xACBDCEDFE0F10213 (MSO to LSO)
    SKDs = 0x0213243546576879 (MSO to LSO)
    IVm = 0xBADCAB24 (MSO to LSO)
    IVs = 0xDEAFBABE (MSO to LSO)

 */
BOOST_FIXTURE_TEST_CASE( start_encryption_example, link_layer_with_security )
{
    static const bluetoe::details::uint128_t example_long_term_key = { {
        0x4C, 0x68, 0x38, 0x41,
        0x39, 0xF5, 0x74, 0xD8,
        0x36, 0xBC, 0xF3, 0x4E,
        0x9D, 0xFB, 0x01, 0xBF
    } };

    key_vault = std::make_pair( true, example_long_term_key );
    setup_encryption_response( 0x0213243546576879, 0xDEAFBABE );

    ll_control_pdu( ll_enc_req( 0xABCDEF1234567890, 0x2474, 0xACBDCEDFE0F10213, 0xBADCAB24 ) );
    ll_empty_pdu();

    run();

    const auto used_key = encryption_key();
    BOOST_CHECK_EQUAL_COLLECTIONS( std::begin( used_key ), std::end( used_key ), std::begin( example_long_term_key ), std::end( example_long_term_key ) );

    check_transmitted( 1, 0, ll_control( ll_enc_rsp( 0x0213243546576879, 0xDEAFBABE ) ) );
    check_transmitted( 1, 1, ll_control( ll_start_enc_req() ) );

    BOOST_CHECK( connection_events().at( 1 ).receive_encryption_at_start_of_event );
    check_transmission_never_encrypted();
}

BOOST_FIXTURE_TEST_CASE( start_encryption, link_layer_with_security )
{
    request_encryption_with_known_key();
    central_confirms_encryption();

    run();

    check_transmitted( 3, ll_control( ll_start_enc_rsp() ) );

    BOOST_CHECK( connection_events().at( 1 ).receive_encryption_at_start_of_event );
    BOOST_CHECK( connection_events().at( 3 ).transmit_encryption_at_start_of_event );
}

BOOST_FIXTURE_TEST_CASE( start_pause_encryption, link_layer_with_security )
{
    request_encryption_with_known_key();
    central_confirms_encryption();

    ll_control_pdu( ll_pause_enc_req() );
    ll_empty_pdu();

    run();

    check_transmitted( 5, ll_control( ll_pause_enc_rsp() ) );

    BOOST_CHECK( !connection_events().at( 5 ).receive_encryption_at_start_of_event );
    BOOST_CHECK( connection_events().at( 5 ).transmit_encryption_at_start_of_event );
}

BOOST_FIXTURE_TEST_CASE( pause_encryption, link_layer_with_security )
{
    request_encryption_with_known_key();
    central_confirms_encryption();

    ll_control_pdu( ll_pause_enc_req() );
    ll_empty_pdu();

    ll_control_pdu( ll_pause_enc_rsp() );

    run();

    check_transmitted( 5, ll_control( ll_pause_enc_rsp() ) );

    BOOST_CHECK( !connection_events().at( 7 ).receive_encryption_at_start_of_event );
    BOOST_CHECK( !connection_events().at( 7 ).transmit_encryption_at_start_of_event );
}
