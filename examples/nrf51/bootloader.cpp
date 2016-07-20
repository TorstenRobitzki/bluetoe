#include <bluetoe/services/bootloader.hpp>
#include <bluetoe/server.hpp>
#include <bluetoe/bindings/nrf51.hpp>

static constexpr std::size_t flash_page_size = 1024;
namespace bb = bluetoe::bootloader;

class flash_handler {
public:
    bb::error_codes start_flash( std::uintptr_t address, const std::uint8_t* values, std::size_t size );
    bb::error_codes run( std::uintptr_t start_addr );
    bb::error_codes reset();
    std::pair< const std::uint8_t*, std::size_t > get_version();
    void read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination );
    std::uint32_t checksum32( std::uintptr_t start_addr, std::size_t size );
    std::uint32_t checksum32( const std::uint8_t* start_addr, std::size_t size, std::uint32_t old_crc );
    std::uint32_t checksum32( std::uintptr_t start_addr );
};

using gatt_definition = bluetoe::server<
    bluetoe::bootloader_service<
        bb::handler< flash_handler >,
        bb::page_size< flash_page_size >,
        bb::white_list<
            bb::memory_region< 16 * flash_page_size, 256 * flash_page_size >
        >
    >
>;

/*
 * Change the default device address slightly, so that clients do not get confused by devices that
 * have the very same address, but totally different GATT structures
 */
struct address_generator {
    static constexpr bool is_random()
    {
        return true;
    }

    template < class Radio >
    static bluetoe::link_layer::random_device_address address( const Radio& r )
    {
        return bluetoe::link_layer::address::generate_static_random_address( ~r.static_random_address_seed() );
    }

    typedef bluetoe::link_layer::details::device_address_meta_type meta_type;
};

gatt_definition gatt_server;

bluetoe::nrf51<
    gatt_definition,
    bluetoe::link_layer::buffer_sizes< flash_page_size * 3, flash_page_size * 3 >,
    address_generator
> link_layer;

int main()
{
    for ( ;; )
        link_layer.run( gatt_server );
}

bb::error_codes flash_handler::start_flash( std::uintptr_t address, const std::uint8_t* values, std::size_t size )
{
    bb::end_flash( gatt_server );

    return bb::error_codes::success;
}

bb::error_codes flash_handler::run( std::uintptr_t start_addr )
{
    return bb::error_codes::success;
}

bb::error_codes flash_handler::reset()
{
    return bb::error_codes::success;
}

std::pair< const std::uint8_t*, std::size_t > flash_handler::get_version()
{
    static const std::uint8_t version[] = "ble_flash 1.0";

    return std::pair< const std::uint8_t*, std::size_t >( &version[ 0 ], sizeof( version ) -1 );
}

void flash_handler::read_mem( std::uintptr_t address, std::size_t size, std::uint8_t* destination )
{
}

std::uint32_t flash_handler::checksum32( std::uintptr_t start_addr, std::size_t size )
{
    return 42u;
}

std::uint32_t flash_handler::checksum32( const std::uint8_t* start_addr, std::size_t size, std::uint32_t old_crc )
{
    return 42u;
}

std::uint32_t flash_handler::checksum32( std::uintptr_t start_addr )
{
    return 42u;
}
