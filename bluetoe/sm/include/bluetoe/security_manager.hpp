#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <array>
#include <algorithm>

#include <bluetoe/codes.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/link_state.hpp>
#include <bluetoe/pairing_status.hpp>
#include <bluetoe/ll_meta_types.hpp>
#include <bluetoe/oob_authentication.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/io_capabilities.hpp>
#include <bluetoe/l2cap_channels.hpp>
#include <bluetoe/security_connection_data.hpp>

namespace bluetoe {

    namespace details {

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
            central_identification,
            identity_information,
            identity_address_information,
            signing_information,
            security_request,
            pairing_public_key,
            pairing_dhkey_check,
            pairing_keypress_notification
        };
    }

    /**
     * @brief access to a user defined key data base
     *
     * The provided implementation has to implement the following functions:
     *
     *   template < class Radio >
     *   longterm_key_t create_new_bond( Radio& radio, const bluetoe::link_layer::device_address& mac );
     *
     *   void store_bond(
     *       const bluetoe::details::longterm_key_t& key,
     *       const bluetoe::link_layer::device_address& mac)
     *
     *   std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand, const bluetoe::link_layer::device_address& remote_address ) const;
     *
     *   template < class Connection >
     *   void restore_cccds( Connection& connection );
     *
     * create_new_bond() is used to create a longterm key für a bond with the given device. `radio` can be used
     * to generate random numbers.
     *
     * store_bond() is used to store a key that was either created by store_bond(), or during the LESC paring process.
     *
     * find_key() will be called to lookup a stored long term key. If it does not exists, the function should return
     * a pair with the first member set to false.
     *
     * This will also set bonding flags in the pairing response to "Bonding".
     *
     * @tparam Obj type that implements the given requirements
     * @tparam obj instance that implements the given requirements
     */
    template < class Obj, Obj& obj >
    struct bonding_data_base
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::authentication_requirements_flags_meta_type,
                            details::bonding_data_base_meta_type,
                            details::key_distribution_meta_type {};

        static constexpr std::uint8_t flags = static_cast< std::uint8_t >(
            details::authentication_requirements_flags::bonding );

        // Data that has to be added by the bonding data base
        template < class OtherConnectionData >
        class bonding_db_data_t : public OtherConnectionData
        {
        public:
            std::pair< bool, details::uint128_t > find_key( std::uint16_t ediv, std::uint64_t rand ) const
            {
                const auto local_key = OtherConnectionData::find_key( ediv, rand );

                if ( local_key.first )
                    return local_key;

                return obj.find_key( ediv, rand, this->remote_address() );
            }

            template < class Radio, class Connection >
            void arm_key_distribution( Radio& radio, const Connection& connection )
            {
                pending_encryption_information = true;
                pending_central_identification = true;

                pending_key = obj.create_new_bond( radio, connection.remote_address() );
                obj.store_bond( pending_key, connection );
            }

            template < typename Connection >
            void store_lesc_key_in_bond_db( const details::uint128_t& key, const Connection& connection )
            {
                obj.store_bond( details::longterm_key_t{ key, 0u, 0u }, connection );
            }

            template < typename Connection >
            void distribute_keys( std::uint8_t* output, std::size_t& out_size, Connection& connection )
            {
                out_size = 0;

                if ( connection.security_attributes().is_encrypted )
                {
                    if ( pending_encryption_information )
                    {
                        pending_encryption_information = false;

                        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::encryption_information );
                        std::copy( pending_key.longterm_key.begin(), pending_key.longterm_key.end(), &output[ 1 ] );
                        std::fill( pending_key.longterm_key.begin(), pending_key.longterm_key.end(), 0 );

                        out_size = 17;
                    }
                    else if ( pending_central_identification )
                    {
                        pending_central_identification = false;

                        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::central_identification );
                        details::write_16bit( &output[ 1 ], pending_key.ediv );
                        details::write_64bit( &output[ 3 ], pending_key.rand );

                        out_size = 11;
                    }
                }
            }

            template < typename Connection >
            void restore_bonded_cccds( Connection& connection )
            {
                obj.restore_cccds( connection );
            }

        private:
            bool pending_encryption_information;
            bool pending_central_identification;
            details::longterm_key_t pending_key;
        };

        static constexpr std::uint8_t request_key_flags = 0x01;

        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    struct no_bonding_data_base
    {
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::authentication_requirements_flags_meta_type,
                            details::bonding_data_base_meta_type {};

        template < class OtherConnectionData >
        struct bonding_db_data_t : OtherConnectionData
        {
            template < class Radio, class Connection >
            void arm_key_distribution( Radio&, const Connection& )
            {
            }

            template < typename Connection >
            void distribute_keys( std::uint8_t*, std::size_t& out_size, Connection& )
            {
                out_size = 0;
            }

            template < typename Connection >
            void store_lesc_key_in_bond_db( const details::uint128_t&, const Connection& )
            {
            }

            template < typename Connection >
            void restore_bonded_cccds( Connection& )
            {
            }
        };

    };

    struct no_key_distribution
    {
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::key_distribution_meta_type {};

        static constexpr std::uint8_t request_key_flags = 0;
    };
    /** @endcond */

    namespace details {

        inline void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size )
        {
            output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_failed );
            output[ 1 ] = static_cast< std::uint8_t >( error_code );

            out_size = 2;
        }

        template < typename ... Options >
        struct accumulate_authentication_requirements_flags;

        template <>
        struct accumulate_authentication_requirements_flags< std::tuple<> >
        {
            static constexpr std::uint8_t flags = 0;
        };

        template < typename Option, class ... Options >
        struct accumulate_authentication_requirements_flags< std::tuple< Option, Options... > > :
            accumulate_authentication_requirements_flags< std::tuple< Options... > >
        {
            static constexpr std::uint8_t flags = Option::flags
                | accumulate_authentication_requirements_flags< std::tuple< Options... > >::flags;
        };

        // features required by legacy and by lesc pairing
        template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
        class security_manager_base :
            protected details::find_by_meta_type<
                details::oob_authentication_callback_meta_type,
                Options...,
                details::no_oob_authentication >::type
        {
        protected:
            static constexpr std::uint8_t authentication_requirements_flags =
                accumulate_authentication_requirements_flags<
                    typename find_all_by_meta_type< authentication_requirements_flags_meta_type, Options... >::type
                >::flags;

            using bonding_data_base_t = typename details::find_by_meta_type<
                details::bonding_data_base_meta_type,
                Options...,
                no_bonding_data_base >::type;

            using key_distribution_t = typename details::find_by_meta_type<
                details::key_distribution_meta_type,
                Options...,
                no_key_distribution >::type;

            template < class Connection >
            void error_response( details::sm_error_codes error_code, std::uint8_t* output, std::size_t& out_size, Connection& state )
            {
                state.error_reset();
                details::error_response( error_code, output, out_size );
            }

            using io_device_t = io_capabilities_matrix< Options... >;

            static constexpr std::uint8_t   min_max_key_size = 7;
            static constexpr std::uint8_t   max_max_key_size = 16;
            static constexpr std::size_t    pairing_req_resp_size = 7;

            template < class Connection >
            void legacy_handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            void create_pairing_response( std::uint8_t* output, std::size_t& out_size, const io_capabilities_t& io_caps );

            template < class Connection >
            void legacy_handle_pairing_confirm( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void legacy_handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            uint128_t legacy_create_temporary_key( Connection& );

            template < class Connection >
            uint128_t legacy_temporary_key( const Connection& ) const;

            details::legacy_pairing_algorithm legacy_select_pairing_algorithm( std::uint8_t io_capability, std::uint8_t oob_data_flag, std::uint8_t auth_req, bool has_oob_data );

            details::uint128_t legacy_c1_p1(
                const std::uint8_t* input, const std::uint8_t* output,
                const bluetoe::link_layer::device_address& initiating_device,
                const bluetoe::link_layer::device_address& responding_device );

            details::uint128_t legacy_c1_p2(
                const bluetoe::link_layer::device_address& initiating_device,
                const bluetoe::link_layer::device_address& responding_device );

            static constexpr std::size_t    public_key_exchange_size = 65;
            static constexpr std::size_t    pairing_confirm_size     = 17;
            static constexpr std::size_t    pairing_random_size      = 17;
            static constexpr std::size_t    pairing_dhkey_check_size = 17;

            template < class Connection >
            void lesc_handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void lesc_handle_pairing_public_key( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void lesc_handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void lesc_handle_pairing_dhkey_check( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            bool lesc_security_manager_output_available( Connection& ) const;

            template < class Connection >
            void lesc_l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& );

            details::io_capabilities_t legacy_local_io_caps() const;
            details::io_capabilities_t lesc_local_io_caps() const;

            details::lesc_pairing_algorithm lesc_select_pairing_algorithm( std::uint8_t io_capability, std::uint8_t oob_data_flag, std::uint8_t auth_req, bool has_oob_data );

            SecurityFunctions& security_functions()
            {
                return static_cast< SecurityFunctions& >( *this );
            }

            const SecurityFunctions& security_functions() const
            {
                return static_cast< const SecurityFunctions& >( *this );
            }
        };

        template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
        class legacy_security_manager_impl : public security_manager_base< SecurityFunctions, ConnectionData, Options... >
        {
        public:
            /** @cond HIDDEN_SYMBOLS */

            template < class Connection >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& );

            static constexpr std::uint16_t channel_id               = l2cap_channel_ids::sm;
            static constexpr std::size_t   minimum_channel_mtu_size = default_att_mtu_size;
            static constexpr std::size_t   maximum_channel_mtu_size = default_att_mtu_size;

            using base_t = security_manager_base< SecurityFunctions, ConnectionData, Options... >;

            template < class OtherConnectionData >
            using channel_data_t = typename base_t::bonding_data_base_t::template bonding_db_data_t< ConnectionData< OtherConnectionData > >;
            /** @endcond */
        };

        template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
        class lesc_security_manager_impl : public security_manager_base< SecurityFunctions, ConnectionData, Options... >
        {
        public:
            /** @cond HIDDEN_SYMBOLS */
            template < class Connection >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& );

            static constexpr std::uint16_t channel_id               = l2cap_channel_ids::sm;
            static constexpr std::size_t   minimum_channel_mtu_size = default_lesc_mtu_size;
            static constexpr std::size_t   maximum_channel_mtu_size = default_lesc_mtu_size;

            using base_t = security_manager_base< SecurityFunctions, ConnectionData, Options... >;

            template < class OtherConnectionData >
            using channel_data_t = typename base_t::bonding_data_base_t::template bonding_db_data_t< ConnectionData< OtherConnectionData > >;
            /** @endcond */
        };

        template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
        class security_manager_impl : public security_manager_base< SecurityFunctions, ConnectionData, Options... >
        {
        public:
            /** @cond HIDDEN_SYMBOLS */
            template < class Connection >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& );

            static constexpr std::uint16_t channel_id               = l2cap_channel_ids::sm;
            static constexpr std::size_t   minimum_channel_mtu_size = default_lesc_mtu_size;
            static constexpr std::size_t   maximum_channel_mtu_size = default_lesc_mtu_size;

            using base_t = security_manager_base< SecurityFunctions, ConnectionData, Options... >;

            template < class OtherConnectionData >
            using channel_data_t = typename base_t::bonding_data_base_t::template bonding_db_data_t< ConnectionData< OtherConnectionData > >;

        private:
            using security_manager_base< SecurityFunctions, ConnectionData, Options... >::security_functions;

            template < class Connection >
            void handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            /** @endcond */
        };

    } // namespace details

    /**
     * @brief A Security manager implementation that supports legacy pairing.
     *
     * The legacy_security_manager supports legacy pairing. To do so, the security manager needs a hardware binding
     * that supports the required cryptographical primities. Usually this is the SM implementation with the least resource
     * costs.
     *
     * By default, this security manager only implements "just works" pairing. To implement other pairing methods, the
     * library must get some details about how to handle IO.
     *
     * @sa pairing_yes_no
     * @sa pairing_keyboard
     * @sa pairing_numeric_output
     *
     * @sa bonding_data_base
     *
     * @sa lesc_security_manager
     * @sa security_manager
     * @sa no_security_manager
     */
    class legacy_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < typename SecurityFunctions, typename ... Options >
        using impl = details::legacy_security_manager_impl< SecurityFunctions, details::legacy_security_connection_data, Options... >;

        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::security_manager_meta_type {};
        /** @endcond */
    };

    /**
     * @brief A Security manager that implements only LESC pairing.
     *
     * This is the current default. The SM will reject every attempt to pair to the device without
     * LE Secure Connections.
     *
     * By default, this security manager only implements "just works" pairing. To implement other pairing methods, the
     * library must get some details about how to handle IO.
     *
     * @sa pairing_yes_no
     * @sa pairing_keyboard
     * @sa pairing_numeric_output
     *
     * @sa bonding_data_base
     *
     * @sa legacy_security_manager
     * @sa security_manager
     * @sa no_security_manager
     */
    class lesc_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < typename SecurityFunctions, typename ... Options >
        using impl = details::lesc_security_manager_impl< SecurityFunctions, details::lesc_security_connection_data, Options... >;

        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::security_manager_meta_type {};
        /** @endcond */
    };

    /**
     * @brief A security manager that implpementes the full set of pairing methods (Legacy and LESC)
     *
     * By default, this security manager only implements "just works" pairing. To implement other pairing methods, the
     * library must get some details about how to handle IO.
     *
     * @sa pairing_yes_no
     * @sa pairing_keyboard
     * @sa pairing_numeric_output
     *
     * @sa bonding_data_base
     *
     * @sa legacy_security_manager
     * @sa lesc_security_manager
     * @sa no_security_manager
     */
    class security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < typename SecurityFunctions, typename ... Options >
        using impl = details::security_manager_impl< SecurityFunctions, details::security_connection_data, Options... >;

        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::security_manager_meta_type {};

        /** @endcond */
    };

    /**
     * @brief implementation of the security manager, that actievly rejects every pairing attempt.
     *
     * Use this option, if you are sure, that you are not going to require encryption at all or if the
     * required security measures are implemented on the application level.
     *
     * @sa legacy_security_manager
     * @sa lesc_security_manager
     * @sa security_manager
     */
    class no_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        template < typename SecurityFunctions, typename ... >
        class impl
        {
        public:
            template < class OtherConnectionData >
            class channel_data_t : public OtherConnectionData
            {
            public:
                void remote_connection_created( const bluetoe::link_layer::device_address& )
                {
                }

                device_pairing_status local_device_pairing_status() const
                {
                    return bluetoe::device_pairing_status::no_key;
                }

            };

            template < class Connection >
            void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& );

            template < class Connection >
            void l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& );

            static constexpr std::uint16_t channel_id               = l2cap_channel_ids::sm;
            static constexpr std::size_t   minimum_channel_mtu_size = 0;
            static constexpr std::size_t   maximum_channel_mtu_size = 0;
        };

        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::security_manager_meta_type {};
        /** @endcond */
    };

    /**
     * @brief requests bonding during pairing
     *
     * This will set bonding flags in the pairing response to "Bonding"
     */
    struct enable_bonding
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::authentication_requirements_flags_meta_type {};

        static constexpr std::uint8_t flags = static_cast< std::uint8_t >(
            details::authentication_requirements_flags::bonding );
        /** @endcond */
    };

    /**
     * @brief set the MITM flag in the Authentication requirements flags of the
     *        pairing response.
     */
    struct require_man_in_the_middle_protection
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::authentication_requirements_flags_meta_type {};

        static constexpr std::uint8_t flags = static_cast< std::uint8_t >(
            details::authentication_requirements_flags::mitm );
        /** @endcond */
    };

    /**
     * @brief set the MITM flag in the Authentication requirements flags of the
     *        pairing response.
     */
    struct enable_keypress_notifications
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :  link_layer::details::valid_link_layer_option_meta_type,
                            details::authentication_requirements_flags_meta_type {};

        static constexpr std::uint8_t flags = static_cast< std::uint8_t >(
            details::authentication_requirements_flags::keypress );
        /** @endcond */
    };

    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_handle_pairing_request(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        using namespace details;

        if ( in_size != pairing_req_resp_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::sm_pairing_state::idle )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        const std::uint8_t io_capability                = input[ 1 ];
        const std::uint8_t oob_data_flag                = input[ 2 ];
        const std::uint8_t auth_req                     = input[ 3 ] & 0x1f;
        const std::uint8_t max_key_size                 = input[ 4 ];
        const std::uint8_t initiator_key_distribution   = input[ 5 ];
        const std::uint8_t responder_key_distribution   = input[ 6 ];

        if (
            ( io_capability > static_cast< std::uint8_t >( io_capabilities::last ) )
         || ( oob_data_flag & ~0x01 )
         || ( max_key_size < min_max_key_size || max_key_size > max_max_key_size )
         || ( initiator_key_distribution & 0xf0 )
         || ( responder_key_distribution & 0xf0 )
        )
        {
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );
        }

        this->request_oob_data_presents_for_remote_device( state.remote_address() );
        state.pairing_algorithm( legacy_select_pairing_algorithm( io_capability, oob_data_flag, auth_req, this->has_oob_data_for_remote_device() ) );

        create_pairing_response( output, out_size, legacy_local_io_caps() );

        const details::uint128_t srand    = security_functions().create_srand();
        const details::uint128_t p1       = legacy_c1_p1( input, output, state.remote_address(), security_functions().local_address() );
        const details::uint128_t p2       = legacy_c1_p2( state.remote_address(), security_functions().local_address() );

        state.legacy_pairing_request( srand, p1, p2 );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    inline void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::create_pairing_response( std::uint8_t* output, std::size_t& out_size, const io_capabilities_t& io_caps )
    {
        out_size = pairing_req_resp_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_response );
        output[ 1 ] = io_caps[ 0 ];
        output[ 2 ] = io_caps[ 1 ];
        output[ 3 ] = io_caps[ 2 ];
        output[ 4 ] = max_max_key_size;
        output[ 5 ] = 0;
        output[ 6 ] = key_distribution_t::request_key_flags;
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_handle_pairing_confirm(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        using namespace details;

        static constexpr std::size_t    request_size = 17;

        if ( in_size != request_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != sm_pairing_state::legacy_pairing_requested )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        // save mconfirm for later
        state.pairing_confirm( &input[ 1 ], &input[ request_size ] );

        out_size = request_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_confirm );

        const auto temp_key = legacy_create_temporary_key( state );

        io_device_t::sm_pairing_numeric_output( temp_key );

        const auto sconfirm = security_functions().c1( temp_key, state.srand(), state.c1_p1(), state.c1_p2() );
        std::copy( sconfirm.begin(), sconfirm.end(), &output[ 1 ] );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_handle_pairing_random(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        using namespace details;

        static constexpr std::size_t    pairing_random_size = 17;

        if ( in_size != pairing_random_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != sm_pairing_state::legacy_pairing_confirmed )
            return this->error_response( sm_error_codes::unspecified_reason, output, out_size, state );

        uint128_t mrand;
        std::copy( &input[ 1 ], &input[ pairing_random_size ], mrand.begin() );
        const uint128_t temp_key = legacy_temporary_key( state );

        const auto mconfirm = security_functions().c1( temp_key, mrand, state.c1_p1(), state.c1_p2() );

        if ( mconfirm != state.mconfirm() )
            return this->error_response( sm_error_codes::confirm_value_failed, output, out_size, state );

        out_size = pairing_random_size;
        output[ 0 ] = static_cast< std::uint8_t >( sm_opcodes::pairing_random );

        const auto srand = state.srand();
        std::copy( srand.begin(), srand.end(), &output[ 1 ] );

        const auto stk = security_functions().s1( temp_key, srand, mrand );
        state.legacy_pairing_completed( stk );
        state.arm_key_distribution( security_functions(), state );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    details::uint128_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_create_temporary_key( Connection& state )
    {
        const auto algo = state.legacy_pairing_algorithm();

        switch ( algo )
        {
            case legacy_pairing_algorithm::oob_authentication:
                return this->get_oob_data_for_last_remote_device();

            case legacy_pairing_algorithm::passkey_entry_display:
            {
                const auto key = security_functions().create_passkey();
                state.passkey( key );

                return key;
            }

            case legacy_pairing_algorithm::passkey_entry_input:
            {

                const auto key = io_device_t::sm_pairing_passkey();
                state.passkey( key );

                return key;
            }

            default:
                break;
        }

        return uint128_t( { 0 } );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    details::uint128_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_temporary_key( const Connection& state ) const
    {
        const auto algo = state.legacy_pairing_algorithm();

        switch ( algo )
        {
            case legacy_pairing_algorithm::oob_authentication:
                return this->get_oob_data_for_last_remote_device();

            case legacy_pairing_algorithm::passkey_entry_display:
            case legacy_pairing_algorithm::passkey_entry_input:
                return state.passkey();

            default:
                break;
        }

        return uint128_t( { 0 } );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    details::legacy_pairing_algorithm details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_select_pairing_algorithm( std::uint8_t io_capability, std::uint8_t oob_data_flag, std::uint8_t /* auth_req */, bool has_oob_data )
    {
        if ( oob_data_flag && has_oob_data )
            return details::legacy_pairing_algorithm::oob_authentication;

        return io_device_t::select_legacy_pairing_algorithm( io_capability );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    inline details::uint128_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_c1_p1(
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

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    inline details::uint128_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_c1_p2(
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

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_handle_pairing_request(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        using namespace details;

        if ( in_size != pairing_req_resp_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::sm_pairing_state::idle )
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

        state.pairing_algorithm( lesc_select_pairing_algorithm( io_capability, oob_data_flag, auth_req, this->has_oob_data_for_remote_device() ) );
        state.pairing_requested( remote_io_caps );
        create_pairing_response( output, out_size, lesc_local_io_caps() );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    details::io_capabilities_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::legacy_local_io_caps() const
    {
        static constexpr std::uint8_t oob_authentication_data_not_present                = 0x00;
        static constexpr std::uint8_t oob_authentication_data_from_remote_device_present = 0x01;

        return {{
            static_cast< std::uint8_t >( io_device_t::get_io_capabilities() ),
            this->has_oob_data_for_remote_device()
            ? oob_authentication_data_from_remote_device_present
            : oob_authentication_data_not_present,
            authentication_requirements_flags }};
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    details::io_capabilities_t details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_local_io_caps() const
    {
        return {{
            static_cast< std::uint8_t >( io_device_t::get_io_capabilities() ),
            0,
            authentication_requirements_flags | static_cast< std::uint8_t >( details::authentication_requirements_flags::secure_connections ) }};
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_handle_pairing_public_key(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        if ( in_size != public_key_exchange_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::sm_pairing_state::lesc_pairing_requested )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        assert( out_size >= public_key_exchange_size );

        if ( !security_functions().is_valid_public_key( &input[ 1 ] ) )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_public_key );

        out_size = public_key_exchange_size;
        const auto& keys  = security_functions().generate_keys();
        const auto& nonce = security_functions().select_random_nonce();

        state.public_key_exchanged( keys.second, keys.first, &input[ 1 ], nonce );
        std::copy( keys.first.begin(), keys.first.end(), &output[ 1 ] );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_handle_pairing_random( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        if ( in_size != pairing_random_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::sm_pairing_state::lesc_pairing_confirm_send )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        state.pairing_random_exchanged( input + 1 );

        if ( state.lesc_pairing_algorithm() == lesc_pairing_algorithm::numeric_comparison )
        {
            io_device_t::sm_pairing_numeric_compare_output( state, security_functions() );
            io_device_t::sm_pairing_request_yes_no( state );
        }

        if ( state.state() == details::sm_pairing_state::user_response_failed )
            return this->error_response( details::sm_error_codes::passkey_entry_failed, output, out_size, state );

        const auto& nonce = state.local_nonce();
        out_size = pairing_random_size;
        output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_random );
        std::copy( nonce.begin(), nonce.end(), &output[ 1 ] );
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_handle_pairing_dhkey_check( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        if ( in_size != pairing_dhkey_check_size )
            return this->error_response( details::sm_error_codes::invalid_parameters, output, out_size, state );

        static const auto expected_states = {
            details::sm_pairing_state::lesc_pairing_random_exchanged,
            details::sm_pairing_state::user_response_wait,
            details::sm_pairing_state::user_response_failed,
            details::sm_pairing_state::user_response_success
        };

        if ( std::find( std::begin( expected_states ), std::end( expected_states ), state.state() ) == std::end( expected_states ) )
            return this->error_response( details::sm_error_codes::unspecified_reason, output, out_size, state );

        if ( state.state() == details::sm_pairing_state::user_response_wait )
        {
            out_size = 0;
        }
        else if ( state.state() == details::sm_pairing_state::user_response_failed )
        {
            return this->error_response( details::sm_error_codes::passkey_entry_failed, output, out_size, state );
        }
        else
        {
            const details::ecdh_shared_secret_t dh_key = security_functions().p256( state.local_private_key(), state.remote_public_key() );

            details::uint128_t mac_key;
            details::uint128_t ltk;
            static const details::uint128_t zero = {{ 0 }};

            std::tie( mac_key, ltk ) = security_functions().f5( dh_key, state.remote_nonce(), state.local_nonce(), state.remote_address(), security_functions().local_address() );

            const auto calc_ea = security_functions().f6( mac_key, state.remote_nonce(), state.local_nonce(), zero, state.remote_io_caps(), state.remote_address(), security_functions().local_address() );

            if ( !std::equal( calc_ea.begin(), calc_ea.end(), &input[ 1 ] ) )
                return this->error_response( details::sm_error_codes::dhkey_check_failed, output, out_size, state );

            const auto eb = security_functions().f6( mac_key, state.local_nonce(), state.remote_nonce(), zero, lesc_local_io_caps(), security_functions().local_address(), state.remote_address() );

            out_size = pairing_dhkey_check_size;
            output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_dhkey_check );
            std::copy( eb.begin(), eb.end(), &output[ 1 ] );

            state.lesc_pairing_completed( ltk );
            state.store_lesc_key_in_bond_db( ltk, state );
        }
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    bool details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_security_manager_output_available( Connection& state ) const
    {
        return state.state() == details::sm_pairing_state::lesc_public_keys_exchanged
            || state.state() == details::sm_pairing_state::user_response_success
            || state.state() == details::sm_pairing_state::user_response_failed;
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        assert( lesc_security_manager_output_available( state ) );

        if ( state.state() == details::sm_pairing_state::lesc_public_keys_exchanged )
        {
            const auto& Nb = state.local_nonce();
            const std::uint8_t* Pkax = state.remote_public_key_x();
            const std::uint8_t* PKbx = state.local_public_key_x();

            const auto confirm = security_functions().f4( PKbx, Pkax, Nb, 0 );

            out_size = this->pairing_confirm_size;
            output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_confirm );
            std::copy( confirm.begin(), confirm.end(), &output[ 1 ] );

            state.pairing_confirm_send();
        }
        else if ( state.state() == details::sm_pairing_state::user_response_success )
        {
            const details::ecdh_shared_secret_t dh_key = security_functions().p256( state.local_private_key(), state.remote_public_key() );

            details::uint128_t mac_key;
            details::uint128_t ltk;
            static const details::uint128_t zero = {{ 0 }};

            std::tie( mac_key, ltk ) = security_functions().f5( dh_key, state.remote_nonce(), state.local_nonce(), state.remote_address(), security_functions().local_address() );
            const auto eb = security_functions().f6( mac_key, state.local_nonce(), state.remote_nonce(), zero, this->lesc_local_io_caps(), security_functions().local_address(), state.remote_address() );

            out_size = this->pairing_dhkey_check_size;
            output[ 0 ] = static_cast< std::uint8_t >( details::sm_opcodes::pairing_dhkey_check );
            std::copy( eb.begin(), eb.end(), &output[ 1 ] );

            state.lesc_pairing_completed( ltk );
            state.store_lesc_key_in_bond_db( ltk, state );
        }
        else
        {
            this->error_response( details::sm_error_codes::passkey_entry_failed, output, out_size, state );
        }

    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    details::lesc_pairing_algorithm details::security_manager_base< SecurityFunctions, ConnectionData, Options... >::lesc_select_pairing_algorithm( std::uint8_t io_capability, std::uint8_t oob_data_flag, std::uint8_t /* auth_req */, bool has_oob_data )
    {
        if ( oob_data_flag || has_oob_data )
            return details::lesc_pairing_algorithm::oob_authentication;

        return io_device_t::select_lesc_pairing_algorithm( io_capability );
    }

    // Legacy
    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::legacy_security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_input(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
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
                this->legacy_handle_pairing_request( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_confirm:
                this->legacy_handle_pairing_confirm( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_random:
                this->legacy_handle_pairing_random( input, in_size, output, out_size, state );
                break;
            default:
                this->error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::legacy_security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        state.distribute_keys( output, out_size, state );
    }

    // LESC
    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::lesc_security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_input(
        const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
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
                this->lesc_handle_pairing_request( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_public_key:
                this->lesc_handle_pairing_public_key( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_random:
                this->lesc_handle_pairing_random( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_dhkey_check:
                this->lesc_handle_pairing_dhkey_check( input, in_size, output, out_size, state );
                break;
            default:
                this->error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::lesc_security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        if ( this->lesc_security_manager_output_available( state ) )
        {
            this->lesc_l2cap_output( output, out_size, state );
        }
        else
        {
            out_size = 0;
        }
    }

    // security_manager_impl
    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
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
                handle_pairing_request( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_confirm:
                this->legacy_handle_pairing_confirm( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_random:
                if ( state.state() == sm_pairing_state::legacy_pairing_confirmed )
                {
                    this->legacy_handle_pairing_random( input, in_size, output, out_size, state );
                }
                else
                {
                    this->lesc_handle_pairing_random( input, in_size, output, out_size, state );
                }
                break;
            case sm_opcodes::pairing_public_key:
                this->lesc_handle_pairing_public_key( input, in_size, output, out_size, state );
                break;
            case sm_opcodes::pairing_dhkey_check:
                this->lesc_handle_pairing_dhkey_check( input, in_size, output, out_size, state );
                break;
            default:
                this->error_response( sm_error_codes::command_not_supported, output, out_size, state );
        }
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_impl< SecurityFunctions, ConnectionData, Options... >::l2cap_output( std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        if ( this->lesc_security_manager_output_available( state ) )
        {
            this->lesc_l2cap_output( output, out_size, state );
        }
        else
        {
            state.distribute_keys( output, out_size, state );
        }
    }

    template < typename SecurityFunctions, template < class OtherConnectionData > class ConnectionData, typename ... Options >
    template < class Connection >
    void details::security_manager_impl< SecurityFunctions, ConnectionData, Options... >::handle_pairing_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size, Connection& state )
    {
        using namespace details;

        if ( in_size != this->pairing_req_resp_size )
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );

        if ( state.state() != details::sm_pairing_state::idle )
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
         || ( max_key_size < this->min_max_key_size || max_key_size > this->max_max_key_size )
         || ( initiator_key_distribution & 0xf0 )
         || ( responder_key_distribution & 0xf0 )
        )
        {
            return this->error_response( sm_error_codes::invalid_parameters, output, out_size, state );
        }

        this->request_oob_data_presents_for_remote_device( state.remote_address() );

        const bool lesc_pairing = ( auth_req & static_cast< std::uint8_t >(authentication_requirements_flags::secure_connections ) );

        if ( lesc_pairing )
        {
            const io_capabilities_t remote_io_caps = {{ io_capability, oob_data_flag, auth_req }};
            state.pairing_algorithm( this->lesc_select_pairing_algorithm( io_capability, oob_data_flag, auth_req, this->has_oob_data_for_remote_device() ) );
            state.pairing_requested( remote_io_caps );

            this->create_pairing_response( output, out_size, this->lesc_local_io_caps() );
        }
        else
        {
            state.pairing_algorithm( this->legacy_select_pairing_algorithm( io_capability, oob_data_flag, auth_req, this->has_oob_data_for_remote_device() ) );

            this->create_pairing_response( output, out_size, this->lesc_local_io_caps() );

            const details::uint128_t srand    = security_functions().create_srand();
            const details::uint128_t p1       = this->legacy_c1_p1( input, output, state.remote_address(), security_functions().local_address() );
            const details::uint128_t p2       = this->legacy_c1_p2( state.remote_address(), security_functions().local_address() );

            state.legacy_pairing_request( srand, p1, p2 );
        }
    }

    // no_security_manager
    template < typename SecurityFunctions, typename ...Os >
    template < class Connection >
    void no_security_manager::impl< SecurityFunctions, Os... >::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size, Connection& )
    {
        error_response( details::sm_error_codes::pairing_not_supported, output, out_size );
    }

    template < typename SecurityFunctions, typename ...Os >
    template < class Connection >
    void no_security_manager::impl< SecurityFunctions, Os... >::l2cap_output( std::uint8_t*, std::size_t& out_size, Connection& )
    {
        out_size = 0;
    }
    /** @endcond */
}

#endif // include guard
