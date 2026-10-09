#ifndef BLUETOE_LINK_LAYER_PROCEDURES_HPP
#define BLUETOE_LINK_LAYER_PROCEDURES_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/security_connection_data.hpp>
#include <bluetoe/ll_ids.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/connection_parameters.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/channel_map.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <concepts>
#include <cstdint>
#include <span>
#include <cassert>
#include <variant>
#include <optional>

namespace bluetoe {
namespace link_layer {
namespace details {

    /*
     * The result / branch of handling a control PDU.
     */
    enum procedure_outcome : std::uint8_t
    {
        // The PDU was handled; the caller frees it.
        handled,

        // The PDU needs an answer and there is no room for it. It has to wait in the receive
        // buffer.
        stalled,

        // The connection ends now, without a PDU to the peer, for example because an
        // instant has passed. A graceful end is a procedure of its own that returns this
        // only once its LL_TERMINATE_IND was acknowledged.
        disconnect
    };

    /*
     * The combination of a procedure_outcome with a reason for a disconnect
     */
    struct procedure_result
    {
        procedure_outcome   outcome;

        // the error code reported to the application; only meaningful for a disconnect
        std::uint8_t        reason;

        static constexpr procedure_result handled()
        {
            return { procedure_outcome::handled, 0 };
        }

        static constexpr procedure_result stalled()
        {
            return { procedure_outcome::stalled, 0 };
        }

        static constexpr procedure_result disconnect( std::uint8_t reason )
        {
            return { procedure_outcome::disconnect, reason };
        }

        bool operator!=( const procedure_result& rhs ) const
        {
            return ( outcome != rhs.outcome )
                || ( outcome == procedure_outcome::disconnect && reason != rhs.reason );
        }

        bool operator==( const procedure_result& rhs ) const
        {
            return !( *this != rhs );
        }
    };

    // the requirements follow with the first procedure
    template < class Procedure >
    concept procedure = true;

    namespace opcodes {
        enum opcodes_t : std::uint8_t
        {
            LL_CONNECTION_UPDATE_IND    = 0x00,
            LL_CHANNEL_MAP_IND          = 0x01,
            LL_TERMINATE_IND            = 0x02,
            LL_ENC_REQ                  = 0x03,
            LL_ENC_RSP                  = 0x04,
            LL_START_ENC_REQ            = 0x05,
            LL_START_ENC_RSP            = 0x06,
            LL_UNKNOWN_RSP              = 0x07,
            LL_FEATURE_REQ              = 0x08,
            LL_FEATURE_RSP              = 0x09,
            LL_PAUSE_ENC_REQ            = 0x0A,
            LL_PAUSE_ENC_RSP            = 0x0B,
            LL_VERSION_IND              = 0x0C,
            LL_REJECT_IND               = 0x0D,
            LL_CONNECTION_PARAM_REQ     = 0x0F,
            LL_CONNECTION_PARAM_RSP     = 0x10,
            LL_REJECT_EXT_IND           = 0x11,
            LL_PING_REQ                 = 0x12,
            LL_PING_RSP                 = 0x13,
            LL_PHY_REQ                  = 0x16,
            LL_PHY_RSP                  = 0x17,
            LL_PHY_UPDATE_IND           = 0x18,
        };
    }

    template < class LinkData, class ProcedureList >
    concept procedure_list_link_data = std::derived_from< LinkData, typename ProcedureList::state_type >;

    /**
     * @brief enum that gives the bit position of the feature flag in
     *        a feature mask.
     */
    enum class link_layer_feature {
        le_encryption                           = 0,
        connection_parameters_request_procedure = 1,
        extended_reject_indication              = 2,
        peripheral_initiated_features_exchange  = 3,
        le_ping                                 = 4,
        le_data_packet_length_extension         = 5,
        ll_privacy                              = 6,
        extended_scanner_filter_policies        = 7,
        le_2m_phy_support                       = 8
    };

    // The size in bits, it takes to keep the Procs feature flag in a mask
    template < class Proc >
    constexpr std::size_t feature_flag_size()
    {
        if constexpr ( requires { { Proc::feature_flag } -> std::convertible_to<link_layer_feature>; } )
            return static_cast< std::size_t >( Proc::feature_flag ) + 1;
        else
            return 0;
    }

    template < class ... Procs >
    using feature_flag_mask_t_from_procs = bluetoe::details::uint_least_t< std::max( { std::size_t{0}, feature_flag_size< Procs >() ...} ) >;

    template < class Proc, class ... Procs >
    constexpr feature_flag_mask_t_from_procs< Procs... > feature_set()
    {
        if constexpr ( requires { { Proc::feature_flag } -> std::convertible_to<link_layer_feature>; } )
            return feature_flag_mask_t_from_procs< Procs... >( 1 ) << static_cast< std::size_t >( Proc::feature_flag );
        else
            return 0;
    }

    struct pending_procedure_rejection
    {
        opcodes::opcodes_t                  opcode;
        controller_error_codes::error_codes error;
    };

    /**
     * @brief a group of procedures that implement a link layer procedure together
     */
    template < class ... Procs >
    using procedure_group = std::tuple< Procs... >;

    template < class ProcsList >
    class procedure_list_impl;

    template < class ... Procs >
    class procedure_list_impl< std::tuple< Procs... > >
    {
    public:
        /**
         * @brief a type, that is large enough, to keep all feature flags, that are supported
         *        by the list.
         */
        using feature_flag_mask_t = feature_flag_mask_t_from_procs< Procs... >;

        // the state of all procedures of one link; the link data derives from it
        struct state_type : Procs::state_type...
        {
            // a union that contains all instant_state. As there can be only one instant
            // handled per time, the required memory can overlap
            // Add an int at the end to support an empty procedure list.
            std::variant< typename Procs::instant_state..., int > procedures_instant_state;

            // if set to an value, a procedure with instant is running
            std::optional< std::uint16_t >  procedures_instant;

            // the set of implemented features of this link layer procedure list
            // the default is support for LL_REJECT_EXT_IND
            static constexpr feature_flag_mask_t implemented_features = (
                ( feature_flag_mask_t{1} << static_cast< int >( link_layer_feature::extended_reject_indication ) )
                    | ... | feature_set< Procs, Procs... >() );

            // By default, the link layer will not use LL_REJECT_EXT_IND until it can
            // prove that the peer supports it
            static constexpr feature_flag_mask_t features_not_used_before_proved =
                ( feature_flag_mask_t{1} << static_cast< int >( link_layer_feature::extended_reject_indication ) );

            feature_flag_mask_t currently_used_features =
                implemented_features & ~features_not_used_before_proved;

            std::optional< pending_procedure_rejection > pending_reject;

            bool supports_feature( link_layer_feature feat ) const
            {
                return currently_used_features & ( feature_flag_mask_t{1} << static_cast< int >( feat ) );
            }

            void enable_reject_ext()
            {
                currently_used_features = currently_used_features
                    | ( feature_flag_mask_t{1} << static_cast< int >( link_layer_feature::extended_reject_indication ) );
            }
        };

        /**
         * @brief A received LL control PDU
         *
         * To be called on every received link layer control PDU.
         */
        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

        /**
         * @brief see, if there has something to be done for the current instant at the current connection event
         *
         * To be called on every connection event that actually happens. If a link layer control PDU
         * was received on the connection event, connection_event() has to be called before
         * handle_control_pdu().
         *
         * Must be called for every connection event the peripheral listens to;
         * the event of a pending instant is never skipped.
         */
        template < class LinkLayer, class LinkData >
        static procedure_result connection_event( LinkLayer& link_layer, LinkData& link )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

        /**
         * @brief connection got reset
         *
         * Has to be called, when a connection is closed and the same link object should
         * be reused for a next connection.
         */
        template < class LinkData >
        static void connection_reset( LinkData& link )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

        /**
         * @brief Returns true, if the encryption is currently changed and no data PDUs are expected
         *
         * The link layer is expected to check encryption_change_in_progress() for every incoming data PDU
         * and close the connection with error code 0x3D and to hold back all outgoing data PDUs if
         * encryption_change_in_progress() returns true.
         */
        template < class LinkData >
        static bool encryption_change_in_progress( const LinkData& link )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

    private:
        template < class LinkData >
        static procedure_result veto_control_pdu( LinkData& link, std::span< const std::uint8_t > pdu )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

        template < class LinkLayer, class LinkData >
        static bool send_pending_reject( LinkLayer&, LinkData& link )
            requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >;

        static constexpr std::uint8_t   LL_UNKNOWN_RSP_SIZE         = 2;
    };

    /**
     * @brief list of supported and implemented link layer procedures
     */
    template < class ... Procs >
    using procedure_list = procedure_list_impl<
        typename bluetoe::details::flatten< std::tuple< Procs... > >::type >;

    class procedure_base
    {
    public:
        template < class LinkData >
        static bool try_reject( LinkData& link, opcodes::opcodes_t opcode, controller_error_codes::error_codes error )
        {
            const bool supports_extended_reject = link.supports_feature( link_layer_feature::extended_reject_indication );

            if ( const auto write = link.buffers.allocate_ll_transmit_buffer( supports_extended_reject ? 3 : 2 );
                write.size != 0 )
            {
                using layout_t = typename decltype(link.buffers)::layout;

                if ( supports_extended_reject )
                {
                    fill< layout_t >( write, { llid::ll_control_pdu_code, 3, opcodes::LL_REJECT_EXT_IND, opcode, error } );
                }
                else
                {
                    fill< layout_t >( write, { llid::ll_control_pdu_code, 2, opcodes::LL_REJECT_IND, error } );
                }

                link.buffers.commit_ll_transmit_buffer( write );

                return true;
            }

            return false;
        }
    protected:
        template < class LinkData, class FillPdu >
        static procedure_result allocate_and_transmit( LinkData& link, std::size_t required_size, FillPdu fill )
        {
            if ( const auto write = link.buffers.allocate_ll_transmit_buffer( required_size );
                write.size != 0 )
            {
                fill( write );
                link.buffers.commit_ll_transmit_buffer( write );

                return procedure_result::handled();
            }

            return procedure_result::stalled();
        }

        template < class LinkData >
        static void reject( LinkData& link, opcodes::opcodes_t opcode, controller_error_codes::error_codes error )
        {
            assert( !link.pending_reject.has_value() );

            if ( !try_reject( link, opcode, error ) )
            {
                link.pending_reject = pending_procedure_rejection{
                    .opcode = opcode,
                    .error = error
                };
            }
        }
    };

    /**
     * @brief the implementation of the LE ping link layer procedure
     */
    class le_ping_procedure : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_PING_REQ;
        static constexpr std::uint8_t  ctr_data_size = 0;
        static constexpr link_layer_feature feature_flag  = link_layer_feature::le_ping;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            return allocate_and_transmit( link, 1, [&]( auto write )
            {
                fill< typename decltype(link.buffers)::layout >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_PING_RSP } );
            });
        }

        struct state_type {};
        struct instant_state {};
    };

    /**
     * @brief implementation of the version exchange procedure
     */
    class version_exchange_procedure : procedure_base
    {
    public:
        static constexpr std::uint8_t opcode         = opcodes::LL_VERSION_IND;
        static constexpr std::uint8_t ctr_data_size  = 5;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            const std::uint8_t response_size = 6;

            version_exchange_procedure::state_type& state = link;

            if ( state.version_exchanged )
                return procedure_result::handled();

            return allocate_and_transmit( link, response_size, [&]( auto write )
            {
                const std::uint16_t company_identifier = link_layer.link_layer_company_identifier();

                fill< typename decltype(link.buffers)::layout >( write, {
                    llid::ll_control_pdu_code, response_size, opcode,
                    link_layer.supported_link_layer_version(),
                    static_cast< std::uint8_t >( company_identifier ),
                    static_cast< std::uint8_t >( company_identifier >> 8 ),
                    0x00, 0x00
                } );

                state.version_exchanged = true;
            });
        }

        struct state_type {
            bool version_exchanged = false;
        };

        struct instant_state {};
    };

    /**
     * @brief implementation of the termination procedure
     */
    class termination_procedure
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_TERMINATE_IND;
        static constexpr std::uint8_t  ctr_data_size = 1;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& /*link*/, std::span< const std::uint8_t > pdu )
        {
            return procedure_result::disconnect( pdu[ 1 ] );
        }

        struct state_type {};
        struct instant_state {};
    };

    class feature_exchange_procedure : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_FEATURE_REQ;
        static constexpr std::uint8_t  ctr_data_size = 8;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            const std::uint8_t response_size = 9;

            return allocate_and_transmit( link, response_size, [&]( auto write )
            {
                using link_t = std::decay_t< decltype( link ) >;
                using mask_t = decltype( link.currently_used_features );

                const mask_t remote_mask = parse_mask< mask_t >( pdu );

                // enable features that where previously not used because it was
                // unknown whether the remote supports it or not
                link.currently_used_features = link.currently_used_features |
                    ( link_t::features_not_used_before_proved & remote_mask );

                // do not use features that are not supported by the remote side
                link.currently_used_features = link.currently_used_features & remote_mask;

                write_response( link, write, remote_mask );
            });
        }

        struct state_type {};
        struct instant_state {};

    private:
        template < class Mask >
        static Mask parse_mask( auto pdu )
        {
            Mask mask = 0;

            for ( std::size_t octet = 0; octet != sizeof( Mask ); ++octet )
                mask |= Mask( pdu[ 1 + octet ] ) << ( 8 * octet );

            return mask;
        }

        static void write_response( auto& link, auto write, auto remote_mask )
        {
            const std::uint8_t response_size = 9;

            using layout_t = typename decltype(link.buffers)::layout;

            layout_t::header( write, llid::ll_control_pdu_code | ( response_size << 8 ) );
            std::uint8_t* body = layout_t::body( write ).first;

            *body = opcodes::LL_FEATURE_RSP;
            ++body;

            std::size_t pos = 1;

            using mask_t = decltype( remote_mask );
            using link_t = std::decay_t< decltype( link ) >;

            const mask_t local_mask_0  = ( link_t::implemented_features ) & 0xff;
            const mask_t remote_mask_0 = remote_mask & 0xff;

            const mask_t outgoing_mask = ( local_mask_0 & remote_mask_0 )
                | ( link_t::implemented_features & ~mask_t{0xff} );

            for ( auto mask = outgoing_mask; pos != response_size; mask = mask >> 8 )
            {
                *body = static_cast< std::uint8_t >( mask & 0xff );
                ++body;
                ++pos;
            }
        }
    };

    class procedure_with_instant
    {
    protected:
        template < class InstantState, class LinkData >
        static procedure_result apply_and_check_instant( const InstantState& state, LinkData& link, const std::uint16_t instant )
        {
            if ( link.procedures_instant.has_value() )
                return procedure_result::disconnect(
                    std::holds_alternative< InstantState >( link.procedures_instant_state )
                        ? controller_error_codes::ll_procedure_collision
                        : controller_error_codes::different_transaction_collision );

            if ( static_cast< std::uint16_t >( instant - link.connection_event_counter() ) >= 0x7fff
                || instant == link.connection_event_counter() )
                return procedure_result::disconnect( controller_error_codes::instant_passed );

            link.procedures_instant_state = state;
            link.procedures_instant       = instant;

            return procedure_result::handled();
        }
    };

    /**
     * @brief implementation of a connection update indication
     */
    class connection_update_indication : public procedure_with_instant
    {
    public:
        static constexpr std::uint8_t opcode        = opcodes::LL_CONNECTION_UPDATE_IND;
        static constexpr std::uint8_t ctr_data_size = 11;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /* link_layer */, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            instant_state new_timing;

            if ( !new_timing.from_connection_update( pdu.data() ) )
                return procedure_result::disconnect( controller_error_codes::invalid_ll_parameters );

            return apply_and_check_instant( new_timing, link, bluetoe::details::read_16bit( pdu.data() + 9 + 1 ) );
        }

        template < class LinkLayer, class LinkData >
        static bool connection_event( LinkLayer& link_layer, LinkData& link, procedure_result& /* result */ )
        {
            assert( link.procedures_instant.has_value() );

            if ( !std::get_if< instant_state >( &link.procedures_instant_state ) )
                return false;

            link_layer.update_connection( link, get< instant_state >( link.procedures_instant_state ) );

            return true;
        }

        struct state_type {};
        using instant_state = connection_timing;
    };

    /**
     * @brief implementaion of a channel map update
     */
    class channel_map_update_procedure : public procedure_with_instant
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_CHANNEL_MAP_IND;
        static constexpr std::uint8_t  ctr_data_size = 7;

        static constexpr std::uint8_t  map_data_size = 5;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            if ( !channel_map::check_planned_map( pdu.subspan< 1, map_data_size>() ) )
            {
                return procedure_result::disconnect( controller_error_codes::invalid_ll_parameters );
            }

            instant_state new_map;
            std::copy( pdu.data() + 1, pdu.data() + 1 + map_data_size, new_map.begin() );

            return apply_and_check_instant( new_map, link, bluetoe::details::read_16bit( pdu.data() + map_data_size + 1 ) );
        }

        template < class LinkLayer, class LinkData >
        static bool connection_event( LinkLayer& link_layer, LinkData& link, procedure_result& /* result */ )
        {
            assert( link.procedures_instant.has_value() );

            if ( !std::get_if< instant_state >( &link.procedures_instant_state ) )
                return false;

            link_layer.update_channel_map( link, get< instant_state >( link.procedures_instant_state ) );

            return true;
        }

        struct state_type {};
        using instant_state = std::array< std::uint8_t, map_data_size >;
    };

    class phy_request : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_PHY_REQ;
        static constexpr std::uint8_t  ctr_data_size = 2;

        static constexpr link_layer_feature feature_flag  = link_layer_feature::le_2m_phy_support;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            static constexpr std::size_t response_size = 3;

            return allocate_and_transmit( link, response_size, [&]( auto write )
            {
                using layout_t = decltype(link.buffers)::layout;
                fill< layout_t >( write, {
                    llid::ll_control_pdu_code, response_size,
                    opcodes::LL_PHY_RSP,
                    phy_ll_encoding::le_2m_phy,
                    phy_ll_encoding::le_2m_phy } );
            });
        }

        struct state_type {};
        struct instant_state {};
    };

    class phy_update_indication : public procedure_with_instant
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_PHY_UPDATE_IND;
        static constexpr std::uint8_t  ctr_data_size = 4;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            const instant_state new_phy {
                .c_to_p  = pdu[ 1 ],
                .p_to_c  = pdu[ 2 ]
            };
            const std::uint16_t instant = bluetoe::details::read_16bit( pdu.data() + 2 + 1 );

            if ( !valid_phy_encoding( new_phy.c_to_p ) || !valid_phy_encoding( new_phy.p_to_c ) )
            {
                return procedure_result::disconnect( controller_error_codes::invalid_ll_parameters );
            }

            if ( !supported_phy_encoding( new_phy.c_to_p ) || !supported_phy_encoding( new_phy.p_to_c ) )
            {
                return procedure_result::disconnect( controller_error_codes::unsupported_ll_parameter_value );
            }

            if ( new_phy.c_to_p != phy_ll_encoding::le_unchanged_coding
              || new_phy.p_to_c != phy_ll_encoding::le_unchanged_coding )
            {
                return apply_and_check_instant( new_phy, link, instant );
            }

            return procedure_result::handled();
        }

        template < class LinkLayer, class LinkData >
        static bool connection_event( LinkLayer& link_layer, LinkData& link, procedure_result& /* result */ )
        {
            assert( link.procedures_instant.has_value() );

            const instant_state* const new_phy = std::get_if< instant_state >( &link.procedures_instant_state );
            if ( !new_phy )
                return false;

            link_layer.update_phy( link,
                static_cast< phy_ll_encoding::phy_ll_encoding_t >( new_phy->c_to_p ),
                static_cast< phy_ll_encoding::phy_ll_encoding_t >( new_phy->p_to_c ) );

            return true;
        }

        struct state_type {};
        struct instant_state {
            std::uint8_t c_to_p;
            std::uint8_t p_to_c;
        };

    private:
        static bool valid_phy_encoding( std::uint8_t c )
        {
            return c == phy_ll_encoding::le_unchanged_coding
                || c == phy_ll_encoding::le_1m_phy
                || c == phy_ll_encoding::le_2m_phy
                || c == phy_ll_encoding::le_coded_phy;
        }

        static bool supported_phy_encoding( std::uint8_t c )
        {
            return c == phy_ll_encoding::le_unchanged_coding
                || c == phy_ll_encoding::le_1m_phy
                || c == phy_ll_encoding::le_2m_phy;
        }
    };

    /**
     * @brief all procedures required to implement the link layer phy update procedure
     */
    using phy_update_procedure = procedure_group< phy_request, phy_update_indication >;

    /*
     * Encryption
     */

    struct encryption_state
    {
        bool has_key = false;

        enum {
            idle,
            started,
            waiting_start,
            paused
        } state = idle;
    };

    class encryption_request : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_ENC_REQ;
        static constexpr std::uint8_t  ctr_data_size = 22;

        static constexpr link_layer_feature feature_flag  = link_layer_feature::le_encryption;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            const std::size_t response_size = 1 + 8 + 4;

            return allocate_and_transmit( link, response_size, [&]( auto write ){
                const std::uint64_t rand = bluetoe::details::read_64bit( &pdu[ 1 ] );
                const std::uint16_t ediv = bluetoe::details::read_16bit( &pdu[ 9 ] );
                const std::uint64_t skdm = bluetoe::details::read_64bit( &pdu[ 11 ] );
                const std::uint32_t ivm  = bluetoe::details::read_32bit( &pdu[ 19 ] );
                      std::uint64_t skds = 0;
                      std::uint32_t ivs  = 0;

                using layout_t = decltype(link.buffers)::layout;

                fill< layout_t >( write, { llid::ll_control_pdu_code, response_size, opcodes::LL_ENC_RSP } );

                encryption_state& state = link;
                state.state = encryption_state::started;

                bluetoe::details::uint128_t key;
                std::tie( state.has_key, key ) = link_layer.find_key( link, ediv, rand );

                // setup encryption
                if ( state.has_key )
                    std::tie( skds, ivs ) = link_layer.setup_encryption( link, key, skdm, ivm );

                std::uint8_t* write_body = layout_t::body( write ).first;
                bluetoe::details::write_64bit( &write_body[ 1 ], skds );
                bluetoe::details::write_32bit( &write_body[ 9 ], ivs );
            } );
        }

        template < class LinkLayer, class LinkData >
        static bool transmit( LinkLayer& link_layer, LinkData& link )
        {
            static constexpr std::size_t response_size = 1;

            encryption_state& state = link;

            if ( state.state != encryption_state::started )
                return true;

            using layout_t = decltype(link.buffers)::layout;

            if ( state.has_key )
            {
                return allocate_and_transmit( link, response_size, [&]( auto write ){
                    fill< layout_t >( write, {
                        llid::ll_control_pdu_code, response_size, opcodes::LL_START_ENC_REQ } );

                    link_layer.encrypt_receive( link, true );
                    state.state = encryption_state::waiting_start;
                } ) == procedure_result::handled();
            }

            state.state = encryption_state::idle;
            reject( link, opcodes::LL_ENC_REQ, controller_error_codes::pin_or_key_missing );

            return false;
        }

        template < class LinkData >
        static bool encryption_change_in_progress( const LinkData& link )
        {
            const encryption_state& state = link;
            return state.state != encryption_state::idle;
        }

        static bool terminate_pdu( std::span< const std::uint8_t > pdu )
        {
            return pdu.size() == 2 && pdu[ 0 ] == opcodes::LL_TERMINATE_IND;
        }

        static bool start_enc_rsp_pdu( std::span< const std::uint8_t > pdu )
        {
            return pdu.size() == 1 && pdu[ 0 ] == opcodes::LL_START_ENC_RSP;
        }

        static bool pause_enc_rsp_pdu( std::span< const std::uint8_t > pdu )
        {
            return pdu.size() == 1 && pdu[ 0 ] == opcodes::LL_PAUSE_ENC_RSP;
        }

        template < class LinkData >
        static procedure_result veto_control_pdu( const LinkData& link, std::span< const std::uint8_t > pdu )
        {
            const encryption_state& state = link;
            assert( pdu.size() >= 1 );

            // default: disconnect when not idle
            if ( state.state != encryption_state::idle )
            {
                // terminate is always ok
                if ( terminate_pdu( pdu ) )
                    return procedure_result::handled();

                // when the LL_START_ENC_REQ was send, this is what we are waiting for
                if ( state.state == encryption_state::waiting_start && start_enc_rsp_pdu( pdu ) )
                    return procedure_result::handled();

                // if pause, we are waiting for the response
                if ( state.state == encryption_state::paused && pause_enc_rsp_pdu( pdu ) )
                    return procedure_result::handled();

                return procedure_result::disconnect( controller_error_codes::connection_terminated_due_to_mic_failure );
            }

            return procedure_result::handled();
        }

        using state_type = encryption_state;
        struct instant_state {};
    };

    class encryption_start_response : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_START_ENC_RSP;
        static constexpr std::uint8_t  ctr_data_size = 0;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            encryption_state& state = link;

            if ( state.state != encryption_state::waiting_start )
                return procedure_result::disconnect( controller_error_codes::connection_terminated_due_to_mic_failure );

            using layout_t = decltype(link.buffers)::layout;

            return allocate_and_transmit( link, 1, [&]( auto write ){
                fill< layout_t >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_START_ENC_RSP } );

                const bool encryption_changed = link_layer.encrypt_transmit( link, true );

                if ( encryption_changed )
                    link_layer.encryption_changed( link, true );

                state.state = encryption_state::idle;
            });
        }

        struct state_type {};
        struct instant_state {};
    };

    class encryption_pause_request : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_PAUSE_ENC_REQ;
        static constexpr std::uint8_t  ctr_data_size = 0;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            encryption_state& state = link;

            if ( state.state != encryption_state::idle )
                return procedure_result::disconnect( controller_error_codes::connection_terminated_due_to_mic_failure );

            using layout_t = decltype(link.buffers)::layout;

            return allocate_and_transmit( link, 1, [&]( auto write ){
                fill< layout_t >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_PAUSE_ENC_RSP } );

                const bool encryption_changed = link_layer.encrypt_receive( link, false );

                if ( encryption_changed )
                    link_layer.encryption_changed( link, false );

                state.state = encryption_state::paused;
            });
        }

        struct state_type {};
        struct instant_state {};
    };

    class encryption_pause_response : procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_PAUSE_ENC_RSP;
        static constexpr std::uint8_t  ctr_data_size = 0;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            encryption_state& state = link;

            if ( state.state != encryption_state::paused )
                return procedure_result::disconnect( controller_error_codes::connection_terminated_due_to_mic_failure );

            link_layer.encrypt_transmit( link, false );

            state.state = encryption_state::idle;

            return procedure_result::handled();
        }

        struct state_type {};
        struct instant_state {};
    };

    /**
     * @brief all procedures required to implement the link layer encryption procedure
     */
    using encryption_procedure = procedure_group<
        encryption_request, encryption_start_response, encryption_pause_request, encryption_pause_response >;

    /*
     * different flavours of connection parameter requests
     */

    template < class FillPdu >
    class connection_parameters_procedure_base : private procedure_base
    {
    public:
        static constexpr std::uint8_t  opcode        = opcodes::LL_CONNECTION_PARAM_REQ;
        static constexpr std::uint8_t  ctr_data_size = 23;

        static constexpr std::size_t   response_size = ctr_data_size + 1;

        static constexpr link_layer_feature feature_flag  = link_layer_feature::connection_parameters_request_procedure;

        struct state_type {};
        struct instant_state {};

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /* link_layer */, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            // with the peer sending a LL_CONNECTION_PARAM_REQ it proved, that it can handle LL_REJECT_EXT_IND
            link.enable_reject_ext();

            std::optional< requested_connection_parameters > params = parse_and_check_params( pdu );

            if ( !params.has_value() )
            {
                reject( link, opcodes::LL_CONNECTION_PARAM_REQ, controller_error_codes::invalid_ll_parameters );
                return procedure_result::handled();
            }

            return allocate_and_transmit( link, response_size, [&]( auto write ){
                using layout_t = decltype(link.buffers)::layout;

                fill< layout_t >( write, { llid::ll_control_pdu_code, response_size, opcodes::LL_CONNECTION_PARAM_RSP } );
                FillPdu::fill_pdu( *params, layout_t::body( write ).first + 1, pdu );
            } );
        }

    protected:
        struct requested_connection_parameters
        {
            std::uint16_t min_interval;
            std::uint16_t max_interval;
            std::uint16_t latency;
            std::uint16_t timeout;
        };

        static constexpr std::uint16_t interval_minimum = 6u;
        static constexpr std::uint16_t interval_maximum = 3200u;
        static constexpr std::uint16_t latency_maximum  = 499u;
        static constexpr std::uint16_t timeout_minimum  = 10;
        static constexpr std::uint16_t timeout_maximum  = 3200;

        static std::optional< requested_connection_parameters >  parse_and_check_params( std::span< const std::uint8_t > pdu )
        {
            const std::uint8_t* const body = pdu.data();

            using bluetoe::details::read_16bit;

            requested_connection_parameters params;

            params.min_interval = read_16bit( body + 1 );
            params.max_interval = read_16bit( body + 3 );
            params.latency      = read_16bit( body + 5 );
            params.timeout      = read_16bit( body + 7 );

            // check that raw data
            if ( params.max_interval < params.min_interval
              || params.min_interval < interval_minimum
              || params.max_interval > interval_maximum
              || params.latency > latency_maximum
              || params.timeout < timeout_minimum
              || params.timeout > timeout_maximum )
            {
                return {};
            }

            // the Link Layer shall ensure that the Timeout (in milliseconds)
            // is greater than 2 × Interval_Max × (Latency + 1).
            // timeout is given in units of 10ms interval in units of 1.25ms; 10/1.25 == 8
            if ( params.timeout * 8 <= 2 * params.max_interval * ( params.latency + 1 ) )
            {
                return {};
            }

            return { params };
        }
    };

    class connection_parameters_request_procedure
        : public connection_parameters_procedure_base< connection_parameters_request_procedure >
    {
    public:
        static void fill_pdu( const auto&, std::uint8_t* output, std::span< const std::uint8_t > pdu )
        {
            std::copy( pdu.begin() + 1, pdu.end(), output );
        }
    };

    template <
        std::uint16_t Interval_min,
        std::uint16_t Interval_max,
        std::uint16_t Latency_min,
        std::uint16_t Latency_max,
        std::uint16_t Timeout_min,
        std::uint16_t Timeout_max >
    class desired_connection_parameters_procedure
        : public connection_parameters_procedure_base<
            desired_connection_parameters_procedure< Interval_min, Interval_max, Latency_min, Latency_max, Timeout_min, Timeout_max > >
    {
    public:
        static void fill_pdu( auto& params, std::uint8_t* output, std::span< const std::uint8_t > /* pdu */)
        {
            params.min_interval = std::max( params.min_interval, Interval_min );
            params.max_interval = std::min( params.max_interval, Interval_max );

            if ( params.min_interval > params.max_interval )
            {
                params.min_interval = Interval_min;
                params.max_interval = Interval_max;
            }

            if ( params.latency < Latency_min || params.latency > Latency_max )
            {
                params.latency = ( Latency_min + Latency_max ) / 2;
            }

            if ( params.timeout < Timeout_min || params.timeout > Timeout_max )
            {
                params.timeout = ( Timeout_min + Timeout_max ) / 2;
            }

            bluetoe::details::write_16bit( output, params.min_interval );
            bluetoe::details::write_16bit( output + 2, params.max_interval );
            bluetoe::details::write_16bit( output + 4, params.latency );
            bluetoe::details::write_16bit( output + 6, params.timeout );
            output[ 8 ] = 0;
            bluetoe::details::write_16bit( output + 9, 0 );
            bluetoe::details::write_16bit( output + 11, 0xffff );
            bluetoe::details::write_16bit( output + 13, 0xffff );
            bluetoe::details::write_16bit( output + 15, 0xffff );
            bluetoe::details::write_16bit( output + 17, 0xffff );
            bluetoe::details::write_16bit( output + 19, 0xffff );
            bluetoe::details::write_16bit( output + 21, 0xffff );
        }
    private:
        using base_t = connection_parameters_procedure_base<
            desired_connection_parameters_procedure< Interval_min, Interval_max, Latency_min, Latency_max, Timeout_min, Timeout_max > >;

        static_assert( Interval_min >= base_t::interval_minimum, "Interval_min too small" );
        static_assert( Interval_max >= base_t::interval_minimum, "Interval_max too small" );
        static_assert( Interval_min <= base_t::interval_maximum, "Interval_min too large" );
        static_assert( Interval_max <= base_t::interval_maximum, "Interval_max too large" );
        static_assert( Interval_min <= Interval_max, "Interval_min should be smaller or equal to Interval_max" );
        static_assert( Latency_max <= base_t::latency_maximum, "Latency_max is too large" );
        static_assert( Latency_min <= base_t::latency_maximum, "Latency_min is too large" );
        static_assert( Latency_min <= Latency_max, "Latency_min should be smaller or equal to Latency_max" );
        static_assert( Timeout_min >= base_t::timeout_minimum, "Timeout_min too small" );
        static_assert( Timeout_max >= base_t::timeout_minimum, "Timeout_max too small" );
        static_assert( Timeout_min <= base_t::timeout_maximum, "Timeout_min too large" );
        static_assert( Timeout_max <= base_t::timeout_maximum, "Timeout_max too large" );
        static_assert( Timeout_min <= Timeout_max, "Timeout_min should be smaller or equal to Timeout_max" );
        static_assert( Timeout_min * 8 > 2 * Interval_max * ( Latency_max + 1 ), "the Link Layer shall ensure that the Timeout is greater than 2 × Interval_Max × (Latency + 1)" );

    };

    ///////////////////////
    // implementation
    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list_impl< std::tuple< Procs... > >::handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        assert( pdu.size() <= 0xff );

        if ( pdu.size() == 0 )
            return procedure_result::handled();

        const std::uint8_t opcode = pdu[ 0 ];
        [[maybe_unused]] const std::size_t  ctr_data_size = pdu.size() - 1;

        // there are states, where certain PDUs lead to disconnection
        procedure_result result = veto_control_pdu( link, pdu );
        if ( result.outcome == procedure_outcome::disconnect )
            return result;

        if ( link.pending_reject.has_value() )
            return procedure_result::stalled();

        const bool found =
            ( ... || ( Procs::opcode == opcode && Procs::ctr_data_size == ctr_data_size
            && ( result = Procs::handle_control_pdu( link_layer, link, pdu ), true ) ) );

        if ( found )
            return result;

        const auto out_buffer = link.buffers.allocate_ll_transmit_buffer( LL_UNKNOWN_RSP_SIZE );
        if ( out_buffer.size == 0 )
            return procedure_result::stalled();

        using layout_t = decltype(link.buffers)::layout;
        fill< layout_t >( out_buffer, { llid::ll_control_pdu_code, LL_UNKNOWN_RSP_SIZE, opcodes::LL_UNKNOWN_RSP, opcode } );
        link.buffers.commit_ll_transmit_buffer( out_buffer );

        return procedure_result::handled();
    }


    template < class Proc, class LinkLayer, class LinkData >
    bool call_procedure_connection_event( LinkLayer& link_layer, LinkData& link, procedure_result& result )
    {
        if constexpr ( requires { Proc::connection_event( link_layer, link, result ); } )
            return Proc::connection_event( link_layer, link, result );
        else
            return false;
    }

    template < class Proc, class LinkLayer, class LinkData >
    bool call_procedure_transmit( LinkLayer& link_layer, LinkData& link )
    {
        if constexpr ( requires { Proc::transmit( link_layer, link ); } )
        {
            return !link.pending_reject.has_value() && Proc::transmit( link_layer, link );
        }

        return true;
    }

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list_impl< std::tuple< Procs... > >::connection_event( LinkLayer& link_layer, LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        procedure_result result = procedure_result::handled();
        state_type&      state  = link;

        if ( state.procedures_instant.has_value() && *state.procedures_instant == link.connection_event_counter() )
        {
            [[maybe_unused]] const int handled =
                ( ... + ( call_procedure_connection_event< Procs >( link_layer, link, result ) ? 1 : 0 ) );

            assert( handled == 1 );

            state.procedures_instant.reset();
        }

        if ( result == procedure_result::handled() && state.pending_reject.has_value() )
        {
            if ( !send_pending_reject( link_layer, link ) )
                return result;

            state.pending_reject.reset();
        }

        if ( result == procedure_result::handled() )
        {
            // are there other procedures that need to send out data?
            [[maybe_unused]] const bool check = ( true && ... && call_procedure_transmit< Procs >( link_layer, link ) );
        }

        return result;
    }

    template < class ... Procs >
    template < class LinkData >
    void procedure_list_impl< std::tuple< Procs... > >::connection_reset( LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        static_cast< state_type& >( link ) = state_type();
    }

    template < class Proc, class LinkData >
    bool call_procedure_encryption_change_in_progress( const LinkData& link )
    {
        if constexpr ( requires { Proc::encryption_change_in_progress( link ); } )
        {
            return Proc::encryption_change_in_progress( link );
        }

        return false;
    }

    template < class ... Procs >
    template < class LinkData >
    bool procedure_list_impl< std::tuple< Procs... > >::encryption_change_in_progress( const LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        return ( false || ... || call_procedure_encryption_change_in_progress< Procs >( link ) );
    }

    template < class Proc, class LinkData >
    bool call_procedure_veto_control_pdu( LinkData& link, std::span< const std::uint8_t > pdu, procedure_result& result )
    {
        if constexpr ( requires { Proc::veto_control_pdu( link, pdu ); } )
        {
            result = Proc::veto_control_pdu( link, pdu );

            return result.outcome == procedure_outcome::disconnect;
        }

        return false;
    }

    template < class ... Procs >
    template < class LinkData >
    procedure_result procedure_list_impl< std::tuple< Procs... > >::veto_control_pdu( [[maybe_unused]] LinkData& link, [[maybe_unused]] std::span< const std::uint8_t > pdu )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        procedure_result result = procedure_result::handled();
        [[maybe_unused]] const bool check = ( false || ... || call_procedure_veto_control_pdu< Procs >( link, pdu, result ) );

        return result;
    }

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    bool procedure_list_impl< std::tuple< Procs... > >::send_pending_reject( LinkLayer&, LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        state_type& state = link;
        assert( state.pending_reject.has_value() );

        return procedure_base::try_reject( link, state.pending_reject->opcode, state.pending_reject->error );
    }

}
}
}

#endif

