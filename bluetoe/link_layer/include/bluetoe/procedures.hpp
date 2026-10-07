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
            LL_PING_REQ                 = 0x12,
            LL_PING_RSP                 = 0x13,
            LL_PHY_REQ                  = 0x16,
            LL_PHY_RSP                  = 0x17,
            LL_PHY_UPDATE_IND           = 0x18,
        };
    }

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

    template < class LinkData, class ProcedureList >
    concept procedure_list_link_data = std::derived_from< LinkData, typename ProcedureList::state_type >;

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
            static constexpr feature_flag_mask_t implemented_features = ( 0 | ... | feature_set< Procs, Procs... >() );

            feature_flag_mask_t currently_used_features = implemented_features;

            std::optional< pending_procedure_rejection > pending_reject;
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

    private:
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

            if ( const auto write = link.buffers.allocate_ll_transmit_buffer( 2 );
                write.size != 0 )
            {
                using layout_t = typename decltype(link.buffers)::layout;

                fill< layout_t >( write, { llid::ll_control_pdu_code, 2, opcodes::LL_REJECT_IND, error } );
                link.buffers.commit_ll_transmit_buffer( write );
            }
            else
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
                parse_mask( link, pdu );
                write_response( link, write );
            });
        }

        struct state_type {};
        struct instant_state {};

    private:
        static void parse_mask( auto& link, auto pdu )
        {
            using mask_t = decltype( link.currently_used_features );

            mask_t mask = 0;

            for ( std::size_t octet = 0; octet != sizeof( mask_t ); ++octet )
                mask |= mask_t( pdu[ 1 + octet ] ) << ( 8 * octet );

            link.currently_used_features = link.currently_used_features & mask;
        }

        static void write_response( auto& link, auto write )
        {
            const std::uint8_t response_size = 9;

            using layout_t = typename decltype(link.buffers)::layout;

            layout_t::header( write, llid::ll_control_pdu_code | ( response_size << 8 ) );
            std::uint8_t* body = layout_t::body( write ).first;

            *body = opcodes::LL_FEATURE_RSP;
            ++body;

            std::size_t pos = 1;
            for ( auto mask = link.currently_used_features; pos != response_size; mask = mask >> 8 )
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

            if ( static_cast< std::uint16_t >( instant - link.connection_event_counter() ) > 0x7fff
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
        bool pending_start_request = false;
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

                bluetoe::details::uint128_t key;
                encryption_state& state = link;
                state.pending_start_request = true;
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

            if ( !state.pending_start_request )
                return true;

            using layout_t = decltype(link.buffers)::layout;

            if ( state.has_key )
            {
                return allocate_and_transmit( link, response_size, [&]( auto write ){
                    fill< layout_t >( write, {
                        llid::ll_control_pdu_code, response_size, opcodes::LL_START_ENC_REQ } );

                    link_layer.encrypt_receive( link, true );
                    state.pending_start_request = false;
                } ) == procedure_result::handled();
            }

            state.pending_start_request = false;
            reject( link, opcodes::LL_ENC_REQ, controller_error_codes::pin_or_key_missing );

            return false;
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
            using layout_t = decltype(link.buffers)::layout;

            return allocate_and_transmit( link, 1, [&]( auto write ){
                fill< layout_t >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_START_ENC_RSP } );

                const bool encryption_changed = link_layer.encrypt_transmit( link, true );

                if ( encryption_changed )
                    link_layer.encryption_changed( link, true );

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
            using layout_t = decltype(link.buffers)::layout;

            return allocate_and_transmit( link, 1, [&]( auto write ){
                fill< layout_t >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_PAUSE_ENC_RSP } );

                const bool encryption_changed = link_layer.encrypt_receive( link, false );

                if ( encryption_changed )
                    link_layer.encryption_changed( link, false );

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
            link_layer.encrypt_transmit( link, false );

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

        if ( link.pending_reject.has_value() )
            return procedure_result::stalled();

        const std::uint8_t opcode = pdu[ 0 ];

        procedure_result result;
        const bool found =
            ( ... || ( Procs::opcode == opcode && Procs::ctr_data_size + 1 == pdu.size()
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

        state_type& state = link;

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

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    bool procedure_list_impl< std::tuple< Procs... > >::send_pending_reject( LinkLayer&, LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list_impl< std::tuple< Procs... > > >
    {
        state_type& state = link;
        assert( state.pending_reject.has_value() );

        if ( const auto write = link.buffers.allocate_ll_transmit_buffer( 2 );
            write.size != 0 )
        {
            using layout_t = typename decltype(link.buffers)::layout;

            fill< layout_t >( write, { llid::ll_control_pdu_code, 2, opcodes::LL_REJECT_IND, state.pending_reject->error } );

            link.buffers.commit_ll_transmit_buffer( write );
            state.pending_reject.reset();

            return true;
        }

        return false;
    }

}
}
}

#endif

