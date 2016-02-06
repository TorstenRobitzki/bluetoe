#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/codes.hpp>

std::uint8_t read_blob_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
{
    if ( offset == 0 )
    {
        *out_buffer = 42;
        out_size    = 1;

        return bluetoe::error_codes::success;
    }

    return bluetoe::error_codes::invalid_offset;
}

std::uint8_t read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
{
    *out_buffer = 42;
    out_size    = 1;

    return bluetoe::error_codes::success;
}

std::uint8_t write_blob_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value )
{
    return bluetoe::error_codes::write_not_permitted;
}

std::uint8_t write_handler( std::size_t write_size, const std::uint8_t* value )
{
    return bluetoe::error_codes::write_not_permitted;
}

struct static_handler {
    static constexpr std::uint8_t code_word[] = { 'a', 'b', 'c' };

    static std::uint8_t read( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        if ( offset > sizeof( code_word ) )
            return bluetoe::error_codes::invalid_offset;

        out_size = std::min( read_size, sizeof( code_word ) - offset );
        std::copy( &code_word[ offset ], &code_word[ offset + out_size], out_buffer );

        return bluetoe::error_codes::success;
    }

    static std::uint8_t write( std::size_t offset, std::size_t write_size, const std::uint8_t* value )
    {
        return bluetoe::error_codes::write_not_permitted;
    }
};


struct handler {
    std::uint8_t read_blob( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        return bluetoe::error_codes::success;
    }

    std::uint8_t write_blob( std::size_t offset, std::size_t write_size, const std::uint8_t* value )
    {
        return bluetoe::error_codes::write_not_permitted;
    }

    std::uint8_t read_blob_c( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const;
    std::uint8_t read_blob_v( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) volatile;
    std::uint8_t read_blob_vc( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const volatile;
} handler_instance;

bluetoe::server<
    bluetoe::service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::characteristic<
            bluetoe::free_read_blob_handler< &read_blob_handler >,
            bluetoe::free_write_blob_handler< &write_blob_handler >
        >,
        bluetoe::characteristic<
            bluetoe::free_read_handler< &read_handler >,
            bluetoe::free_raw_write_handler< &write_handler >
        >,
        bluetoe::characteristic<
            bluetoe::free_read_blob_handler< &static_handler::read >,
            bluetoe::free_write_blob_handler< &static_handler::write >
        >,
        bluetoe::characteristic<
            bluetoe::read_blob_handler< handler, handler_instance, &handler::read_blob >,
            bluetoe::write_blob_handler< handler, handler_instance, &handler::write_blob >
        >
    >
> gatt_server;