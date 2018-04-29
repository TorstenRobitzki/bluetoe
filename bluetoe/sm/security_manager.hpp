#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>

#include <bluetoe/codes.hpp>
#include <bluetoe/link_layer/address.hpp>

namespace bluetoe {
    namespace details {
        using uint128_t = std::array< std::uint8_t, 16 >;

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

        enum class pairing_state : std::uint8_t {
            idle,
            pairing_requested,
            pairing_confirmed
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
        template < class OtherConnectionData >
        class connection_data : public OtherConnectionData
        {
        public:
            template < class ... Args >
            connection_data( Args&&... args )
                : OtherConnectionData( args... )
                , state_( details::pairing_state::idle )
            {}

            details::pairing_state state() const
            {
                return state_;
            }

            void remote_connection_created( const bluetoe::link_layer::device_address& remote )
            {
                remote_addr_ = remote;
            }

            const bluetoe::link_layer::device_address& remote_addr() const
            {
                return remote_addr_;
            }

            void pairing_request( const details::uint128_t& srand, const details::uint128_t& p1, const details::uint128_t& p2 )
            {
                assert( state_ == details::pairing_state::idle );
                state_ = details::pairing_state::pairing_requested;
                state_data_.pairing_state.c1_p1 = p1;
                state_data_.pairing_state.c1_p2 = p2;
                state_data_.pairing_state.srand = srand;
            }

            void pairing_confirm( const std::uint8_t* mconfirm_begin, const std::uint8_t* mconfirm_end )
            {
                assert( state_ == details::pairing_state::pairing_requested );
                state_ = details::pairing_state::pairing_confirmed;

                assert( static_cast< std::size_t >( mconfirm_end - mconfirm_begin ) == state_data_.pairing_state.mconfirm.max_size() );
                std::copy( mconfirm_begin, mconfirm_end, state_data_.pairing_state.mconfirm.begin() );
            }

            void error_reset()
            {
                state_ = details::pairing_state::idle;
            }

            const details::uint128_t& c1_p1() const
            {
                return state_data_.pairing_state.c1_p1;
            }

            const details::uint128_t& c1_p2() const
            {
                return state_data_.pairing_state.c1_p2;
            }

            const details::uint128_t& srand() const
            {
                return state_data_.pairing_state.srand;
            }

            const details::uint128_t& mconfirm() const
            {
                return state_data_.pairing_state.mconfirm;
            }

        private:
            bluetoe::link_layer::device_address remote_addr_;
            details::pairing_state              state_;

            union {
                struct {
                    details::uint128_t c1_p1;
                    details::uint128_t c1_p2;
                    details::uint128_t srand;
                    details::uint128_t mconfirm;
                }                                   pairing_state;
            }                       state_data_;
        };

        template < class OtherConnectionData, class SecurityFunctions >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        typedef details::security_manager_meta_type meta_type;
    private:
        static constexpr std::uint8_t   min_max_key_size = 7;
        static constexpr std::uint8_t   max_max_key_size = 16;
        static constexpr std::size_t    pairing_req_resp_size = 7;

        template < class OtherConnectionData, class SecurityFunctions >
        void handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        void create_pairing_response( std::uint8_t* output, std::size_t& out_size );

        template < class OtherConnectionData, class SecurityFunctions >
        void handle_pairing_confirm( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        template < class OtherConnectionData, class SecurityFunctions >
        void handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        template < class OtherConnectionData >
        void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& );

        details::uint128_t c1_p1(
            const std::uint8_t* input, const std::uint8_t* output,
            const bluetoe::link_layer::device_address& initiating_device,
            const bluetoe::link_layer::device_address& responding_device );

        details::uint128_t c1_p2(
            const bluetoe::link_layer::device_address& initiating_device,
            const bluetoe::link_layer::device_address& responding_device );

        /** @endcond */
    };

    /**
     * @brief current default implementation of the security manager, that actievly rejects every pairing attempt.
     */
    class no_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < class OtherConnectionData >
        class connection_data : public OtherConnectionData
        {
        public:
            template < class ... Args >
            connection_data( Args&&... args )
                : OtherConnectionData( args... )
            {}

            void remote_connection_created( const bluetoe::link_layer::device_address& )
            {
            }
        };

        template < class OtherConnectionData, class SecurityFunctions >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        typedef details::security_manager_meta_type meta_type;
        /** @endcond */
    };


    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < class OtherConnectionData, class SecurityFunctions >
    void security_manager::l2cap_input(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace bluetoe::details;

        // is there at least an opcode?
        if ( in_size == 0 )
            return error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        assert( in_size != 0 );
        assert( out_size >= default_att_mtu_size );

        const sm_opcodes opcode = static_cast< sm_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
            case sm_opcodes::pairing_request:
                handle_pairing_request( input, in_size, output, out_size, state, func );
                break;
            case sm_opcodes::pairing_confirm:
                handle_pairing_confirm( input, in_size, output, out_size, state, func );
                break;
            case sm_opcodes::pairing_random:
                handle_pairing_random( input, in_size, output, out_size, state, func );
                break;
            default:
                error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < class OtherConnectionData, class SecurityFunctions >
    void security_manager::handle_pairing_request(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& functions )
    {
        using namespace details;

        if ( in_size != pairing_req_resp_size )
            return error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::pairing_state::idle )
            return error_response( sm_error_codes::unspecified_reason, output, out_size, state );

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
            return error_response( sm_error_codes::invalid_parameters, output, out_size, state );
        }

        create_pairing_response( output, out_size );

        const details::uint128_t srand    = functions.create_srand();
        const details::uint128_t p1       = c1_p1( input, output, state.remote_addr(), functions.local_addr() );
        const details::uint128_t p2       = c1_p2( state.remote_addr(), functions.local_addr() );

        state.pairing_request( srand, p1, p2 );
    }

    inline void security_manager::create_pairing_response( std::uint8_t* output, std::size_t& out_size )
    {
        using namespace details;

        out_size = pairing_req_resp_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_response );
        output[ 1 ] = static_cast< std::uint8_t >( io_capabilities::no_input_no_output );
        output[ 2 ] = 0;
        output[ 3 ] = 0;
        output[ 4 ] = max_max_key_size;
        output[ 5 ] = 0;
        output[ 6 ] = 0;
    }

    template < class OtherConnectionData, class SecurityFunctions >
    void security_manager::handle_pairing_confirm(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace details;

        static constexpr std::size_t    request_size = 17;

        if ( in_size != request_size )
            return error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != pairing_state::pairing_requested )
            return error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        // save mconfirm for later
        state.pairing_confirm( &input[ 1 ], &input[ request_size ] );

        out_size = request_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_confirm );

        const auto sconfirm = func.c1( { { 0 } }, state.srand(), state.c1_p1(), state.c1_p2() );
        std::copy( sconfirm.begin(), sconfirm.end(), &output[ 1 ] );
    }

    template < class OtherConnectionData, class SecurityFunctions >
    void security_manager::handle_pairing_random(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace details;

        static constexpr std::size_t    pairing_random_size = 17;

        if ( in_size != pairing_random_size )
            return error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != pairing_state::pairing_confirmed )
            return error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        uint128_t mrand{};
        std::copy( &input[ 1 ], &input[ pairing_random_size ], mrand.begin() );
        const auto mconfirm = func.c1( { { 0 } }, mrand, state.c1_p1(), state.c1_p2() );

        if ( mconfirm != state.mconfirm() )
            return error_response( sm_error_codes::confirm_value_failed, output, out_size, state );

        out_size = pairing_random_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_random );

        const auto srand = state.srand();
        std::copy( srand.begin(), srand.end(), &output[ 1 ] );
    }

    template < class OtherConnectionData >
    void security_manager::error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state )
    {
        state.error_reset();
        details::error_response( error_code, output, out_size );
    }

    inline details::uint128_t security_manager::c1_p1(
        const std::uint8_t* input, const std::uint8_t* output,
        const bluetoe::link_layer::device_address& initiating_device,
        const bluetoe::link_layer::device_address& responding_device )
    {
        details::uint128_t result{ {
            std::uint8_t( initiating_device.is_random() ? 0x01 : 0x00 ),
            std::uint8_t( responding_device.is_random() ? 0x01 : 0x00 ) } };

        std::copy( input, input + pairing_req_resp_size, &result[ 2 ] );
        std::copy( output, output + pairing_req_resp_size, &result[ 2 + pairing_req_resp_size ] );

        return result;
    }

    inline details::uint128_t security_manager::c1_p2(
        const bluetoe::link_layer::device_address& initiating_device,
        const bluetoe::link_layer::device_address& responding_device )
    {
        static constexpr std::size_t address_size_in_bytes = 6;
        static constexpr std::uint8_t padding = 0;

        details::uint128_t result;

        std::copy( responding_device.begin(), responding_device.end(), result.begin() );
        std::copy( initiating_device.begin(), initiating_device.end(), std::next( result.begin(), address_size_in_bytes ) );
        std::fill( std::next( result.begin(), 2 * address_size_in_bytes ), result.end(), padding );

        return result;
    }


    template < class OtherConnectionData, class SecurityFunctions >
    void no_security_manager::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& )
    {
        error_response( details::sm_error_codes::pairing_not_supported, output, out_size );
    }

    /** @endcond */
}
#endif
