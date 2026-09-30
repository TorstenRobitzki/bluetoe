#ifndef BLUETOE_LINK_LAYER_PROCEDURES_HPP
#define BLUETOE_LINK_LAYER_PROCEDURES_HPP

#include <bluetoe/buffer.hpp>

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

    /**
     * @brief list of supported and implemented link layer procedures
     */
    template < class ... Procs >
    class procedure_list
    {
    public:
        // the state of all procedures of one link; the link data derives from it
        struct state_type {};

        /*
         * A received LL control PDU: the opcode and its parameters, without the header of
         * the data channel PDU. The procedures get the whole link, but own only their
         * state_type part of it.
         */
        template < class LinkLayer, class LinkData >
        static procedure_result handle_control_pdu( LinkLayer& link_layer, LinkData& link, std::span< const std::uint8_t > pdu )
            requires std::derived_from< LinkData, state_type >;

    private:
        static constexpr std::uint8_t   ll_control_pdu_code         = 3;

        static constexpr std::uint8_t   LL_UNKNOWN_RSP              = 0x07;
    };


    // implementation
    template < class ... Procs >
    template < class LinkLayer, class LinkData >
    procedure_result procedure_list< Procs... >::handle_control_pdu( LinkLayer& /*link_layer*/, LinkData& link, std::span< const std::uint8_t > pdu )
        requires std::derived_from< LinkData, state_type >
    {
        using layout_t = decltype(link.buffers)::layout;

        assert( pdu.size() != 0 );
        const std::uint8_t opcode = pdu[ 0 ];

        const auto out_buffer = link.buffers.allocate_ll_transmit_buffer( 2 );
        if ( out_buffer.size == 0 )
            return procedure_result::stalled();

        fill< layout_t >( out_buffer, { ll_control_pdu_code, 2, LL_UNKNOWN_RSP, opcode } );
        link.buffers.commit_ll_transmit_buffer( out_buffer );

        return procedure_result::handled();
    }

}
}
}

#endif

