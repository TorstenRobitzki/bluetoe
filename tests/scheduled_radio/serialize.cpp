#include "serialize.hpp"

#include <cassert>

static constexpr std::uint16_t max_size = 256;
static constexpr std::size_t   num_write_buffers = 2;
static constexpr std::size_t   num_read_buffers = 1;

static std::size_t next_write_buffer = 0;
static std::size_t next_read_buffer = 0;

static std::uint8_t write_buffers_[ num_write_buffers ][ max_size ];
static std::uint8_t read_buffers_[ num_read_buffers ][ max_size ];

std::uint8_t* allocate_local_write_buffer( [[maybe_unused]] std::uint16_t length )
{
    assert( length <= max_size );
    std::uint8_t* result = write_buffers_[ next_write_buffer ];
    next_write_buffer = ( next_write_buffer + 1 ) % num_write_buffers;

    return result;
}

std::uint8_t* allocate_local_read_buffer( [[maybe_unused]] std::uint16_t length )
{
    assert( length <= max_size );
    std::uint8_t* result = read_buffers_[ next_read_buffer ];
    next_read_buffer = ( next_read_buffer + 1 ) % num_read_buffers;

    return result;
}
