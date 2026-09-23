/**
 * @file test_tools_encryption_tests.cpp
 *
 * The host's session key derivation and CCM against the sample data of the Core
 * Specification, Vol 6 Part C, 1: the same long term key, diversifier and IV, and the four
 * packets of the encryption start.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/encryption.hpp"

#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    // LTK = 0x4C68384139F574D836BCF34E9DFB01BF, least significant byte first as the security manager keeps it
    const bluetoe::details::uint128_t long_term_key = {{
        0xbf, 0x01, 0xfb, 0x9d, 0x4e, 0xf3, 0xbc, 0x36, 0xd8, 0x74, 0xf5, 0x39, 0x41, 0x38, 0x68, 0x4c }};

    constexpr std::uint64_t skdm = 0xACBDCEDFE0F10213;
    constexpr std::uint64_t skds = 0x0213243546576879;
    constexpr std::uint32_t ivm  = 0xBADCAB24;
    constexpr std::uint32_t ivs  = 0xDEAFBABE;

    const encryption_session sample( long_term_key, skdm, skds, ivm, ivs );

    const std::vector< std::uint8_t > start_enc_rsp = { 0x06 };

    const std::vector< std::uint8_t > data_1 = {
        0x17, 0x00, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30 };

    const std::vector< std::uint8_t > data_2 = {
        0x17, 0x00, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31, 0x30, 0x41, 0x42, 0x43, 0x44,
        0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51 };

    const std::vector< std::uint8_t > start_enc_rsp_1_encrypted = { 0x9f, 0xcd, 0xa7, 0xf4, 0x48 };
    const std::vector< std::uint8_t > start_enc_rsp_2_encrypted = { 0xa3, 0x4c, 0x13, 0xa4, 0x15 };

    const std::vector< std::uint8_t > data_1_encrypted = {
        0x7a, 0x70, 0xd6, 0x64, 0x15, 0x22, 0x6d, 0xf2, 0x6b, 0x17, 0x83, 0x9a, 0x06, 0x04, 0x05, 0x59,
        0x6b, 0xd6, 0x56, 0x4f, 0x79, 0x6b, 0x5b, 0x9c, 0xe6, 0xff, 0x32, 0xf7, 0x5a, 0x6d, 0x33 };

    const std::vector< std::uint8_t > data_2_encrypted = {
        0xf3, 0x88, 0x81, 0xe7, 0xbd, 0x94, 0xc9, 0xc3, 0x69, 0xb9, 0xa6, 0x68, 0x46, 0xdd, 0x47, 0x86,
        0xaa, 0x8c, 0x39, 0xce, 0x54, 0x0d, 0x0d, 0xae, 0x3a, 0xdc, 0xdf, 0x89, 0xb9, 0x60, 0x88 };
}

// SK = 0x99AD1B5226A37E3E058E3B8E27C2C666
BOOST_AUTO_TEST_CASE( the_session_key_is_derived_as_the_specification_shows )
{
    const std::array< std::uint8_t, 16 > expected = {
        0x99, 0xad, 0x1b, 0x52, 0x26, 0xa3, 0x7e, 0x3e, 0x05, 0x8e, 0x3b, 0x8e, 0x27, 0xc2, 0xc6, 0x66 };

    BOOST_TEST( sample.key() == expected, boost::test_tools::per_element() );
}

// LL_START_ENC_RSP1: from the central, the first encrypted PDU in that direction
BOOST_AUTO_TEST_CASE( the_centrals_start_enc_rsp_encrypts_as_the_specification_shows )
{
    BOOST_TEST( sample.encrypt( encryption_direction::central_to_peripheral, 0, llid::control, start_enc_rsp ) == start_enc_rsp_1_encrypted,
        boost::test_tools::per_element() );
}

// LL_START_ENC_RSP2: from the peripheral, the first in that direction
BOOST_AUTO_TEST_CASE( the_peripherals_start_enc_rsp_encrypts_as_the_specification_shows )
{
    BOOST_TEST( sample.encrypt( encryption_direction::peripheral_to_central, 0, llid::control, start_enc_rsp ) == start_enc_rsp_2_encrypted,
        boost::test_tools::per_element() );
}

// the two data packets, the second PDU in each direction
BOOST_AUTO_TEST_CASE( the_data_packets_encrypt_as_the_specification_shows )
{
    BOOST_TEST( sample.encrypt( encryption_direction::central_to_peripheral, 1, llid::start, data_1 ) == data_1_encrypted,
        boost::test_tools::per_element() );
    BOOST_TEST( sample.encrypt( encryption_direction::peripheral_to_central, 1, llid::start, data_2 ) == data_2_encrypted,
        boost::test_tools::per_element() );
}

BOOST_AUTO_TEST_CASE( a_ciphertext_decrypts_to_its_plaintext )
{
    const auto plaintext = sample.decrypt( encryption_direction::central_to_peripheral, 1, llid::start, data_1_encrypted );

    BOOST_REQUIRE( plaintext );
    BOOST_TEST( *plaintext == data_1, boost::test_tools::per_element() );
}

BOOST_AUTO_TEST_CASE( a_wrong_mic_is_detected )
{
    auto corrupted = data_1_encrypted;
    corrupted.back() ^= 0x01;

    BOOST_CHECK( !sample.decrypt( encryption_direction::central_to_peripheral, 1, llid::start, corrupted ) );
}

BOOST_AUTO_TEST_CASE( a_wrong_counter_is_detected )
{
    BOOST_CHECK( !sample.decrypt( encryption_direction::central_to_peripheral, 2, llid::start, data_1_encrypted ) );
}

BOOST_AUTO_TEST_CASE( a_wrong_direction_is_detected )
{
    BOOST_CHECK( !sample.decrypt( encryption_direction::peripheral_to_central, 1, llid::start, data_1_encrypted ) );
}

BOOST_AUTO_TEST_CASE( a_wrong_llid_is_detected )
{
    BOOST_CHECK( !sample.decrypt( encryption_direction::central_to_peripheral, 1, llid::continuation, data_1_encrypted ) );
}

BOOST_AUTO_TEST_CASE( a_ciphertext_shorter_than_a_mic_is_refused )
{
    const std::vector< std::uint8_t > too_short = { 0x01, 0x02, 0x03 };

    BOOST_CHECK( !sample.decrypt( encryption_direction::central_to_peripheral, 0, llid::start, too_short ) );
}
