#ifndef BLUETOE_CODES_HPP
#define BLUETOE_CODES_HPP

#include <cstdint>

namespace bluetoe {
namespace details {

    static constexpr std::uint16_t default_att_mtu_size = 23;
    static constexpr std::uint16_t default_lesc_mtu_size = 65;

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
        read_multiple_request       = 0x0E,
        read_multiple_response      = 0x0F,
        read_by_group_type_request  = 0x10,
        read_by_group_type_response = 0x11,
        write_request               = 0x12,
        write_response              = 0x13,
        prepare_write_request       = 0x16,
        prepare_write_response      = 0x17,
        execute_write_request       = 0x18,
        execute_write_response      = 0x19,
        write_command               = 0x52,
        notification                = 0x1B,
        indication                  = 0x1D,
        confirmation                = 0x1E

    };

    constexpr std::uint8_t bits( att_opcodes c )
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

    constexpr std::uint8_t bits( att_error_codes c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class att_uuid_format : std::uint8_t {
        short_16bit = 0x01,
        long_128bit = 0x02
    };

    constexpr std::uint8_t bits( att_uuid_format c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class gatt_uuids : std::uint16_t {
        primary_service                     = 0x2800,
        secondary_service                   = 0x2801,
        include                             = 0x2802,
        characteristic                      = 0x2803,
        characteristic_user_description     = 0x2901,
        client_characteristic_configuration = 0x2902,

        internal_128bit_uuid    = 1
    };

    constexpr std::uint16_t bits( gatt_uuids c )
    {
        return static_cast< std::uint16_t >( c );
    }

    enum class gatt_characteristic_properties : std::uint8_t {
        read                    = 0x02,
        write_without_response  = 0x04,
        write                   = 0x08,
        notify                  = 0x10,
        indicate                = 0x20
    };

    constexpr std::uint8_t bits( gatt_characteristic_properties c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum class gap_types : std::uint8_t {
        flags                           = 0x01,
        incomplete_service_uuids_16     = 0x02,
        complete_service_uuids_16       = 0x03,
        incomplete_service_uuids_128    = 0x06,
        complete_service_uuids_128      = 0x07,
        complete_local_name             = 0x09,
        shortened_local_name            = 0x08,
    };

    constexpr std::uint8_t bits( gap_types c )
    {
        return static_cast< std::uint8_t >( c );
    }

    enum {
        client_characteristic_configuration_notification_enabled = 1,
        client_characteristic_configuration_indication_enabled   = 2
    };

    inline std::uint8_t* write_opcode( std::uint8_t* out, details::att_opcodes opcode )
    {
        *out = bits( opcode );
        return out + 1;
    }
}


/**
 * namespace for error codes, that should be convertable to int
 */
namespace error_codes {

    /**
     * @brief Error codes to be returned by read and write handlers for characteristic values
     */
    enum error_codes : std::uint8_t {
        /**
         * read or write request could be fulfilled without an error
         */
        success                             = 0x00,

        /**
         * The attribute handle given was not valid on this server.
         */
        invalid_handle                      = 0x01,

        /**
         * The attribute cannot be read.
         */
        read_not_permitted,

        /**
         * The attribute cannot be written.
         */
        write_not_permitted,

        /**
         * The attribute PDU was invalid.
         */
        invalid_pdu,

        /**
         * The attribute requires authentication before it can be read or written.
         */
        insufficient_authentication,

        /**
         * Attribute server does not support the request received from the client.
         */
        request_not_supported,

        /**
         * Offset specified was past the end of the attribute.
         */
        invalid_offset,

        /**
         * The attribute requires authorization before it can be read or written.
         */
        insufficient_authorization,

        /**
         * Too many prepare writes have been queued.
         */
        prepare_queue_full,

        /**
         * No attribute found within the given attri- bute handle range.
         */
        attribute_not_found,

        /**
         * The attribute cannot be read or written using the Read Blob Request.
         */
        attribute_not_long,

        /**
         * The Encryption Key Size used for encrypting this link is insufficient.
         */
        insufficient_encryption_key_size,

        /**
         * The attribute value length is invalid for the operation.
         */
        invalid_attribute_value_length,

        /**
         * The attribute request that was requested has encountered an error that was unlikely,
         * and therefore could not be completed as requested.
         */
        unlikely_error,

        /**
         * The attribute requires encryption before it can be read or written.
         */
        insufficient_encryption,

        /**
         * The attribute type is not a supported grouping attribute as defined by a higher layer specification.
         */
        unsupported_group_type,

        /**
         * Insufficient Resources to complete the request.
         */
        insufficient_resources,

        /**
         * Start of range for application specific error codes
         */
        application_error_start             = 0x80,

        /**
         * Last code of the range for application specific error codes
         */
        application_error_end               = 0x9f,

        /**
         * The Out of Range error code is used when an attribute value is out of range as defined by
         * a profile or service specification.
         */
        out_of_range                        = 0xff,

        /**
         * The Procedure Already in Progress error code is used when a profile or service request cannot
         * be serviced because an operation that has been previously triggered is still in progress.
         */
        procedure_already_in_progress       = 0xfe,

        /**
         * The Client Characteristic Configuration Descriptor Improperly Configured error code is used
         * when a Client Characteristic Configuration descriptor is not configured according to the
         * requirements of the profile or service.
         */
        cccd_improperly_configured          = 0xfd
    };
}
}
#endif
