#ifndef BLUETOE_CODES_HPP
#define BLUETOE_CODES_HPP

#include <cstdint>

namespace bluetoe {
namespace details {

    static constexpr std::uint16_t default_att_mtu_size = 23;

    enum class att_opcodes : std::uint8_t {
        error_response              = 0x01,
        exchange_mtu_request        = 0x02,
        exchange_mtu_response       = 0x03,
        find_information_request    = 0x04,
        find_information_response   = 0x05,
        find_by_type_value_request  = 0x06,
        find_by_type_value_response = 0x07,
        read_by_type_request        = 0x08,
        read_by_type_response       = 0x09,
        read_request                = 0x0A,
        read_response               = 0x0B,
        read_blob_request           = 0x0C,
        read_blob_response          = 0x0D,
        read_by_group_type_request  = 0x10,
        read_by_group_type_response = 0x11,
        write_request               = 0x12,
        write_response              = 0x13

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

    enum class gatt_uuids : std::uint16_t {
        primary_service                     = 0x2800,
        characteristic                      = 0x2803,
        characteristic_user_description     = 0x2901,
        client_characteristic_configuration = 0x2902,

        internal_128bit_uuid    = 1
    };

    inline std::uint16_t bits( gatt_uuids c )
    {
        return static_cast< std::uint16_t >( c );
    }

    enum class gatt_characteristic_properties : std::uint8_t {
        read    = 0x02,
        write   = 0x08,
        notify  = 0x10
    };

    inline std::uint8_t bits( gatt_characteristic_properties c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class gap_types : std::uint8_t {
        flags                   = 0x01,
        complete_local_name     = 0x09,
        shortened_local_name    = 0x08,
    };

    inline std::uint8_t bits( gap_types c )
    {
        return static_cast< std::uint8_t >( c );
    }

}
}
#endif