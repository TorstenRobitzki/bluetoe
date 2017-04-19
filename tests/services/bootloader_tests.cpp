#include <iostream>
#include <iomanip>
#include <random>
#include "../hexdump.hpp"
#include "../test_uuid.hpp"

#include <bluetoe/server.hpp>
#include <bluetoe/services/bootloader.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_gatt.hpp"

using namespace test;

static constexpr std::size_t flash_start_addr  = 0x1000;
static constexpr std::size_t flash_start_addr2 = 0x2000;
static constexpr std::size_t block_size = 0x100;
static constexpr std::size_t num_blocks = 4;
static constexpr std::size_t test_mtu_size = 23;

struct handler {
    std::pair< const std::uint8_t*, std::size_t > get_version()
    {
        static const std::uint8_t version[] = { 0x47, 0x11 };
        return std::pair< const std::uint8_t*, std::size_t >( version, sizeof( version ) );
    }

    void read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination )
    {
        assert( address >= flash_start_addr );
        assert( address + size <= flash_start_addr + block_size * num_blocks );

        std::copy( &device_memory[ address - flash_start_addr ], &device_memory[ address - flash_start_addr + size ], destination );
    }

    std::uint32_t checksum32( std::uintptr_t start_addr, std::size_t size )
    {
        std::uint32_t result = 0;

        for ( std::uintptr_t i = start_addr; i != start_addr + size; ++i )
        {
            result += device_memory.at( i - flash_start_addr );
        }

        return result;
    }

    std::uint32_t checksum32( const std::uint8_t* start_addr, std::size_t size, std::uint32_t result )
    {
        for ( ; size; ++start_addr, --size )
        {
            result += *start_addr;
        }

        return result;
    }

    std::uint32_t checksum32( std::uintptr_t start_addr )
    {
        std::uint32_t result = 0;

        while ( start_addr )
        {
            result += start_addr & 0xff;
            start_addr = start_addr >> 8;
        }

        return result;
    }

    bluetoe::bootloader::error_codes start_flash( std::uintptr_t address, const std::uint8_t* values, std::size_t size )
    {
        start_flash_address = address;
        start_flash_content.insert( start_flash_content.end(), values, values + size );

        std::copy( values, values + size, &device_memory[ address - flash_start_addr ] );

        return bluetoe::bootloader::error_codes::success;
    }

    bluetoe::bootloader::error_codes run( std::uintptr_t start_addr )
    {
        start_program_called = start_addr;

        return bluetoe::bootloader::error_codes::success;
    }

    bluetoe::bootloader::error_codes reset()
    {
        reset_called = true;

        return bluetoe::bootloader::error_codes::success;
    }

    handler()
        : start_flash_address( 0x1234 )
        , start_program_called( 0 )
        , reset_called( false )
    {
        for ( int b = 0; b != num_blocks; ++b )
        {
            for ( int v = 0; v != block_size; ++v )
                device_memory.push_back( v );
        }

        original_device_memory = device_memory;
    }

    std::vector< std::uint8_t > device_memory;
    std::vector< std::uint8_t > original_device_memory;
    std::uintptr_t              start_flash_address;
    std::vector< std::uint8_t > start_flash_content;
    std::uintptr_t              start_program_called;
    bool                        reset_called;
};

using bootloader_server = bluetoe::server<
    bluetoe::bootloader_service<
        bluetoe::bootloader::page_size< block_size >,
        bluetoe::bootloader::handler< handler >,
        bluetoe::bootloader::white_list<
            bluetoe::bootloader::memory_region< flash_start_addr, flash_start_addr + num_blocks * block_size >,
            bluetoe::bootloader::memory_region< flash_start_addr2, flash_start_addr2 + num_blocks * block_size >
        >
    >
>;

/*
 * GATT Tests
 */
BOOST_FIXTURE_TEST_CASE( service_discoverable_by_uuid, gatt_procedures< bootloader_server > )
{
    BOOST_CHECK_NE(
        ( discover_primary_service_by_uuid< bluetoe::bootloader::service_uuid >() ),
        discovered_service( 0, 0 ) );
}

template < class Server >
struct services_discovered : gatt_procedures< Server, test_mtu_size >
{
    services_discovered()
        : service( this->template discover_primary_service_by_uuid< bluetoe::bootloader::service_uuid >() )
    {
        BOOST_REQUIRE_NE( service, discovered_service() );
    }

    const discovered_service service;
};

BOOST_FIXTURE_TEST_CASE( characteristics_are_discoverable, services_discovered< bootloader_server > )
{
    BOOST_CHECK_NE(
        discover_characteristic_by_uuid< bluetoe::bootloader::control_point_uuid >( service ),
        discovered_characteristic() );

    BOOST_CHECK_NE(
        discover_characteristic_by_uuid< bluetoe::bootloader::data_uuid >( service ),
        discovered_characteristic() );

    BOOST_CHECK_NE(
        discover_characteristic_by_uuid< bluetoe::bootloader::progress_uuid >( service ),
        discovered_characteristic() );
}

template < class Server >
struct all_discovered : services_discovered< Server >
{
    all_discovered()
        : cp_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::control_point_uuid >( this->service ) )
        , data_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::data_uuid >( this->service ) )
        , progress_char( this->template discover_characteristic_by_uuid< bluetoe::bootloader::progress_uuid >( this->service ) )
    {
        BOOST_REQUIRE_NE( cp_char, discovered_characteristic() );
        BOOST_REQUIRE_NE( data_char, discovered_characteristic() );
        BOOST_REQUIRE_NE( progress_char, discovered_characteristic() );
    }

    const discovered_characteristic cp_char;
    const discovered_characteristic data_char;
    const discovered_characteristic progress_char;
};

BOOST_FIXTURE_TEST_CASE( control_point_properties, all_discovered< bootloader_server > )
{
    BOOST_CHECK_EQUAL( cp_char.properties, 0x1c );
}

BOOST_FIXTURE_TEST_CASE( data_char_properties, all_discovered< bootloader_server > )
{
    BOOST_CHECK_EQUAL( data_char.properties, 0x0c );
}

BOOST_FIXTURE_TEST_CASE( progress_char_properties, all_discovered< bootloader_server > )
{
    BOOST_CHECK_EQUAL( progress_char.properties, 0x10 );
}

BOOST_FIXTURE_TEST_CASE( control_point_subscribe_for_notifications, all_discovered< bootloader_server > )
{
    const auto cccd = discover_cccd( cp_char );
    BOOST_CHECK_NE( cccd, discovered_characteristic_descriptor() );

    l2cap_input({
        0x12, low( cccd.handle ), high( cccd.handle ),
        0x01, 0x00
    });

    expected_result( { 0x13 } );
}

BOOST_FIXTURE_TEST_CASE( progress_subscribe_for_notifications, all_discovered< bootloader_server > )
{
    const auto cccd = discover_cccd( progress_char );
    BOOST_CHECK_NE( cccd, discovered_characteristic_descriptor() );

    l2cap_input({
        0x12, low( cccd.handle ), high( cccd.handle ),
        0x01, 0x00
    });

    expected_result( { 0x13 } );
}

template < class Server >
struct all_discovered_and_subscribed : all_discovered< Server >
{
    all_discovered_and_subscribed()
        : cp_cccd( this->discover_cccd( this->cp_char ) )
        , progress_cccd( this->discover_cccd( this->progress_char ) )
    {
        this->l2cap_input({
            0x12, this->low( cp_cccd.handle ), this->high( cp_cccd.handle ),
            0x01, 0x00
        });
        this->l2cap_input({
            0x12, this->low( progress_cccd.handle ), this->high( progress_cccd.handle ),
            0x01, 0x00
        });
    }

    void add_ptr( std::vector< std::uint8_t >& v, std::uintptr_t p )
    {
        for ( int i = 0; i != sizeof( p ); ++i )
        {
            v.push_back( p & 0xff );
            p = p >> 8;
        }
    }

    const discovered_characteristic_descriptor    cp_cccd;
    const discovered_characteristic_descriptor    progress_cccd;
};

/*
 * Get Version
 */
BOOST_FIXTURE_TEST_CASE( get_version, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x00 }, connection );
    expected_result( { 0x13 } );

    expected_output( notification, {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),    // notification
        0x00,                                                               // response code
        0x47, 0x11                                                          // version as given by handler::get_version()
    } );
}

/*
 * Get CRC
 */
BOOST_FIXTURE_TEST_CASE( get_crc, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1000 );
    add_ptr( input, 0x1002 );

    l2cap_input( input, connection );

    expected_result( { 0x13 } );

    const std::uint32_t expected_checksum = device_memory[ 0 ] + device_memory[ 1 ];

    expected_output( notification, {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),    // notification
        0x01,                                                               // response code
        static_cast< std::uint8_t >( expected_checksum & 0xff ),
        static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ), 0x00, 0x00
    } );
}

BOOST_FIXTURE_TEST_CASE( get_crc_wrong_addresses, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1002 );
    add_ptr( input, 0x1000 );

    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12,
        low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07 } );

    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( get_crc_start_address_out_of_range, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x0fff );
    add_ptr( input, 0x1002 );

    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12,
        low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07 } );

    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( get_crc_end_address_out_of_range, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1000 );
    add_ptr( input, 0x1401 );

    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12,
        low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07 } );

    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( get_crc_end_full_range, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1000 );
    add_ptr( input, 0x1400 );

    l2cap_input( input, connection );

    expected_result( { 0x13 } );

    BOOST_CHECK( notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( get_crc_over_different_out_of_range, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x01 };

    add_ptr( input, 0x1000 );
    add_ptr( input, 0x2000 );

    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12,
        low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07 } );

    BOOST_CHECK( !notification.valid() );
}

/*
 * Get Sizes
 */
BOOST_FIXTURE_TEST_CASE( get_sizes, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x02 }, connection );

    expected_result( { 0x13 } );

    expected_output( notification, {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),    // notification
        0x02,                                                               // response code
        sizeof(std::uint8_t*),                                              // Address size
        block_size & 0xff, block_size >> 8, 0, 0,                           // Size of a Page
        0x02, 0, 0, 0                                                       // Number of pages the bootloader can buffer
    } );
}

/*
 * Start Flash
 */
BOOST_FIXTURE_TEST_CASE( flash_address_wrong_ptr_size, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x03, 0x12 }, connection );

    expected_result( {
        0x01, 0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x0d } ); // Invalid Attribute Value Length

    // no notification expected
    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( flash_address_out_of_range, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x03 };

    add_ptr( input, 0x0 );
    l2cap_input( input, connection );

    expected_result( {
        0x01, 0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07 } ); // invalid Offset

    // no notification expected
    BOOST_CHECK( !notification.valid() );
}

BOOST_FIXTURE_TEST_CASE( flash_address, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x03 };

    add_ptr( input, 0x1002 );
    l2cap_input( input, connection );

    expected_result( { 0x13 } );

    expected_output( notification, {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),    // notification
        0x03,                                                               // response code
        0x17,                                                               // MTU
        0x12, 0x00, 0x00, 0x00                                              // checksum over start address = 0x10 + 0x02
    } );
}

/*
 * Flush
 */
BOOST_FIXTURE_TEST_CASE( flush_when_not_in_flash_mode, all_discovered_and_subscribed< bootloader_server > )
{
    check_error_response( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x05 },
        0x12, cp_char.value_handle, 0x82 );
}

template < class Server, std::size_t StartAddress = flash_start_addr >
struct start_flash : all_discovered_and_subscribed< Server >
{
    start_flash()
        : written_memory( this->original_device_memory )
    {
        start_flash_procedure( StartAddress );
    }

    void start_flash_procedure( std::size_t start_address )
    {
        address = start_address;
        std::vector< std::uint8_t > input = {
            0x12, this->low( this->cp_char.value_handle ), this->high( this->cp_char.value_handle ),
            0x03 };

        this->add_ptr( input, start_address );

        this->l2cap_input( input, this->connection );

        this->expected_result( { 0x13 } );

        BOOST_REQUIRE( this->notification.valid() );
        this->notification.clear();
    }

    void write_to_data_char( const std::initializer_list< std::uint8_t > data )
    {
        write_to_data_char( data.begin(), data.size() );
    }

    void write_to_data_char( const std::uint8_t* data, std::size_t size )
    {
        while ( size )
        {
            std::size_t transfer_size = std::min( test_mtu_size, size );

            std::vector< std::uint8_t > output = {
                0x12, this->low( this->data_char.value_handle ), this->high( this->data_char.value_handle )
            };
            output.insert( output.end(), data, data + transfer_size );

            this->l2cap_input( output, this->connection );
            this->expected_result( { 0x13 } );
            size -= transfer_size;

            // use at() not std::copy to find runtime errors
            for ( ; transfer_size; ++data, --transfer_size, ++address )
            {
                written_memory.at( address - flash_start_addr ) = *data;
            }
        }

    }

    void write_random_to_data_char( std::size_t size )
    {
        std::vector< std::uint8_t > random_numbers;

        for (; size; --size )
            random_numbers.push_back( random_() & 0xff );

        write_to_data_char( &random_numbers[ 0 ], random_numbers.size() );
    }

    std::vector< std::uint8_t > written_memory;
    std::size_t                 address;
    std::mt19937                random_;
};

template < class Server >
struct write_3_bytes_at_the_beginning_of_the_flash : start_flash< Server >
{
    write_3_bytes_at_the_beginning_of_the_flash()
    {
        this->l2cap_input( {
            0x12, this->low( this->data_char.value_handle ), this->high( this->data_char.value_handle ),
            0x0a, 0x0b, 0x0c
        }, this->connection );

        this->expected_result( { 0x13 } );
    }
};

BOOST_FIXTURE_TEST_CASE( flush_notification_after_flashing, write_3_bytes_at_the_beginning_of_the_flash< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x05
    }, connection );

    expected_result( { 0x13 } );

    const std::uint32_t expected_checksum = checksum32( flash_start_addr )
        + 0x0a + 0x0b + 0x0c;

    expected_output< bluetoe::bootloader::control_point_uuid >( {
        0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x05,
        static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
        static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
        static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
        static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
        0x00, 0x00,                             // consecutive number
    } );

    // the flashed memory should be a whole page and the content should be equal to the original content
    // exept for the 3 bytes, written at the beginning
    BOOST_CHECK_EQUAL( start_flash_address, flash_start_addr );

    static constexpr std::uint8_t expected_flash_content[] = { 0x0a, 0x0b, 0x0c };

    std::copy( std::begin( expected_flash_content ), std::end( expected_flash_content ),
        original_device_memory.begin() );

    BOOST_CHECK_EQUAL( start_flash_content.size(), block_size );
    BOOST_CHECK_EQUAL_COLLECTIONS( original_device_memory.begin(), original_device_memory.begin() + start_flash_content.size(),
        start_flash_content.begin(), start_flash_content.end() );

    // now, when signaling the end of the flash process, we get a notification
    end_flash( *this );

    expected_output< bluetoe::bootloader::progress_uuid >( {
        0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
        static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
        static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
        static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
        static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
        0x00, 0x00,                             // consecutive number
        0x17                                    // MTU
   } );
}

/*
 * Start
 */
BOOST_FIXTURE_TEST_CASE( start_program, all_discovered_and_subscribed< bootloader_server > )
{
    std::vector< std::uint8_t > input = {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x06 };

    add_ptr( input, 0xabcd1234 );
    l2cap_input( input, connection );

    BOOST_CHECK_EQUAL( start_program_called, 0xabcd1234 );
}

/*
 * Reset
 */
BOOST_FIXTURE_TEST_CASE( reset_bootloader, all_discovered_and_subscribed< bootloader_server > )
{
    l2cap_input( {
        0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
        0x07
    }, connection );

    BOOST_CHECK( reset_called );
}

BOOST_AUTO_TEST_SUITE( flashing_data )

    using write_at_250 = start_flash< bootloader_server, flash_start_addr + 250 >;
    BOOST_FIXTURE_TEST_CASE( flash_data_at_end_of_block, write_at_250 )
    {
        write_to_data_char( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );

        BOOST_CHECK( !notification.valid() );

        // the first block must be completly written
        BOOST_CHECK_EQUAL( start_flash_content.size(), block_size );
        BOOST_CHECK_EQUAL( start_flash_address, flash_start_addr );

        BOOST_CHECK_EQUAL_COLLECTIONS( written_memory.begin(), written_memory.begin() + block_size,
            start_flash_content.begin(), start_flash_content.end() );

        // now, when signaling the end of the flash process, we get a notification
        end_flash( *this );

        const std::uint32_t expected_checksum = checksum32( flash_start_addr + 250 )
            + 0x01 + 0x02 + 0x03 + 0x04 + 0x05 + 0x06;

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
            0x00, 0x00,                             // consecutive number
            0x17                                    // MTU
       } );
    }

    using write_at_250 = start_flash< bootloader_server, flash_start_addr + 250 >;
    BOOST_FIXTURE_TEST_CASE( flash_data_over_the_end_of_a_block, write_at_250 )
    {
        write_to_data_char( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );
        write_random_to_data_char( block_size );

        // now both buffers are filled
        check_error_response( {
            0x12, low( data_char.value_handle ), high( data_char.value_handle ),
            0x07
        }, 0x12, data_char.value_handle, 0x83 );
    }

    BOOST_FIXTURE_TEST_CASE( flash_data_after_a_filled_block, start_flash< bootloader_server > )
    {
        write_random_to_data_char( block_size );
        write_random_to_data_char( block_size - 1 );

        // now, there is only room for a single byte
        check_error_response( {
            0x12, low( data_char.value_handle ), high( data_char.value_handle ),
            0x07, 0x08
        }, 0x12, data_char.value_handle, 0x83 );
    }

    BOOST_FIXTURE_TEST_CASE( write_4_blocks, start_flash< bootloader_server > )
    {
        write_random_to_data_char( block_size );
        write_random_to_data_char( block_size );

        end_flash( *this );

        const std::uint32_t expected_checksum =
            checksum32( &written_memory[ 0 ], block_size, checksum32( flash_start_addr ) );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
            0x00, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block2 =
            checksum32( &written_memory[ 1 * block_size ], block_size, expected_checksum );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block2 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 24 ) & 0xff ),
            0x01, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        // last block to write
        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block3 =
            checksum32( &written_memory[ 2 * block_size ], block_size, expected_checksum_block2 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block3 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 24 ) & 0xff ),
            0x02, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        end_flash( *this );

        const std::uint32_t expected_checksum_block4 =
            checksum32( &written_memory[ 3 * block_size ], block_size, expected_checksum_block3 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block4 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 24 ) & 0xff ),
            0x03, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            written_memory.begin(), written_memory.end(),
            device_memory.begin(), device_memory.end() );
    }

    BOOST_FIXTURE_TEST_CASE( write_4_blocks_fast_flash, start_flash< bootloader_server > )
    {
        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum =
            checksum32( &written_memory[ 0 ], block_size, checksum32( flash_start_addr ) );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
            0x00, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block2 =
            checksum32( &written_memory[ 1 * block_size ], block_size, expected_checksum );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block2 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 24 ) & 0xff ),
            0x01, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block3 =
            checksum32( &written_memory[ 2 * block_size ], block_size, expected_checksum_block2 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block3 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 24 ) & 0xff ),
            0x02, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block4 =
            checksum32( &written_memory[ 3 * block_size ], block_size, expected_checksum_block3 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block4 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 24 ) & 0xff ),
            0x03, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            written_memory.begin(), written_memory.end(),
            device_memory.begin(), device_memory.end() );
    }

    BOOST_FIXTURE_TEST_CASE( write_4_blocks_over_block_ends, start_flash< bootloader_server > )
    {
        write_random_to_data_char( block_size + 4 );
        end_flash( *this );

        const std::uint32_t expected_checksum =
            checksum32( &written_memory[ 0 ], block_size, checksum32( flash_start_addr ) );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
            0x00, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block2 =
            checksum32( &written_memory[ 1 * block_size ], block_size, expected_checksum );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block2 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block2 >> 24 ) & 0xff ),
            0x01, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size );
        end_flash( *this );

        const std::uint32_t expected_checksum_block3 =
            checksum32( &written_memory[ 2 * block_size ], block_size, expected_checksum_block2 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block3 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block3 >> 24 ) & 0xff ),
            0x02, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        write_random_to_data_char( block_size - 4 );
        end_flash( *this );

        const std::uint32_t expected_checksum_block4 =
            checksum32( &written_memory[ 3 * block_size ], block_size, expected_checksum_block3 );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum_block4 & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum_block4 >> 24 ) & 0xff ),
            0x03, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );

        BOOST_CHECK_EQUAL_COLLECTIONS(
            written_memory.begin(), written_memory.end(),
            device_memory.begin(), device_memory.end() );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE( stop_flash )

    BOOST_FIXTURE_TEST_CASE( stop_flash, all_discovered_and_subscribed< bootloader_server > )
    {
         l2cap_input( {
            0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
            0x04 }, connection );

        expected_result( { 0x13 } );

        expected_output( notification, {
            0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),
            0x04 } );
    }

    BOOST_FIXTURE_TEST_CASE( stop_flash_while_in_flash_mode, start_flash< bootloader_server > )
    {
        l2cap_input( {
            0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
            0x04 }, connection );

        expected_result( { 0x13 } );

        expected_output( notification, {
            0x1b, low( cp_char.value_handle ), high( cp_char.value_handle ),
            0x04 } );
    }

    BOOST_FIXTURE_TEST_CASE( flashing_after_stopping, start_flash< bootloader_server > )
    {
        write_to_data_char( { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } );

        l2cap_input( {
            0x12, low( cp_char.value_handle ), high( cp_char.value_handle ),
            0x04 }, connection );

        expected_result( { 0x13 } );
        written_memory = original_device_memory;

        start_flash_procedure( flash_start_addr + block_size / 2 );
        write_random_to_data_char( block_size / 2 );
        end_flash( *this );

        const std::uint32_t expected_checksum =
            checksum32( &written_memory[ block_size / 2 ], block_size / 2, checksum32( flash_start_addr + block_size / 2 ) );

        expected_output< bluetoe::bootloader::progress_uuid >( {
            0x1b, low( progress_char.value_handle ), high( progress_char.value_handle ),
            static_cast< std::uint8_t >( expected_checksum & 0xff ),               // checksum
            static_cast< std::uint8_t >( ( expected_checksum >> 8 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 16 ) & 0xff ),
            static_cast< std::uint8_t >( ( expected_checksum >> 24 ) & 0xff ),
            0x00, 0x00,                             // consecutive number
            0x17                                    // MTU
        } );
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_CASE( write_page_without_command, all_discovered_and_subscribed< bootloader_server > )
{
    check_error_response( {
        0x12, low( data_char.value_handle ), high( data_char.value_handle ),
        0x00, 0x01, 0x02, 0x03 },
        0x12, data_char.value_handle, 0x80 );
}
