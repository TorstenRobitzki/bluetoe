#ifndef BLUETOE_CODES_HPP
#define BLUETOE_CODES_HPP

#include <cstdint>

namespace bluetoe
{
    enum class att_opcodes : std::uint8_t {
        error_response = 0x01,
        find_information_request = 0x04,
        find_information_response = 0x05,
    };

    inline std::uint8_t bits( att_opcodes c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class att_error_codes : std::uint8_t {
        invalid_handle                      = 0x01,
        read_not_permitted,
        write_not_permitted,
        invalid_pdu,
        insufficient_authentication,
        request_not_supported,
        invalid_offset,
        insufficient_authorization,
        prepare_queue_full,
        attribute_not_found,
        attribute_not_long,
        insufficient_encryption_key_size,
        invalid_attribute_value_length,
        unlikely_error,
        insufficient_encryption,
        unsupported_group_type,
        insufficient_resources
    };

    inline std::uint8_t bits( att_error_codes c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class att_uuid_format : std::uint8_t {
        short_16bit = 0x01,
        long_128bit = 0x02
    };

    inline std::uint8_t bits( att_uuid_format c )
    {
        return static_cast< std::uint8_t >( c );
    }
}

#endif