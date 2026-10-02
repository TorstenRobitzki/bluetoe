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
        tx_power_level                  = 0x0a,
        appearance                      = 0x19,
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
 * namespace for error codes, that should be convertable to uint8_t
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

/**
 * namespace for controller error codes, that should be convertable to uint8_t
 */
namespace controller_error_codes {
    enum error_codes : std::uint8_t
    {
        success,
        unknown_hci_command,
        unknown_connection_identifier,
        hardware_failure,
        page_timeout,
        authentication_failure,
        pin_or_key_missing,
        memory_capacity_exceeded,
        connection_timeout,
        connection_limit_exceeded,
        synchronous_connection_limit_to_a_device_exceeded,
        connection_already_exists,
        command_disallowed,
        connection_rejected_due_to_limited_resources,
        connection_rejected_due_to_security_reasons,
        connection_rejected_due_to_unacceptable_bd_addr,
        connection_accept_timeout_exceeded,
        unsupported_feature_or_parameter_value,
        invalid_hci_command_parameters,
        remote_user_terminated_connection,
        remote_device_terminated_connection_due_to_low_resources,
        remote_device_terminated_connection_due_to_power_off,
        connection_terminated_by_local_host,
        repeated_attempts,
        pairing_not_allowed,
        unknown_lmp_pdu,
        unsupported_remote_feature,
        sco_offset_rejected,
        sco_interval_rejected,
        sco_air_mode_rejected,
        invalid_lmp_parameters,
        invalid_ll_parameters = invalid_lmp_parameters,
        unspecified_error,
        unsupported_lmp_parameter_value,
        unsupported_ll_parameter_value = unsupported_lmp_parameter_value,
        role_change_not_allowed,
        lmp_response_timeout_ll_response_timeout,
        lmp_error_transaction_collision_ll_procedure_collision,
        lmp_pdu_not_allowed,
        encryption_mode_not_acceptable,
        link_key_cannot_be_changed,
        requested_qos_not_supported,
        instant_passed,
        pairing_with_unit_key_not_supported,
        different_transaction_collision,
        reserved_for_future_use1,
        qos_unacceptable_parameter,
        qos_rejected,
        channel_classification_not_supported,
        insufficient_security,
        parameter_out_of_mandatory_range,
        reserved_for_future_use2,
        role_switch_pending,
        reserved_for_future_use3,
        reserved_slot_violation,
        role_switch_failed,
        extended_inquiry_response_too_large,
        secure_simple_pairing_not_supported_by_host,
        host_busy_pairing,
        connection_rejected_due_to_no_suitable_channel_found,
        controller_busy,
        unacceptable_connection_parameters,
        advertising_timeout,
        connection_terminated_due_to_mic_failure,
        connection_failed_to_be_established_synchronization_timeout,
        previously_used,
        coarse_clock_adjustment_rejected_but_will_try_to_adjust_using_clock_dragging,
        type0_submap_not_defined,
        unknown_advertising_identifier,
        limit_reached,
        operation_cancelled_by_host,
        packet_too_long,
        too_late,
        too_early,
        insufficient_channels,
    };
}

}
#endif
