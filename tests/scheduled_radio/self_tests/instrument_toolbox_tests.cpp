/**
 * @file instrument_toolbox_tests.cpp
 *
 * The pairing toolbox through the rig, on the host: the radio is the dummy with the
 * software toolbox of tests/security_manager/test_sm.hpp behind it, and every call goes
 * through the proxy, the frames and the dispatcher. The vectors are those of the Core
 * Specification, Vol 3, Part H, Appendix D, as in test_sm_tests.cpp; a wrong result here
 * is a wrong serialisation of an argument or a result.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/dummy_port.hpp"
#include "host/dummy_radio.hpp"
#include "host/proxy.hpp"
#include "instrument/dut_rig.hpp"
#include "link/frame.hpp"

#include "security_manager/test_sm.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    using bluetoe::details::ecdh_private_key_t;
    using bluetoe::details::ecdh_public_key_t;
    using bluetoe::details::ecdh_shared_secret_t;
    using bluetoe::details::io_capabilities_t;
    using bluetoe::details::uint128_t;
    using bluetoe::link_layer::device_address;
    using bluetoe::link_layer::public_device_address;

    /*
     * The dummy radio with a real toolbox behind it.
     */
    template < typename CallBacks >
    class toolbox_radio : public dummy_radio< CallBacks >
    {
    public:
        std::pair< ecdh_public_key_t, ecdh_private_key_t > generate_keys()
        {
            return toolbox_.generate_keys();
        }

        uint128_t select_random_nonce()
        {
            return toolbox_.select_random_nonce();
        }

        ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key )
        {
            return toolbox_.p256( private_key, public_key );
        }

        uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const uint128_t& k, std::uint8_t z )
        {
            return toolbox_.f4( u, v, k, z );
        }

        std::pair< uint128_t, uint128_t > f5(
            const ecdh_shared_secret_t& dh_key, const uint128_t& n1, const uint128_t& n2,
            const device_address& a1, const device_address& a2 )
        {
            return toolbox_.f5( dh_key, n1, n2, a1, a2 );
        }

        uint128_t f6(
            const uint128_t& w, const uint128_t& n1, const uint128_t& n2, const uint128_t& r,
            const io_capabilities_t& io_caps, const device_address& a1, const device_address& a2 )
        {
            return toolbox_.f6( w, n1, n2, r, io_caps, a1, a2 );
        }

        std::uint32_t g2( const std::uint8_t* u, const std::uint8_t* v, const uint128_t& x, const uint128_t& y )
        {
            return toolbox_.g2( u, v, x, y );
        }

    private:
        test::lesc_security_functions toolbox_;
    };

    /*
     * A port that keeps its instance and exposes its buffers, like the one of the rig
     * tests; the buffer type is fixed here, since the rig's default is known.
     */
    using buffer_t = ring_buffer< std::uint8_t, 256 + frame_overhead >;

    template < typename Buffer, typename Wake >
    class observed_port : public dummy_port< Buffer, Wake >
    {
    public:
        observed_port( Buffer& receive, Buffer& transmit, Wake& wake )
            : dummy_port< Buffer, Wake >( receive, transmit, wake )
            , receive( receive )
            , transmit( transmit )
        {
            instance = this;
        }

        Buffer& receive;
        Buffer& transmit;

        static inline observed_port* instance = nullptr;
    };

    using rig_t = dut_rig< toolbox_radio, observed_port >;

    struct rig_transport
    {
        rig_t                                   rig{ "software toolbox on the host", "unit test build" };
        observed_port< buffer_t, rig_t::radio_t >& port = *observed_port< buffer_t, rig_t::radio_t >::instance;
        frame_sender< buffer_t >                sender{ port.receive };
        frame_receiver< 256, buffer_t >         receiver{ port.transmit };

        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            if ( !sender.send( request ) )
                throw link_error( "request does not fit" );

            rig.run();

            if ( receiver.receive() != receive_result::frame )
                throw link_error( "no response" );

            return { receiver.payload().begin(), receiver.payload().end() };
        }
    };

    struct fixture
    {
        rig_transport                           transport;
        proxy< rig_t::functions, rig_transport >  remote{ transport };
    };

    template < class A, class B >
    void check_equal( const A& a, const B& b )
    {
        BOOST_CHECK_EQUAL_COLLECTIONS( a.begin(), a.end(), b.begin(), b.end() );
    }

    // D.1 P-256 key agreement
    const ecdh_private_key_t private_a = {{
        0xbd, 0x1a, 0x3c, 0xcd, 0xa6, 0xb8, 0x99, 0x58, 0x99, 0xb7, 0x40, 0xeb, 0x7b, 0x60, 0xff, 0x4a,
        0x50, 0x3f, 0x10, 0xd2, 0xe3, 0xb3, 0xc9, 0x74, 0x38, 0x5f, 0xc5, 0xa3, 0xd4, 0xf6, 0x49, 0x3f
    }};

    const ecdh_private_key_t private_b = {{
        0xfd, 0xc5, 0x7f, 0xf4, 0x49, 0xdd, 0x4f, 0x6b, 0xfb, 0x7c, 0x9d, 0xf1, 0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee, 0xfb, 0xfc, 0x0a, 0x90, 0x9a, 0xbb, 0xf6, 0x32, 0x3d, 0x8b, 0x18, 0x55
    }};

    const ecdh_public_key_t public_a = {{
        0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc, 0xdb, 0xfd, 0xf4, 0xac, 0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e, 0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20,
        0x8b, 0xd2, 0x89, 0x15, 0xd0, 0x8e, 0x1c, 0x74, 0x24, 0x30, 0xed, 0x8f, 0xc2, 0x45, 0x63, 0x76,
        0x5c, 0x15, 0x52, 0x5a, 0xbf, 0x9a, 0x32, 0x63, 0x6d, 0xeb, 0x2a, 0x65, 0x49, 0x9c, 0x80, 0xdc
    }};

    const ecdh_public_key_t public_b = {{
        0x90, 0xa1, 0xaa, 0x2f, 0xb2, 0x77, 0x90, 0x55, 0x9f, 0xa6, 0x15, 0x86, 0xfd, 0x8a, 0xb5, 0x47,
        0x00, 0x4c, 0x9e, 0xf1, 0x84, 0x22, 0x59, 0x09, 0x96, 0x1d, 0xaf, 0x1f, 0xf0, 0xf0, 0xa1, 0x1e,
        0x4a, 0x21, 0xb1, 0x15, 0xf9, 0xaf, 0x89, 0x5f, 0x76, 0x36, 0x8e, 0xe2, 0x30, 0x11, 0x2d, 0x47,
        0x60, 0x51, 0xb8, 0x9a, 0x3a, 0x70, 0x56, 0x73, 0x37, 0xad, 0x9d, 0x42, 0x3e, 0xf3, 0x55, 0x4c
    }};

    const ecdh_shared_secret_t dh_key = {{
        0x98, 0xa6, 0xbf, 0x73, 0xf3, 0x34, 0x8d, 0x86, 0xf1, 0x66, 0xf8, 0xb4, 0x13, 0x6b, 0x79, 0x99,
        0x9b, 0x7d, 0x39, 0x0a, 0xa6, 0x10, 0x10, 0x34, 0x05, 0xad, 0xc8, 0x57, 0xa3, 0x34, 0x02, 0xec
    }};

    // D.2 f4 and D.5 g2 share u, v and x
    const rig_t::coordinate_t u = {{
        0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc, 0xdb, 0xfd, 0xf4, 0xac, 0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e, 0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20
    }};

    const rig_t::coordinate_t v = {{
        0xfd, 0xc5, 0x7f, 0xf4, 0x49, 0xdd, 0x4f, 0x6b, 0xfb, 0x7c, 0x9d, 0xf1, 0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee, 0xfb, 0xfc, 0x0a, 0x90, 0x9a, 0xbb, 0xf6, 0x32, 0x3d, 0x8b, 0x18, 0x55
    }};

    const uint128_t nonce_central = {{
        0xab, 0xae, 0x2b, 0x71, 0xec, 0xb2, 0xff, 0xff, 0x3e, 0x73, 0x77, 0xd1, 0x54, 0x84, 0xcb, 0xd5
    }};

    const uint128_t nonce_peripheral = {{
        0xcf, 0xc4, 0x3d, 0xff, 0xf7, 0x83, 0x65, 0x21, 0x6e, 0x5f, 0xa7, 0x25, 0xcc, 0xe7, 0xe8, 0xa6
    }};

    const public_device_address addr_controller( { 0xce, 0xbf, 0x37, 0x37, 0x12, 0x56 } );
    const public_device_address addr_peripheral( { 0xc1, 0xcf, 0x2d, 0x70, 0x13, 0xa7 } );
}

BOOST_FIXTURE_TEST_CASE( f4_matches_the_specification, fixture )
{
    const uint128_t expected = {{
        0x2d, 0x87, 0x74, 0xa9, 0xbe, 0xa1, 0xed, 0xf1, 0x1c, 0xbd, 0xa9, 0x07, 0xf1, 0x16, 0xc9, 0xf2
    }};

    check_equal( remote.call< &rig_t::f4 >( u, v, nonce_central, 0x00 ), expected );
}

BOOST_FIXTURE_TEST_CASE( p256_matches_the_specification_in_both_directions, fixture )
{
    check_equal( remote.call< &rig_t::p256 >( private_a, public_b ), dh_key );
    check_equal( remote.call< &rig_t::p256 >( private_b, public_a ), dh_key );
}

BOOST_FIXTURE_TEST_CASE( f5_matches_the_specification, fixture )
{
    const uint128_t expected_mac_key = {{
        0x20, 0x6e, 0x63, 0xce, 0x20, 0x6a, 0x3f, 0xfd, 0x02, 0x4a, 0x08, 0xa1, 0x76, 0xf1, 0x65, 0x29
    }};

    const uint128_t expected_ltk = {{
        0x38, 0x0a, 0x75, 0x94, 0xb5, 0x22, 0x05, 0x98, 0x23, 0xcd, 0xd7, 0x69, 0x11, 0x79, 0x86, 0x69
    }};

    const auto [ mac_key, ltk ] = remote.call< &rig_t::toolbox_t::f5 >(
        dh_key, nonce_central, nonce_peripheral, addr_controller, addr_peripheral );

    check_equal( mac_key, expected_mac_key );
    check_equal( ltk, expected_ltk );
}

BOOST_FIXTURE_TEST_CASE( f6_matches_the_specification, fixture )
{
    const uint128_t mac_key = {{
        0x20, 0x6e, 0x63, 0xce, 0x20, 0x6a, 0x3f, 0xfd, 0x02, 0x4a, 0x08, 0xa1, 0x76, 0xf1, 0x65, 0x29
    }};

    const uint128_t r = {{
        0xc8, 0x0f, 0x2d, 0x0c, 0xd2, 0x42, 0xda, 0x08, 0x54, 0xbb, 0x53, 0xb4, 0x3b, 0x34, 0xa3, 0x12
    }};

    const io_capabilities_t io_caps = {{ 0x02, 0x01, 0x01 }};

    const uint128_t expected = {{
        0x61, 0x8f, 0x95, 0xda, 0x09, 0x0b, 0x6c, 0xd2, 0xc5, 0xe8, 0xd0, 0x9c, 0x98, 0x73, 0xc4, 0xe3
    }};

    check_equal( remote.call< &rig_t::toolbox_t::f6 >(
        mac_key, nonce_central, nonce_peripheral, r, io_caps, addr_controller, addr_peripheral ), expected );
}

BOOST_FIXTURE_TEST_CASE( g2_matches_the_specification, fixture )
{
    BOOST_CHECK_EQUAL( remote.call< &rig_t::g2 >( u, v, nonce_central, nonce_peripheral ), 0x2f9ed5bau );
}

/*
 * The generated pair is the software toolbox's canned pair, so the agreement shows that a
 * pair of arrays travels as a result and that the private key comes back usable.
 */
BOOST_FIXTURE_TEST_CASE( key_agreement_with_the_generated_pair, fixture )
{
    const auto [ public_key, private_key ] = remote.call< &rig_t::toolbox_t::generate_keys >();

    check_equal( remote.call< &rig_t::p256 >( private_key, public_a ), remote.call< &rig_t::p256 >( private_a, public_key ) );
}

BOOST_FIXTURE_TEST_CASE( the_nonce_is_the_toolboxes_nonce, fixture )
{
    check_equal( remote.call< &rig_t::toolbox_t::select_random_nonce >(), nonce_central );
}
