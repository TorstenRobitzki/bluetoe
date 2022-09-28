#ifndef BLUETOE_SM_SECURITY_CONNECTION_DATA_HPP
#define BLUETOE_SM_SECURITY_CONNECTION_DATA_HPP

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
        struct authentication_requirements_flags_meta_type {};
        struct bonding_data_base_meta_type {};
        struct key_distribution_meta_type {};

        enum class sm_pairing_state : std::uint8_t {
            // both, legacy and LESC
            idle,
            pairing_completed,
            user_response_wait,
            user_response_failed,
            user_response_success,
            // legacy only
            legacy_pairing_requested,
            legacy_pairing_confirmed,
            // LESC only
            lesc_pairing_requested,
            lesc_public_keys_exchanged,
            lesc_pairing_confirm_send,
            lesc_pairing_random_exchanged,
        };

        enum class authentication_requirements_flags : std::uint8_t {
            bonding                 = 0x01,
            mitm                    = 0x04,
            secure_connections      = 0x08,
            keypress                = 0x10
        };

        template < class OtherConnectionData >
        class security_connection_data_base : public OtherConnectionData
        {
        public:
            template < class ... Args >
            security_connection_data_base( Args&&... args )
                : OtherConnectionData( args... )
                , state_( details::sm_pairing_state::idle )
            {}

            details::sm_pairing_state state() const
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
                state( details::sm_pairing_state::idle );
            }

        protected:
            void state( details::sm_pairing_state state )
            {
                state_ = state;
            }

        private:
            link_layer::device_address          remote_addr_;
            details::sm_pairing_state           state_;
        };

        template < class OtherConnectionData >
        class legacy_security_connection_data : public security_connection_data_base< OtherConnectionData >
        {
        public:
            template < class ... Args >
            legacy_security_connection_data( Args&&... args )
                : security_connection_data_base< OtherConnectionData >( args... )
            {}

            void legacy_pairing_request( const details::uint128_t& srand, const details::uint128_t& p1, const details::uint128_t& p2 )
            {
                assert( this->state() == details::sm_pairing_state::idle );
                this->state( details::sm_pairing_state::legacy_pairing_requested );
                state_data_.pairing_state.c1_p1 = p1;
                state_data_.pairing_state.c1_p2 = p2;
                state_data_.pairing_state.srand = srand;
            }

            void pairing_confirm( const std::uint8_t* mconfirm_begin, const std::uint8_t* mconfirm_end )
            {
                assert( this->state() == details::sm_pairing_state::legacy_pairing_requested );
                this->state( details::sm_pairing_state::legacy_pairing_confirmed );

                assert( static_cast< std::size_t >( mconfirm_end - mconfirm_begin ) == state_data_.pairing_state.mconfirm.max_size() );
                std::copy( mconfirm_begin, mconfirm_end, state_data_.pairing_state.mconfirm.begin() );
            }

            void legacy_pairing_completed( const details::uint128_t& short_term_key )
            {
                assert( this->state() == details::sm_pairing_state::legacy_pairing_confirmed );
                this->state( details::sm_pairing_state::pairing_completed );

                state_data_.completed_state.short_term_key = short_term_key;
            }

            template < class Link >
            bool outgoing_security_manager_data_available( const Link& link ) const
            {
                return link.is_encrypted() && this->state() != details::sm_pairing_state::pairing_completed;
            }

            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                if ( ediv == 0 && rand == 0 && this->state() == details::sm_pairing_state::pairing_completed )
                    return { true, state_data_.completed_state.short_term_key };

                return std::pair< bool, details::uint128_t >{};
            }

            void restore_bond( const details::uint128_t& key )
            {
                state_data_.completed_state.short_term_key = key;
                this->state( details::sm_pairing_state::pairing_completed );
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

            void pairing_algorithm( details::legacy_pairing_algorithm algo )
            {
                algorithm_ = algo;
            }

            details::legacy_pairing_algorithm legacy_pairing_algorithm() const
            {
                return algorithm_;
            }

            device_pairing_status local_device_pairing_status() const
            {
                if ( this->state() != details::sm_pairing_state::pairing_completed )
                    return bluetoe::device_pairing_status::no_key;

                return algorithm_ == details::legacy_pairing_algorithm::just_works
                    ? device_pairing_status::unauthenticated_key
                    : device_pairing_status::authenticated_key;
            }

            void passkey( const details::uint128_t& key )
            {
                state_data_.pairing_state.passkey = key;
            }

            details::uint128_t passkey() const
            {
                return state_data_.pairing_state.passkey;
            }
        private:
            details::legacy_pairing_algorithm   algorithm_;

            union {
                struct {
                    details::uint128_t c1_p1;
                    details::uint128_t c1_p2;
                    details::uint128_t srand;
                    details::uint128_t mconfirm;
                    details::uint128_t passkey;
                }                                   pairing_state;

                struct {
                    details::uint128_t short_term_key;
                }                                   completed_state;
            }                       state_data_;
        };

        template < class OtherConnectionData >
        class lesc_security_connection_data : public security_connection_data_base< OtherConnectionData >, public pairing_yes_no_response
        {
        public:
            template < class ... Args >
            lesc_security_connection_data( Args&&... args )
                : security_connection_data_base< OtherConnectionData >( args... )
            {}

            void wait_for_user_response()
            {
                this->state( details::sm_pairing_state::user_response_wait );
            }

            void yes_no_response( bool response ) override
            {
                assert( this->state() == details::sm_pairing_state::user_response_wait );

                this->state( response
                    ? details::sm_pairing_state::user_response_success
                    : details::sm_pairing_state::user_response_failed );
            }

            device_pairing_status local_device_pairing_status() const
            {
                return this->state() == details::sm_pairing_state::pairing_completed
                    ? bluetoe::device_pairing_status::unauthenticated_key
                    : bluetoe::device_pairing_status::no_key;
            }

            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                if ( ediv == 0 && rand == 0 )
                    return { true, long_term_key_ };

                return std::pair< bool, details::uint128_t >{};
            }

            void restore_bond( const details::uint128_t& key )
            {
                long_term_key_ = key;
                this->state( details::sm_pairing_state::pairing_completed );
            }

            void pairing_requested( const io_capabilities_t& remote_io_caps )
            {
                assert( this->state() == details::sm_pairing_state::idle );
                this->state( details::sm_pairing_state::lesc_pairing_requested );
                remote_io_caps_ = remote_io_caps;
            }

            void public_key_exchanged(
                const ecdh_private_key_t&   local_private_key,
                const ecdh_public_key_t&    local_public_key,
                const std::uint8_t*         remote_public_key,
                const details::uint128_t&   nonce )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_requested );
                this->state( details::sm_pairing_state::lesc_public_keys_exchanged );

                local_private_key_ = local_private_key;
                local_public_key_  = local_public_key;
                local_nonce_       = nonce;
                std::copy( remote_public_key, remote_public_key + remote_public_key_.size(), remote_public_key_.begin() );
            }

            void pairing_confirm_send()
            {
                assert( this->state() == details::sm_pairing_state::lesc_public_keys_exchanged );
                this->state( details::sm_pairing_state::lesc_pairing_confirm_send );
            }

            void pairing_random_exchanged( const std::uint8_t* remote_nonce )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_confirm_send );
                this->state( details::sm_pairing_state::lesc_pairing_random_exchanged );

                std::copy( remote_nonce, remote_nonce + 16, remote_nonce_.begin() );
            }

            void lesc_pairing_completed( const details::uint128_t& long_term_key )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_random_exchanged
                     || this->state() == details::sm_pairing_state::user_response_success );

                this->state( details::sm_pairing_state::pairing_completed );

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

            void pairing_algorithm( details::lesc_pairing_algorithm algo )
            {
                algorithm_ = algo;
            }

            details::lesc_pairing_algorithm lesc_pairing_algorithm() const
            {
                return algorithm_;
            }
        private:
            enum lesc_pairing_algorithm         algorithm_;

            ecdh_private_key_t                  local_private_key_;
            ecdh_public_key_t                   local_public_key_;
            ecdh_public_key_t                   remote_public_key_;
            uint128_t                           local_nonce_;
            uint128_t                           remote_nonce_;
            io_capabilities_t                   remote_io_caps_;
            uint128_t                           long_term_key_;
        };

        template < class OtherConnectionData >
        class security_connection_data : public security_connection_data_base< OtherConnectionData >, public pairing_yes_no_response
        {
        public:
            template < class ... Args >
            security_connection_data( Args&&... args )
                : security_connection_data_base< OtherConnectionData >( args... )
            {}

            void wait_for_user_response()
            {
                this->state( details::sm_pairing_state::user_response_wait );
            }

            void yes_no_response( bool response ) override
            {
                assert( this->state() == details::sm_pairing_state::user_response_wait );

                this->state( response
                    ? details::sm_pairing_state::user_response_success
                    : details::sm_pairing_state::user_response_failed );
            }

            void pairing_algorithm( details::legacy_pairing_algorithm algo )
            {
                state_data_.legacy_state.algorithm = algo;
            }

            details::legacy_pairing_algorithm legacy_pairing_algorithm() const
            {
                return state_data_.legacy_state.algorithm;
            }

            void pairing_algorithm( details::lesc_pairing_algorithm algo )
            {
                state_data_.lesc_state.algorithm = algo;
            }

            details::lesc_pairing_algorithm lesc_pairing_algorithm() const
            {
                return state_data_.lesc_state.algorithm;
            }

            void legacy_pairing_request( const details::uint128_t& srand, const details::uint128_t& p1, const details::uint128_t& p2 )
            {
                assert( this->state() == details::sm_pairing_state::idle );
                this->state( details::sm_pairing_state::legacy_pairing_requested );
                state_data_.legacy_state.states.pairing_state.c1_p1 = p1;
                state_data_.legacy_state.states.pairing_state.c1_p2 = p2;
                state_data_.legacy_state.states.pairing_state.srand = srand;
            }

            void pairing_confirm( const std::uint8_t* mconfirm_begin, const std::uint8_t* mconfirm_end )
            {
                assert( this->state() == details::sm_pairing_state::legacy_pairing_requested );
                this->state( details::sm_pairing_state::legacy_pairing_confirmed );

                assert( static_cast< std::size_t >( mconfirm_end - mconfirm_begin ) == state_data_.legacy_state.states.pairing_state.mconfirm.max_size() );
                std::copy( mconfirm_begin, mconfirm_end, state_data_.legacy_state.states.pairing_state.mconfirm.begin() );
            }

            void legacy_pairing_completed( const details::uint128_t& short_term_key )
            {
                assert( this->state() == details::sm_pairing_state::legacy_pairing_confirmed );
                this->state( details::sm_pairing_state::pairing_completed );

                long_term_key_ = short_term_key;

                pairing_status_ = state_data_.legacy_state.algorithm == details::legacy_pairing_algorithm::just_works
                    ? device_pairing_status::unauthenticated_key
                    : device_pairing_status::authenticated_key;
            }

            void lesc_pairing_completed( const details::uint128_t& long_term_key )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_random_exchanged
                     || this->state() == details::sm_pairing_state::user_response_success );

                this->state( details::sm_pairing_state::pairing_completed );

                long_term_key_ = long_term_key;

                pairing_status_ = state_data_.lesc_state.algorithm == details::lesc_pairing_algorithm::just_works
                    ? device_pairing_status::unauthenticated_key
                    : device_pairing_status::authenticated_key;
            }

            const details::uint128_t& c1_p1() const
            {
                return state_data_.legacy_state.states.pairing_state.c1_p1;
            }

            const details::uint128_t& c1_p2() const
            {
                return state_data_.legacy_state.states.pairing_state.c1_p2;
            }

            const details::uint128_t& srand() const
            {
                return state_data_.legacy_state.states.pairing_state.srand;
            }

            const details::uint128_t& mconfirm() const
            {
                return state_data_.legacy_state.states.pairing_state.mconfirm;
            }

            void passkey( const details::uint128_t& key )
            {
                state_data_.legacy_state.states.pairing_state.passkey = key;
            }

            details::uint128_t passkey() const
            {
                return state_data_.legacy_state.states.pairing_state.passkey;
            }

            void public_key_exchanged(
                const ecdh_private_key_t&   local_private_key,
                const ecdh_public_key_t&    local_public_key,
                const std::uint8_t*         remote_public_key,
                const details::uint128_t&   nonce )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_requested );
                this->state( details::sm_pairing_state::lesc_public_keys_exchanged );

                state_data_.lesc_state.local_private_key_ = local_private_key;
                state_data_.lesc_state.local_public_key_  = local_public_key;
                state_data_.lesc_state.local_nonce_       = nonce;
                std::copy( remote_public_key, remote_public_key + state_data_.lesc_state.remote_public_key_.size(), state_data_.lesc_state.remote_public_key_.begin() );
            }

            void pairing_confirm_send()
            {
                assert( this->state() == details::sm_pairing_state::lesc_public_keys_exchanged );
                this->state( details::sm_pairing_state::lesc_pairing_confirm_send );
            }

            void pairing_random_exchanged( const std::uint8_t* remote_nonce )
            {
                assert( this->state() == details::sm_pairing_state::lesc_pairing_confirm_send );
                this->state( details::sm_pairing_state::lesc_pairing_random_exchanged );

                std::copy( remote_nonce, remote_nonce + 16, state_data_.lesc_state.remote_nonce_.begin() );
            }

            void pairing_requested( const io_capabilities_t& remote_io_caps )
            {
                assert( this->state() == details::sm_pairing_state::idle );
                this->state( details::sm_pairing_state::lesc_pairing_requested );
                state_data_.lesc_state.remote_io_caps_ = remote_io_caps;
            }

            const uint128_t& local_nonce() const
            {
                return state_data_.lesc_state.local_nonce_;
            }

            const uint128_t& remote_nonce() const
            {
                return state_data_.lesc_state.remote_nonce_;
            }

            const std::uint8_t* local_public_key_x() const
            {
                return state_data_.lesc_state.local_public_key_.data();
            }

            const std::uint8_t* remote_public_key_x() const
            {
                return state_data_.lesc_state.remote_public_key_.data();
            }

            const std::uint8_t* remote_public_key() const
            {
                return state_data_.lesc_state.remote_public_key_.data();
            }

            const std::uint8_t* local_private_key() const
            {
                return state_data_.lesc_state.local_private_key_.data();
            }

            const io_capabilities_t& remote_io_caps() const
            {
                return state_data_.lesc_state.remote_io_caps_;
            }

            template < class Link >
            bool outgoing_security_manager_data_available( const Link& link ) const
            {
                return link.is_encrypted() && this->state() != details::sm_pairing_state::pairing_completed;
            }

            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                if ( ediv == 0 && rand == 0 )
                    return { true, long_term_key_ };

                return std::pair< bool, details::uint128_t >{};
            }

            void restore_bond( const details::uint128_t& key )
            {
                long_term_key_ = key;
                this->state( details::sm_pairing_state::pairing_completed );
            }

            device_pairing_status local_device_pairing_status() const
            {
                if ( this->state() != details::sm_pairing_state::pairing_completed )
                    return bluetoe::device_pairing_status::no_key;

                return pairing_status_;
            }

        private:
            details::uint128_t                  long_term_key_;
            device_pairing_status               pairing_status_;

            union {
                struct {
                    union {
                        struct {
                            details::uint128_t c1_p1;
                            details::uint128_t c1_p2;
                            details::uint128_t srand;
                            details::uint128_t mconfirm;
                            details::uint128_t passkey;
                        }                                   pairing_state;
                    } states;
                    enum legacy_pairing_algorithm    algorithm;
                }                                           legacy_state;

                struct {
                    ecdh_private_key_t          local_private_key_;
                    ecdh_public_key_t           local_public_key_;
                    ecdh_public_key_t           remote_public_key_;
                    uint128_t                   local_nonce_;
                    uint128_t                   remote_nonce_;
                    io_capabilities_t           remote_io_caps_;
                    enum lesc_pairing_algorithm algorithm;
                }                                           lesc_state;
            } state_data_;
        };
    }
}

#endif
