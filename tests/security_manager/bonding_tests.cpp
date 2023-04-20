#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/security_manager.hpp>

#include "test_sm.hpp"

struct data_base_t
{
    template < class Radio >
    bluetoe::details::longterm_key_t create_new_bond(
        Radio& /*radio*/, const bluetoe::link_layer::device_address& mac )
    {
        creating_mac = mac;

        return bluetoe::details::longterm_key_t{
            .longterm_key = {
                0x01, 0x02, 0x03, 0x04,
                0x11, 0x12, 0x13, 0x14,
                0x21, 0x22, 0x23, 0x24,
                0x31, 0x32, 0x33, 0x34
            },
            .rand         = 0x12345678aabbccddu,
            .ediv         = 0x1122u
        };
    }

    template < class Connection >
    void store_bond(
        const bluetoe::details::longterm_key_t& key,
        const Connection& connection)
    {
        stored_bond_key = key;
        stored_bond_mac = connection.remote_address();
    }

    std::pair< bool, bluetoe::details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand, const bluetoe::link_layer::device_address& remote_address ) const
    {
        requesting_ediv = ediv;
        requesting_rand = rand;
        requesting_mac  = remote_address;

        return stored_key;
    }

    std::pair< bool, bluetoe::details::uint128_t > stored_key = { false, {} };
    mutable std::uint16_t requesting_ediv = ~0;
    mutable std::uint64_t requesting_rand = ~0;
    mutable bluetoe::link_layer::device_address requesting_mac;
    mutable bluetoe::link_layer::device_address creating_mac;

    bluetoe::details::longterm_key_t     stored_bond_key;
    bluetoe::link_layer::device_address  stored_bond_mac;
} data_base;

struct use_data_base : test::legacy_security_manager<
    100u,
    bluetoe::bonding_data_base< data_base_t, data_base > >
{
    use_data_base()
    {
        data_base = data_base_t();
    }

    using connection_data = channel_data_t< bluetoe::details::no_such_type >;

    std::pair< bool, bluetoe::details::uint128_t > call_find_key( std::uint16_t ediv, std::uint64_t rand, const connection_data& connection ) const
    {
        return connection.find_key( ediv, rand );
    }
};

BOOST_FIXTURE_TEST_CASE( no_key_stored, use_data_base )
{
    connection_data connection;
    BOOST_CHECK( !call_find_key( 0, 0, connection ).first );
}

BOOST_FIXTURE_TEST_CASE( key_stored, use_data_base )
{
    connection_data connection;
    data_base.stored_key = { true, { 0x12, 0x23, 0x34, 0x45 } };

    const auto key = call_find_key( 47, 11, connection );

    BOOST_CHECK( key.first );
    BOOST_TEST( key.second == ( bluetoe::details::uint128_t{ 0x12, 0x23, 0x34, 0x45 } ) );
    BOOST_TEST( data_base.requesting_ediv == 47u );
    BOOST_TEST( data_base.requesting_rand == 11u );
}

BOOST_FIXTURE_TEST_CASE( bonding_is_requested_in_response, use_data_base )
{
    // Pairing request / response
    expected(
        {
            0x01,                   // Pairing Request
            0x01, 0x00, 0x08, 0x10, 0x07, 0x07
        },
        {
            0x02,                   // Pairing Response
            0x03, 0x00, 0x01, 0x10, 0x00, 0x01
        }
    );

    // Pairing confirm
    expected(
        {
            0x03,                   // Pairing Confirm
            0x29, 0x99, 0xe1, 0x97,
            0x95, 0x06, 0x67, 0x76,
            0x66, 0xce, 0x0f, 0xf8,
            0x95, 0xb9, 0x32, 0x83
        },
        {
            0x03,                   // Pairing Confirm
            0x29, 0x99, 0xe1, 0x97, // Confirm Value
            0x95, 0x06, 0x67, 0x76,
            0x66, 0xce, 0x0f, 0xf8,
            0x95, 0xb9, 0x32, 0x83
        }
    );

    // Pairing Random
    expected(
        {
            0x04,                   // opcode
            0xE0, 0x2E, 0x70, 0xC6,
            0x4E, 0x27, 0x88, 0x63,
            0x0E, 0x6F, 0xAD, 0x56,
            0x21, 0xD5, 0x83, 0x57
        },
        {
            0x04,                   // opcode
            0xE0, 0x2E, 0x70, 0xC6,
            0x4E, 0x27, 0x88, 0x63,
            0x0E, 0x6F, 0xAD, 0x56,
            0x21, 0xD5, 0x83, 0x57
        }
    );

    // no key transmitted till the connection gets encrypted
    expected({});

    connection_data_.is_encrypted( true );

    expected({
        0x06,                   // Encryption Information
        0x01, 0x02, 0x03, 0x04,
        0x11, 0x12, 0x13, 0x14,
        0x21, 0x22, 0x23, 0x24,
        0x31, 0x32, 0x33, 0x34
    });

    expected({
        0x07,                   // Central Identification
        0x22, 0x11,             // EDIV
        0xdd, 0xcc, 0xbb, 0xaa, // Rand
        0x78, 0x56, 0x34, 0x12
    });
}