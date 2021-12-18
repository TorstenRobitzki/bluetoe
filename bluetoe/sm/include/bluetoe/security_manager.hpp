#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>

#include <bluetoe/codes.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/link_state.hpp>
#include <bluetoe/pairing_status.hpp>
#include <bluetoe/ll_meta_types.hpp>

namespace bluetoe {

    namespace details {

        using identity_resolving_key_t  = std::array< std::uint8_t, 16 >;
        using uint128_t                 = std::array< std::uint8_t, 16 >;

        using ecdh_public_key_t         = std::array< std::uint8_t, 64 >;
        using ecdh_private_key_t        = std::array< std::uint8_t, 32 >;
        using ecdh_shared_secret_t      = std::array< std::uint8_t, 32 >;

        using io_capabilities_t         = std::array< std::uint8_t, 3 >;

        /**
         * @brief Tuple to store a longterm key along with
         *        EDIV and Rand value to identify them later.
         */
        struct longterm_key_t
        {
            std::array< std::uint8_t, 16 >  longterm_key;
            std::uint64_t                   rand;
            std::uint16_t                   ediv;
        };

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

        enum class legacy_pairing_state : std::uint8_t {
            idle,
            pairing_requested,
            pairing_confirmed,
            pairing_completed,
        };

        enum class lesc_pairing_state : std::uint8_t {
            idle,
            pairing_requested,
            public_keys_exchanged,
            pairing_confirm_send,
            pairing_random_exchanged,
            pairing_completed
        };

        enum class authentication_requirements_flags : std::uint8_t {
            secure_connections      = 0x08
        };

        inline void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
        {
            output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_failed );
            output[ 1 ] = static_cast< std::uint8_t >( error_code );

            out_size = 2;
        }

        template < class OtherConnectionData >
        class legacy_security_connection_data : public OtherConnectionData
        {
        public:
            template < class ... Args >
            legacy_security_connection_data( Args&&... args )
                : OtherConnectionData( args... )
                , state_( details::legacy_pairing_state::idle )
            {}

            details::legacy_pairing_state state() const
            {
                return state_;
            }

            void remote_connection_created( const bluetoe::link_layer::device_address& remote )
            {
                remote_addr_ = remote;
            }

            const bluetoe::link_layer::device_address& remote_address() const
            {
                return remote_addr_;
            }

            void pairing_request( const details::uint128_t& srand, const details::uint128_t& p1, const details::uint128_t& p2 )
            {
                assert( state_ == details::legacy_pairing_state::idle );
                state_ = details::legacy_pairing_state::pairing_requested;
                state_data_.pairing_state.c1_p1 = p1;
                state_data_.pairing_state.c1_p2 = p2;
                state_data_.pairing_state.srand = srand;
            }

            void pairing_confirm( const std::uint8_t* mconfirm_begin, const std::uint8_t* mconfirm_end )
            {
                assert( state_ == details::legacy_pairing_state::pairing_requested );
                state_ = details::legacy_pairing_state::pairing_confirmed;

                assert( static_cast< std::size_t >( mconfirm_end - mconfirm_begin ) == state_data_.pairing_state.mconfirm.max_size() );
                std::copy( mconfirm_begin, mconfirm_end, state_data_.pairing_state.mconfirm.begin() );
            }

            void pairing_completed( const details::uint128_t& short_term_key )
            {
                assert( state_ == details::legacy_pairing_state::pairing_confirmed );
                state_ = details::legacy_pairing_state::pairing_completed;

                state_data_.completed_state.short_term_key = short_term_key;
            }

            template < class T >
            bool outgoing_security_manager_data_available( const bluetoe::details::link_state< T >& link ) const
            {
                return link.is_encrypted() && state_ != details::legacy_pairing_state::pairing_completed;
            }

            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                if ( ediv == 0 && rand == 0 )
                    return { true, state_data_.completed_state.short_term_key };

                return std::pair< bool, details::uint128_t >{};
            }

            void error_reset()
            {
                state_ = details::legacy_pairing_state::idle;
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

            device_pairing_status local_device_pairing_status() const
            {
                return state_ == details::legacy_pairing_state::pairing_completed
                    ? bluetoe::device_pairing_status::unauthenticated_key
                    : bluetoe::device_pairing_status::no_key;
            }

        private:
            bluetoe::link_layer::device_address remote_addr_;
            details::legacy_pairing_state       state_;

            union {
                struct {
                    details::uint128_t c1_p1;
                    details::uint128_t c1_p2;
                    details::uint128_t srand;
                    details::uint128_t mconfirm;
                }                                   pairing_state;

                struct {
                    details::uint128_t short_term_key;
                }                                   completed_state;
            }                       state_data_;
        };

        template < class OtherConnectionData >
        class lesc_security_connection_data : public OtherConnectionData
        {
        public:
            template < class ... Args >
            lesc_security_connection_data( Args&&... args )
                : OtherConnectionData( args... )
                , state_( details::lesc_pairing_state::idle )
            {}

            details::lesc_pairing_state state() const
            {
                return state_;
            }

            void remote_connection_created( const bluetoe::link_layer::device_address& remote )
            {
                remote_addr_ = remote;
            }

            const bluetoe::link_layer::device_address& remote_address() const
            {
                return remote_addr_;
            }

            void error_reset()
            {
                state_ = details::lesc_pairing_state::idle;
            }

            device_pairing_status local_device_pairing_status() const
            {
                return state_ == details::lesc_pairing_state::pairing_completed
                    ? bluetoe::device_pairing_status::unauthenticated_key
                    : bluetoe::device_pairing_status::no_key;
            }

            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                if ( ediv == 0 && rand == 0 )
                    return { true, long_term_key_ };

                return std::pair< bool, details::uint128_t >{};
            }

            void pairing_requested( const io_capabilities_t& remote_io_caps )
            {
                assert( state_ == details::lesc_pairing_state::idle );
                state_ = details::lesc_pairing_state::pairing_requested;
                remote_io_caps_ = remote_io_caps;
            }

            void public_key_exchanged(
                const ecdh_private_key_t&   local_private_key,
                const ecdh_public_key_t&    local_public_key,
                const std::uint8_t*         remote_public_key,
                const details::uint128_t&   nonce )
            {
                assert( state_ == details::lesc_pairing_state::pairing_requested );
                state_ = details::lesc_pairing_state::public_keys_exchanged;

                local_private_key_ = local_private_key;
                local_public_key_  = local_public_key;
                local_nonce_       = nonce;
                std::copy( remote_public_key, remote_public_key + remote_public_key_.size(), remote_public_key_.begin() );
            }

            void pairing_confirm_send()
            {
                assert( state_ == details::lesc_pairing_state::public_keys_exchanged );
                state_ = details::lesc_pairing_state::pairing_confirm_send;
            }

            void pairing_random_exchanged( const std::uint8_t* remote_nonce )
            {
                assert( state_ == details::lesc_pairing_state::pairing_confirm_send );
                state_ = details::lesc_pairing_state::pairing_random_exchanged;

                std::copy( remote_nonce, remote_nonce + 16, remote_nonce_.begin() );
            }

            void pairing_completed( const details::uint128_t& long_term_key )
            {
                assert( state_ == details::lesc_pairing_state::pairing_random_exchanged );
                state_ = details::lesc_pairing_state::pairing_completed;

                long_term_key_ = long_term_key;
            }

            const uint128_t& local_nonce() const
            {
                return local_nonce_;
            }

            const uint128_t& remote_nonce() const
            {
                return remote_nonce_;
            }

            const std::uint8_t* local_public_key_x() const
            {
                return local_public_key_.data();
            }

            const std::uint8_t* remote_public_key_x() const
            {
                return remote_public_key_.data();
            }

            const std::uint8_t* remote_public_key() const
            {
                return remote_public_key_.data();
            }

            const std::uint8_t* local_private_key() const
            {
                return local_private_key_.data();
            }

            const io_capabilities_t& remote_io_caps() const
            {
                return remote_io_caps_;
            }

        private:
            bluetoe::link_layer::device_address remote_addr_;
            details::lesc_pairing_state         state_;

            ecdh_private_key_t                  local_private_key_;
            ecdh_public_key_t                   local_public_key_;
            ecdh_public_key_t                   remote_public_key_;
            uint128_t                           local_nonce_;
            uint128_t                           remote_nonce_;
            io_capabilities_t                   remote_io_caps_;
            uint128_t                           long_term_key_;
        };

        // features required by legacy and by lesc pairing
        template < template < class OtherConnectionData > class ConnectionData >
        class security_manager_base_base
        {
        protected:
            template < class OtherConnectionData >
            using connection_data = ConnectionData< OtherConnectionData >;

            template < class OtherConnectionData >
            void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state )
            {
                state.error_reset();
                details::error_response( error_code, output, out_size );
            }
        };


        template < template < class OtherConnectionData > class ConnectionData >
        class legacy_security_manager_base : private details::security_manager_base_base< ConnectionData >
        {
        public:
            /** @cond HIDDEN_SYMBOLS */
            template < class OtherConnectionData >
            using connection_data = ConnectionData< OtherConnectionData >;

            template < class OtherConnectionData, class SecurityFunctions >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            template < class OtherConnectionData >
            bool security_manager_output_available( connection_data< OtherConnectionData >& ) const;

            template < class OtherConnectionData, class SecurityFunctions >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            constexpr std::size_t security_manager_channel_mtu_size() const;

            struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                                details::security_manager_meta_type {};
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

            details::uint128_t c1_p1(
                const std::uint8_t* input, const std::uint8_t* output,
                const bluetoe::link_layer::device_address& initiating_device,
                const bluetoe::link_layer::device_address& responding_device );

            details::uint128_t c1_p2(
                const bluetoe::link_layer::device_address& initiating_device,
                const bluetoe::link_layer::device_address& responding_device );

            /** @endcond */
        };

        template < template < class OtherConnectionData > class ConnectionData >
        class lesc_security_manager_base : private details::security_manager_base_base< ConnectionData >
        {
        public:
            /** @cond HIDDEN_SYMBOLS */
            template < class OtherConnectionData >
            using connection_data = ConnectionData< OtherConnectionData >;

            template < class OtherConnectionData, class SecurityFunctions >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            template < class OtherConnectionData >
            bool security_manager_output_available( connection_data< OtherConnectionData >& ) const;

            template < class OtherConnectionData, class SecurityFunctions >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            constexpr std::size_t security_manager_channel_mtu_size() const;

            struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                                details::security_manager_meta_type {};
        private:
            static constexpr std::uint8_t   min_max_key_size = 7;
            static constexpr std::uint8_t   max_max_key_size = 16;
            static constexpr std::size_t    pairing_req_resp_size    = 7;
            static constexpr std::size_t    public_key_exchange_size = 65;
            static constexpr std::size_t    pairing_confirm_size     = 17;
            static constexpr std::size_t    pairing_random_size      = 17;
            static constexpr std::size_t    pairing_dhkey_check_size = 17;

            template < class OtherConnectionData, class SecurityFunctions >
            void handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            void create_pairing_response( std::uint8_t* output, std::size_t& out_size );

            template < class OtherConnectionData, class SecurityFunctions >
            void handle_pairing_public_key( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            template < class OtherConnectionData, class SecurityFunctions >
            void handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            template < class OtherConnectionData, class SecurityFunctions >
            void handle_pairing_dhkey_check( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

            details::io_capabilities_t local_io_caps() const;
            /** @endcond */
        };

    } // namespace details

    /**
     * @brief A Security manager implementation that supports legacy pairing.
     */
    using legacy_security_manager = details::legacy_security_manager_base< details::legacy_security_connection_data >;

    /**
     * @brief A Security manager that implements LESC pairing.
     */
    using lesc_security_manager = details::lesc_security_manager_base< details::lesc_security_connection_data >;

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

            device_pairing_status local_device_pairing_status() const
            {
                return bluetoe::device_pairing_status::no_key;
            }

        };

        template < class OtherConnectionData, class SecurityFunctions >
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        template < class OtherConnectionData >
        bool security_manager_output_available( connection_data< OtherConnectionData >& ) const;

        template < class OtherConnectionData, class SecurityFunctions >
        void l2cap_output( std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& );

        constexpr std::size_t security_manager_channel_mtu_size() const;

        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::security_manager_meta_type {};
        /** @endcond */
    };

    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::legacy_security_manager_base< ConnectionData >::l2cap_input(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace bluetoe::details;

        // is there at least an opcode?
        if ( in_size == 0 )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

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
                this->error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData >
    bool details::legacy_security_manager_base< ConnectionData >::security_manager_output_available( connection_data< OtherConnectionData >& ) const
    {
        return false;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::legacy_security_manager_base< ConnectionData >::l2cap_output( std::uint8_t*, std::size_t&, connection_data< OtherConnectionData >&, SecurityFunctions& )
    {
    }

    template < template < class OtherConnectionData > class ConnectionData >
    constexpr std::size_t details::legacy_security_manager_base< ConnectionData >::security_manager_channel_mtu_size() const
    {
        return default_att_mtu_size;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::legacy_security_manager_base< ConnectionData >::handle_pairing_request(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& functions )
    {
        using namespace details;

        if ( in_size != pairing_req_resp_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::legacy_pairing_state::idle )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

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
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );
        }

        create_pairing_response( output, out_size );

        const details::uint128_t srand    = functions.create_srand();
        const details::uint128_t p1       = c1_p1( input, output, state.remote_address(), functions.local_address() );
        const details::uint128_t p2       = c1_p2( state.remote_address(), functions.local_address() );

        state.pairing_request( srand, p1, p2 );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    inline void details::legacy_security_manager_base< ConnectionData >::create_pairing_response( std::uint8_t* output, std::size_t& out_size )
    {
        out_size = pairing_req_resp_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_response );
        output[ 1 ] = static_cast< std::uint8_t >( io_capabilities::no_input_no_output );
        output[ 2 ] = 0;
        output[ 3 ] = 0;
        output[ 4 ] = max_max_key_size;
        output[ 5 ] = 0;
        output[ 6 ] = 0;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::legacy_security_manager_base< ConnectionData >::handle_pairing_confirm(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace details;

        static constexpr std::size_t    request_size = 17;

        if ( in_size != request_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != legacy_pairing_state::pairing_requested )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        // save mconfirm for later
        state.pairing_confirm( &input[ 1 ], &input[ request_size ] );

        out_size = request_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_confirm );

        const auto sconfirm = func.c1( { { 0 } }, state.srand(), state.c1_p1(), state.c1_p2() );
        std::copy( sconfirm.begin(), sconfirm.end(), &output[ 1 ] );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::legacy_security_manager_base< ConnectionData >::handle_pairing_random(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace details;

        static constexpr std::size_t    pairing_random_size = 17;

        if ( in_size != pairing_random_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != legacy_pairing_state::pairing_confirmed )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        uint128_t mrand;
        std::copy( &input[ 1 ], &input[ pairing_random_size ], mrand.begin() );
        const auto mconfirm = func.c1( { { 0 } }, mrand, state.c1_p1(), state.c1_p2() );

        if ( mconfirm != state.mconfirm() )
            return this->error_response( sm_error_codes::confirm_value_failed, output, out_size, state );

        out_size = pairing_random_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_random );

        const auto srand = state.srand();
        std::copy( srand.begin(), srand.end(), &output[ 1 ] );

        const auto stk = func.s1( { { 0 } }, srand, mrand );
        state.pairing_completed( stk );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    inline details::uint128_t details::legacy_security_manager_base< ConnectionData >::c1_p1(
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

    template < template < class OtherConnectionData > class ConnectionData >
    inline details::uint128_t details::legacy_security_manager_base< ConnectionData >::c1_p2(
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

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::l2cap_input(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& func )
    {
        using namespace bluetoe::details;

        // is there at least an opcode?
        if ( in_size == 0 )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        assert( in_size != 0 );
        assert( out_size >= default_att_mtu_size );

        const sm_opcodes opcode = static_cast< sm_opcodes >( input[ 0 ] );

        switch ( opcode )
        {
            case sm_opcodes::pairing_request:
                handle_pairing_request( input, in_size, output, out_size, state, func );
                break;
            case sm_opcodes::pairing_public_key:
                handle_pairing_public_key( input, in_size, output, out_size, state, func );
                break;
            case sm_opcodes::pairing_random:
                handle_pairing_random( input, in_size, output, out_size, state, func );
                break;
            case sm_opcodes::pairing_dhkey_check:
                handle_pairing_dhkey_check( input, in_size, output, out_size, state, func );
                break;
            default:
                this->error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData >
    bool details::lesc_security_manager_base< ConnectionData >::security_manager_output_available( connection_data< OtherConnectionData >& state ) const
    {
        return state.state() == details::lesc_pairing_state::public_keys_exchanged;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::l2cap_output( std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& functions )
    {
        assert( state.state() == details::lesc_pairing_state::public_keys_exchanged );

        const auto& Nb = state.local_nonce();
        const std::uint8_t* Pkax = state.remote_public_key_x();
        const std::uint8_t* PKbx = state.local_public_key_x();

        const auto confirm = functions.f4( PKbx, Pkax, Nb, 0 );

        out_size = pairing_confirm_size;
        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_confirm );
        std::copy( confirm.begin(), confirm.end(), &output[ 1 ] );

        state.pairing_confirm_send();
    }

    template < template < class OtherConnectionData > class ConnectionData >
    constexpr std::size_t details::lesc_security_manager_base< ConnectionData >::security_manager_channel_mtu_size() const
    {
        return public_key_exchange_size;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::handle_pairing_request(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& /* functions */ )
    {
        using namespace details;

        if ( in_size != pairing_req_resp_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::lesc_pairing_state::idle )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

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
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );
        }

        if ( ( auth_req & static_cast< std::uint8_t >( authentication_requirements_flags::secure_connections ) ) == 0 )
            return this->error_response( sm_error_codes::pairing_not_supported, output, out_size, state );

        const io_capabilities_t remote_io_caps = {{ io_capability, oob_data_flag, auth_req }};
        state.pairing_requested( remote_io_caps );
        create_pairing_response( output, out_size );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::handle_pairing_public_key(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& functions )
    {
        if ( in_size != public_key_exchange_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::lesc_pairing_state::pairing_requested )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        assert( out_size >= public_key_exchange_size );

        if ( !functions.is_valid_public_key( &input[ 1 ] ) )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_public_key );

        out_size = public_key_exchange_size;
        const auto& keys  = functions.generate_keys();
        const auto& nonce = functions.select_random_nonce();

        state.public_key_exchanged( keys.second, keys.first, &input[ 1 ], nonce );
        std::copy( keys.first.begin(), keys.first.end(), &output[ 1 ] );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    inline void details::lesc_security_manager_base< ConnectionData >::create_pairing_response( std::uint8_t* output, std::size_t& out_size )
    {
        using namespace details;

        const io_capabilities_t io_caps = local_io_caps();
        out_size = pairing_req_resp_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_response );
        output[ 1 ] = io_caps[ 0 ];
        output[ 2 ] = io_caps[ 1 ];
        output[ 3 ] = io_caps[ 2 ];
        output[ 4 ] = max_max_key_size;
        output[ 5 ] = 0;
        output[ 6 ] = 0;
    }

    template < template < class OtherConnectionData > class ConnectionData >
    details::io_capabilities_t details::lesc_security_manager_base< ConnectionData >::local_io_caps() const
    {
        return {{
            static_cast< std::uint8_t >( details::io_capabilities::no_input_no_output ),
            0,
            static_cast< std::uint8_t >( details::authentication_requirements_flags::secure_connections ) }};
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& )
    {
        if ( in_size != pairing_random_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::lesc_pairing_state::pairing_confirm_send )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        const auto& nonce = state.local_nonce();

        out_size = pairing_random_size;
        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_random );
        std::copy( nonce.begin(), nonce.end(), &output[ 1 ] );

        state.pairing_random_exchanged( input + 1 );
    }

    template < template < class OtherConnectionData > class ConnectionData >
    template < class OtherConnectionData, class SecurityFunctions >
    void details::lesc_security_manager_base< ConnectionData >::handle_pairing_dhkey_check( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >& state, SecurityFunctions& functions )
    {
        if ( in_size != pairing_dhkey_check_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::lesc_pairing_state::pairing_random_exchanged )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        const details::ecdh_shared_secret_t dh_key = functions.p256( state.local_private_key(), state.remote_public_key() );

        details::uint128_t mac_key;
        details::uint128_t ltk;
        static const details::uint128_t zero = {{ 0 }};

        std::tie( mac_key, ltk ) = functions.f5( dh_key, state.remote_nonce(), state.local_nonce(), state.remote_address(), functions.local_address() );

        const auto calc_ea = functions.f6( mac_key, state.remote_nonce(), state.local_nonce(), zero, state.remote_io_caps(), state.remote_address(), functions.local_address() );

        if ( !std::equal( calc_ea.begin(), calc_ea.end(), &input[ 1 ] ) )
            return this->error_response( details::sm_error_codes::dhkey_check_failed, output, out_size, state );

        const auto eb = functions.f6( mac_key, state.local_nonce(), state.remote_nonce(), zero, local_io_caps(), functions.local_address(), state.remote_address() );

        out_size = pairing_dhkey_check_size;
        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_dhkey_check );
        std::copy( eb.begin(), eb.end(), &output[ 1 ] );

        state.pairing_completed( ltk );
    }

    template < class OtherConnectionData, class SecurityFunctions >
    void no_security_manager::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size, connection_data< OtherConnectionData >&, SecurityFunctions& )
    {
        error_response( details::sm_error_codes::pairing_not_supported, output, out_size );
    }

    template < class OtherConnectionData >
    bool no_security_manager::security_manager_output_available( connection_data< OtherConnectionData >& ) const
    {
        return false;
    }

    template < class OtherConnectionData, class SecurityFunctions >
    void no_security_manager::l2cap_output( std::uint8_t*, std::size_t&, connection_data< OtherConnectionData >&, SecurityFunctions& )
    {
    }

    constexpr std::size_t no_security_manager::security_manager_channel_mtu_size() const
    {
        return 0;
    }

    /** @endcond */
}

#endif // include guard
