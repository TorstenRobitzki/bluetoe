#ifndef BLUETOE_LINK_LAYER_PROCEDURES_HPP
#define BLUETOE_LINK_LAYER_PROCEDURES_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/ll_ids.hpp>

#include <concepts>
#include <cstdint>
#include <span>
#include <cassert>

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
            LL_TERMINATE_IND        = 0x02,
            LL_UNKNOWN_RSP          = 0x07,
            LL_VERSION_IND          = 0x0C,
            LL_PING_REQ             = 0x12,
            LL_PING_RSP             = 0x13,
        };
    }

    /**
     * @brief list of supported and implemented link layer procedures
     */
    template < class ... Procs >
    class procedure_list
    {
    public:
        // the state of all procedures of one link; the link data derives from it
        struct state_type : Procs::state_type... {};

        /*
         * A received LL control PDU
         */
        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
            requires std::derived_from< LinkData, state_type >;

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
    };

    ///////////////////////
    // implementation

    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list< Procs... >::handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
        requires std::derived_from< LinkData, state_type >
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

}
}
}

#endif

