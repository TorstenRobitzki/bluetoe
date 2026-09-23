#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_HOST_PROGRAM_BUILDERS_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_HOST_PROGRAM_BUILDERS_HPP

/**
 * @file program_builders.hpp
 *
 * How a test writes the programs of the two instruments: a device program as
 * steps of calls, a tester program as operations, each built by a function named after what
 * it does, with times as std::chrono durations. What comes out is the wire type of
 * link/program.hpp and link/tester_program.hpp, which the host loads one request at a time.
 */

#include "link/program.hpp"
#include "link/tester_program.hpp"

#include <bluetoe/address.hpp>
#include <bluetoe/ll_constants.hpp>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief `duration` as the delta_time a program carries
     *
     * A builder takes nanoseconds, so that a test writes the unit that fits its value.
     *
     * @throw std::invalid_argument if `duration` is not a whole number of microseconds, or
     *        does not fit into delta_time's 32 bit of them
     */
    inline link_layer::delta_time as_delta_time( std::chrono::nanoseconds duration )
    {
        const auto in_usec = std::chrono::duration_cast< std::chrono::microseconds >( duration );

        if ( in_usec != duration )
            throw std::invalid_argument( "duration is not a whole number of microseconds" );

        if ( in_usec < std::chrono::microseconds::zero()
            || in_usec.count() > std::numeric_limits< std::uint32_t >::max() )
            throw std::invalid_argument( "duration does not fit into delta_time" );

        return link_layer::delta_time( static_cast< std::uint32_t >( in_usec.count() ) );
    }

    /**
     * @brief how long a tester operation may wait for what it waits for
     *
     * Named, so that an operation's window is not mistaken for the delay of its transmission.
     */
    struct time_out
    {
        explicit time_out( std::chrono::nanoseconds duration )
            : window( duration )
        {
        }

        std::chrono::nanoseconds    window;
    };

    /**
     * @brief a step of a device program: the callback it waits for and the calls it makes then
     *
     * The rig loads it as a step and one call after the other; the steps of a program share
     * max_calls calls.
     */
    struct step
    {
        callback_kind       on;
        std::vector< call > calls;
    };

    /**
     * @brief one operation of a tester program, with the PDUs a connection event sends
     *
     * The PDUs are loaded one request each, after the operation; every other operation is
     * one of these on its own, which is what the conversion is for.
     */
    struct tester_step
    {
        operation           op;
        std::vector< pdu >  pdus;

        tester_step( const operation& only ) : op( only ) {}
        tester_step( const operation& with_pdus, std::vector< pdu > pdus ) : op( with_pdus ), pdus( std::move( pdus ) ) {}
    };

    /**
     * @brief a step: the callback it waits for and the calls it makes then
     *
     * on_start() and the others name the callback; this one takes it, for a test that has
     * it in hand.
     */
    template < std::same_as< call >... Calls >
    step on( callback_kind kind, Calls... calls )
    {
        return step{ .on = kind, .calls = { calls... } };
    }

    /**
     * @brief a call a step makes
     * @{
     */
    inline call start_advertising_event( std::uint32_t channel, std::span< const std::uint8_t > transmit )
    {
        return call{
            .kind     = call_kind::start_advertising_event,
            .channel  = channel,
            .transmit = pdu( transmit ) };
    }

    inline call start_advertising_event(
        std::uint32_t channel, std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response )
    {
        return call{
            .kind     = call_kind::start_advertising_event,
            .channel  = channel,
            .transmit = pdu( transmit ),
            .response = adv_pdu( response ) };
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, std::chrono::nanoseconds delay, std::span< const std::uint8_t > transmit )
    {
        return call{
            .kind     = call_kind::schedule_advertising_event,
            .channel  = channel,
            .delay    = as_delta_time( delay ),
            .transmit = pdu( transmit ) };
    }

    inline call schedule_advertising_event(
        std::uint32_t channel, std::chrono::nanoseconds delay,
        std::span< const std::uint8_t > transmit, std::span< const std::uint8_t > response )
    {
        return call{
            .kind     = call_kind::schedule_advertising_event,
            .channel  = channel,
            .delay    = as_delta_time( delay ),
            .transmit = pdu( transmit ),
            .response = adv_pdu( response ) };
    }

    /**
     * @brief a connection event on `channel`, receiving from `start` and until `end` without a
     *        reception, both relative to the callback
     */
    inline call schedule_connection_event(
        std::uint32_t channel, std::chrono::nanoseconds start, std::chrono::nanoseconds end )
    {
        return call{
            .kind      = call_kind::schedule_connection_event,
            .channel   = channel,
            .delay     = as_delta_time( start ),
            .end_delay = as_delta_time( end ) };
    }

    inline call schedule_timer( std::chrono::nanoseconds delay )
    {
        return call{ .kind = call_kind::schedule_timer, .delay = as_delta_time( delay ) };
    }

    inline call cancel_timer()
    {
        return call{ .kind = call_kind::cancel_timer };
    }

    inline call cancel_radio_event()
    {
        return call{ .kind = call_kind::cancel_radio_event };
    }

    inline call set_local_address( const link_layer::device_address& address )
    {
        return call{ .kind = call_kind::set_local_address, .address = address };
    }

    inline call set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        return call{ .kind = call_kind::set_access_address_and_crc_init, .access_address = access_address, .crc_init = crc_init };
    }

    /**
     * @brief use `phy` in both directions for the connection events scheduled from now on
     */
    inline call set_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t phy )
    {
        return call{ .kind = call_kind::set_phy, .phy = phy };
    }

    /**
     * @brief put `data` into the PDU buffer, as the link layer would, for a later connection event
     */
    inline call queue_pdu( std::span< const std::uint8_t > data )
    {
        return call{ .kind = call_kind::queue_pdu, .transmit = pdu( data ) };
    }

    /**
     * @brief take what the PDU buffer received out of it, as the link layer would, so that it has
     *        room again; device_received() hands the PDUs over
     */
    inline call read_received()
    {
        return call{ .kind = call_kind::read_received };
    }

    /**
     * @brief hand the radio the rig's other PDU buffer from now on, as a link layer with a second
     *        connection would; queue_pdu() and read_received() act on the buffer handed out
     */
    inline call switch_pdu_buffer()
    {
        return call{ .kind = call_kind::switch_pdu_buffer };
    }

    /**
     * @brief set the switches of the connection's encryption, as the link layer does at the steps
     *        of the encryption start; the encryption itself is set up with dut::setup_encryption()
     */
    inline call switch_encryption( bool receive, bool transmit )
    {
        return call{ .kind = call_kind::switch_encryption, .receive_encrypted = receive, .transmit_encrypted = transmit };
    }
    /** @} */

    /**
     * @brief a step on the callback its name says
     * @{
     */
    template < std::same_as< call >... Calls >
    step on_start( Calls... calls )
    {
        return on( callback_kind::start, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_adv_received( Calls... calls )
    {
        return on( callback_kind::adv_received, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_adv_timeout( Calls... calls )
    {
        return on( callback_kind::adv_timeout, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_user_timer( Calls... calls )
    {
        return on( callback_kind::user_timer, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_connection_end_event( Calls... calls )
    {
        return on( callback_kind::connection_end_event, calls... );
    }

    template < std::same_as< call >... Calls >
    step on_connection_timeout( Calls... calls )
    {
        return on( callback_kind::connection_timeout, calls... );
    }
    /** @} */

    /**
     * @brief a tester operation: listen on a channel for a duration
     */
    inline operation listen( std::uint32_t channel, std::chrono::nanoseconds duration )
    {
        return operation{
            .kind    = operation_kind::receive,
            .channel = channel,
            .window  = as_delta_time( duration ) };
    }

    /**
     * @brief a tester operation: listen like listen() and end with the `count`th PDU received
     */
    inline operation receive( std::uint32_t channel, std::uint32_t count, time_out window )
    {
        return operation{
            .kind    = operation_kind::receive,
            .channel = channel,
            .window  = as_delta_time( window.window ),
            .count   = count };
    }

    /**
     * @brief a tester operation: listen like listen(), answer the first advertising PDU
     *        from `target` with `response` one inter frame space after it ended, and end with
     *        the reply
     */
    inline operation answer(
        std::uint32_t channel, const link_layer::device_address& target,
        std::span< const std::uint8_t > response, time_out window )
    {
        return operation{
            .kind     = operation_kind::answer,
            .channel  = channel,
            .window   = as_delta_time( window.window ),
            .target   = target,
            .response = adv_pdu( response ) };
    }

    /**
     * @brief a tester operation: use `phy` from here on; a program starts at 1 Mbit
     */
    inline operation use_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t phy )
    {
        return operation{ .kind = operation_kind::set_phy, .phy = phy };
    }

    /**
     * @brief a tester operation: stop the radio and use `access_address` and `crc_init` from
     *        here on
     */
    inline operation use_access_address( std::uint32_t access_address, std::uint32_t crc_init )
    {
        return operation{
            .kind           = operation_kind::set_access_address_and_crc_init,
            .access_address = access_address,
            .crc_init       = crc_init };
    }

    /**
     * @brief a PDU of a connection event, as the tester sends it: with a valid CRC, or, marked by
     *        with_crc_error(), with an invalid one
     */
    struct event_pdu
    {
        template < typename Bytes >
            requires std::convertible_to< const Bytes&, std::span< const std::uint8_t > >
        event_pdu( const Bytes& bytes )
            : data( std::span< const std::uint8_t >( bytes ) )
        {
        }

        std::span< const std::uint8_t > data;
        bool                            crc_error = false;
    };

    inline event_pdu with_crc_error( std::span< const std::uint8_t > bytes )
    {
        event_pdu result( bytes );
        result.crc_error = true;

        return result;
    }

    /**
     * @brief a tester operation: one connection event on `channel`, with the tester as the central
     *
     * The first of `pdus` has its first bit on air `after` the anchor of the previous connection
     * event, or, before the first, after the first bit of the PDU captured last; each further one
     * `t_ifs` after the device's reply ended. It ends with the reply to the last, or, if a reply
     * does not come, with its window, which is what the event can take.
     *
     * @throw std::invalid_argument for more PDUs than an event holds
     */
    inline tester_step connection_event(
        std::uint32_t channel, std::chrono::nanoseconds after,
        std::initializer_list< event_pdu > pdus,
        std::chrono::nanoseconds t_ifs = std::chrono::microseconds( link_layer::inter_frame_space_us ) )
    {
        // an exchange of the largest PDUs at 1 Mbit takes about 4.3 ms, one each way
        constexpr std::chrono::milliseconds per_exchange( 5 );

        if ( pdus.size() == 0 || pdus.size() > max_event_pdus )
            throw std::invalid_argument( "a connection event sends one to max_event_pdus PDUs" );

        operation result{
            .kind      = operation_kind::connection_event,
            .channel   = channel,
            .window    = as_delta_time( after + per_exchange * pdus.size() ),
            .delay     = as_delta_time( after ),
            .t_ifs     = as_delta_time( t_ifs ) };

        tester_step step( result );
        std::size_t index = 0;

        for ( const event_pdu& next : pdus )
        {
            step.pdus.push_back( pdu( next.data ) );
            step.op.crc_errors |= next.crc_error ? 1u << index : 0u;
            ++index;
        }

        return step;
    }
}
}

#endif
