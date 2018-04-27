#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>

#include <bluetoe/codes.hpp>

namespace bluetoe {
    namespace details {
        struct security_manager_meta_type {};

        enum class sm_error_codes : std::uint8_t {
            passkey_entry_failed        = 0x01,
            oob_not_available           = 0x02,
            authentication_requirements = 0x03,
            confirm_value_failed        = 0x04,
            pairing_not_supported       = 0x05,
            encryption_key_size         = 0x06,
            command_not_supported       = 0x07,
            unspecified_reason          = 0x08,
            repeated_attempts           = 0x09,
            invalid_parameters          = 0x0a,
            dhkey_check_failed          = 0x0b,
            numeric_comparison_failed   = 0x0c,
            br_edr_pairing_in_progress  = 0x0d,
            crosstransport_key_derivation_generation_not_allowed = 0x0e
        };

        enum class sm_opcodes : std::uint8_t {
            pairing_request          = 0x01,
            pairing_response,
            pairing_confirm,
            pairing_random,
            pairing_failed,
            encryption_information,
            master_identification,
            identity_information,
            identity_address_information,
            signing_information,
            security_request,
            pairing_public_key,
            pairing_dhkey_check,
            pairing_keypress_notification
        };

        enum class io_capabilities : std::uint8_t {
            display_only,
            display_yes_no,
            keyboard_only,
            no_input_no_output,
            keyboard_display,
            last = keyboard_display
        };

        inline void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
        {
            output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_failed );
            output[ 1 ] = static_cast< std::uint8_t >( error_code );

            out_size = 2;
        }

    }

    /**
     * @brief Security manager implementation.
     */
    class security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < class SecurityFunctions >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, SecurityFunctions& );

        typedef details::security_manager_meta_type meta_type;
    private:
        static constexpr std::uint8_t   min_max_key_size = 7;
        static constexpr std::uint8_t   max_max_key_size = 16;

        void handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );
        void create_pairing_response( std::uint8_t* output, std::size_t& out_size );
        /** @endcond */
    };

    /**
     * @brief current default implementation of the security manager, that actievly rejects every pairing attempt.
     */
    class no_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < class SecurityFunctions >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, SecurityFunctions& );

        typedef details::security_manager_meta_type meta_type;
        /** @endcond */
    };


    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < class SecurityFunctions >
    void security_manager::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, SecurityFunctions& )
    {
        using namespace bluetoe::details;

        // is there at least an opcode?
        if ( in_size == 0 )
            return error_response( sm_error_codes::invalid_parameters, output, out_size );

        assert( in_size != 0 );
        assert( out_size >= default_att_mtu_size );

        const sm_opcodes opcode = static_cast< sm_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
            case sm_opcodes::pairing_request:
                handle_pairing_request( input, in_size, output, out_size );
                break;
            default:
                error_response( sm_error_codes::command_not_supported, output, out_size );
        }
    }

    inline void security_manager::handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        using namespace details;

        static constexpr std::size_t    request_size = 7;

        if ( in_size != request_size )
            return error_response( sm_error_codes::invalid_parameters, output, out_size );

        const std::uint8_t io_capability                = input[ 1 ];
        const std::uint8_t oob_data_flag                = input[ 2 ];
        const std::uint8_t auth_req                     = input[ 3 ];
        const std::uint8_t max_key_size                 = input[ 4 ];
        const std::uint8_t initiator_key_distribution   = input[ 5 ];
        const std::uint8_t responder_key_distribution   = input[ 6 ];

        if (
            ( io_capability > static_cast< std::uint8_t >( io_capabilities::last ) )
         || ( oob_data_flag & ~0x01 )
         || ( auth_req & 0xC0 )
         || ( max_key_size < min_max_key_size || max_key_size > max_max_key_size )
         || ( initiator_key_distribution & 0xf0 )
         || ( responder_key_distribution & 0xf0 )
        )
        {
            return error_response( sm_error_codes::invalid_parameters, output, out_size );
        }

        create_pairing_response( output, out_size );
    }

    inline void security_manager::create_pairing_response( std::uint8_t* output, std::size_t& out_size )
    {
        using namespace details;
        static constexpr std::size_t    response_size = 7;

        out_size = response_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_response );
        output[ 1 ] = static_cast< std::uint8_t >( io_capabilities::no_input_no_output );
        output[ 2 ] = 0;
        output[ 3 ] = 0;
        output[ 4 ] = max_max_key_size;
        output[ 5 ] = 0;
        output[ 6 ] = 0;
    }

    template < class SecurityFunctions >
    void no_security_manager::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size, SecurityFunctions& )
    {
        error_response( details::sm_error_codes::pairing_not_supported, output, out_size );
    }

    /** @endcond */
}
#endif
