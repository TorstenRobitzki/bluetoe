#ifndef BLUETOE_LINK_LAYER_PROCEDURES_HPP
#define BLUETOE_LINK_LAYER_PROCEDURES_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/ll_ids.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/connection_parameters.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/channel_map.hpp>

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
        enum : std::uint8_t
        {
            LL_CONNECTION_UPDATE_IND    = 0x00,
            LL_CHANNEL_MAP_IND          = 0x01,
            LL_TERMINATE_IND            = 0x02,
            LL_UNKNOWN_RSP              = 0x07,
            LL_VERSION_IND              = 0x0C,
            LL_PING_REQ                 = 0x12,
            LL_PING_RSP                 = 0x13,
        };
    }

    template < class LinkData, class ProcedureList >
    concept procedure_list_link_data = std::derived_from< LinkData, typename ProcedureList::state_type >;

    /**
     * @brief list of supported and implemented link layer procedures
     */
    template < class ... Procs >
    class procedure_list
    {
    public:
        // the state of all procedures of one link; the link data derives from it
        struct state_type : Procs::state_type...
        {
            // a union that contains all instant_state. As there can be only one instant
            // handled per time, the required memory can overlap
            // Add an int at the end to support an empty procedure list.
            std::variant< typename Procs::instant_state..., int > procedures_instant_state;

            // if set to an value, a procedure with instant is running
            std::optional< std::uint16_t > procedures_instant;
        };

        /*
         * A received LL control PDU
         */
        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
            requires procedure_list_link_data< LinkData, procedure_list< Procs... > >;

        /**
         * @brief see, if there has something to be done for the current instant at the current connection event
         */
        template < class LinkLayer, class LinkData >
        static procedure_result connection_event( LinkLayer& /*link_layer*/, LinkData& /*link*/ )
            requires procedure_list_link_data< LinkData, procedure_list< Procs... > >;

    private:
        static constexpr std::uint8_t   LL_UNKNOWN_RSP_SIZE         = 2;
    };

    /**
     * @brief the implementation of the LE ping link layer procedure
     */
    class le_ping_procedure
    {
    public:
        static constexpr std::uint8_t opcode        = opcodes::LL_PING_REQ;
        static constexpr std::uint8_t ctr_data_size = 0;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            if ( const auto write = link.buffers.allocate_ll_transmit_buffer( 1 );
                write.size != 0 )
            {
                fill< typename decltype(link.buffers)::layout >( write, { llid::ll_control_pdu_code, 1, opcodes::LL_PING_RSP } );
                link.buffers.commit_ll_transmit_buffer( write );

                return procedure_result::handled();
            }

            return procedure_result::stalled();
        }

        struct state_type {};
        struct instant_state {};
    };

    /**
     * @brief implementation of the version exchange procedure
     */
    class version_exchange_procedure
    {
    public:
        static constexpr std::uint8_t opcode        = opcodes::LL_VERSION_IND;
        static constexpr std::uint8_t ctr_data_size = 5;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > /* pdu */ )
        {
            const std::uint8_t response_size = 6;

            version_exchange_procedure::state_type& state = link;

            if ( state.version_exchanged )
                return procedure_result::handled();

            if ( const auto write = link.buffers.allocate_ll_transmit_buffer( response_size );
                write.size != 0 )
            {
                const std::uint16_t company_identifier = link_layer.link_layer_company_identifier();

                fill< typename decltype(link.buffers)::layout >( write, {
                    llid::ll_control_pdu_code, response_size, opcode,
                    link_layer.supported_link_layer_version(),
                    static_cast< std::uint8_t >( company_identifier ),
                    static_cast< std::uint8_t >( company_identifier >> 8 ),
                    0x00, 0x00
                } );

                link.buffers.commit_ll_transmit_buffer( write );
                state.version_exchanged = true;

                return procedure_result::handled();
            }

            return procedure_result::stalled();
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
        static constexpr std::uint8_t opcode        = opcodes::LL_TERMINATE_IND;
        static constexpr std::uint8_t ctr_data_size = 1;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& /*link*/, std::span< const std::uint8_t > pdu )
        {
            return procedure_result::disconnect( pdu[ 1 ] );
        }

        struct state_type {};
        struct instant_state {};
    };

    class procedure_with_instant
    {
    protected:
        template < class InstantState, class LinkData >
        static procedure_result apply_and_check_instant( const InstantState& state, LinkData& link, const std::uint16_t instant )
        {
            assert( !link.procedures_instant.has_value() );

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
        static constexpr std::uint8_t opcode        = opcodes::LL_CHANNEL_MAP_IND;
        static constexpr std::uint8_t ctr_data_size = 7;

        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        {
            channel_map new_map;
            if ( !new_map.reset( pdu.data() + 1, link.parameters.channels().hop() ) )
                return procedure_result::disconnect( controller_error_codes::invalid_ll_parameters );

            return apply_and_check_instant( new_map, link, bluetoe::details::read_16bit( pdu.data() + 5 + 1 ) );
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
        using instant_state = channel_map;
    };

    ///////////////////////
    // implementation

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list< Procs... >::handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
        requires procedure_list_link_data< LinkData, procedure_list< Procs... > >
    {
        assert( pdu.size() <= 0xff );

        if ( pdu.size() == 0 )
            return procedure_result::handled();

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
    auto call_procedure_connection_event( LinkLayer& link_layer, LinkData& link, procedure_result& result, int = 0 )
     -> decltype( Proc::connection_event( link_layer, link, result ) )
    {
        return Proc::connection_event( link_layer, link, result );
    }

    template < class Proc, class LinkLayer, class LinkData >
    bool call_procedure_connection_event( LinkLayer& /* link_layer */, LinkData& /* link */, procedure_result& /* result */, double = 0 )
    {
        return false;
    }

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list< Procs... >::connection_event( LinkLayer& link_layer, LinkData& link )
        requires procedure_list_link_data< LinkData, procedure_list< Procs... > >
    {
        procedure_result result = procedure_result::handled();

        state_type& state = link;

        if ( state.procedures_instant.has_value() && *state.procedures_instant == link.connection_event_counter() )
        {
            [[maybe_unused]] const int handled =
                ( ... + ( call_procedure_connection_event< Procs >( link_layer, link, result, 0 ) ? 1 : 0 ) );

            assert( handled == 1 );

            state.procedures_instant.reset();
        }

        return result;
    }

}
}
}

#endif

