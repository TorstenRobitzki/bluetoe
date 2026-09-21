#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP

/**
 * @file program.hpp
 *
 * What a program of the device under test and its records look like on the wire, shared by
 * the rig and the host.
 *
 * A program is loaded one request at a time: a step, the callback it waits for, and then each
 * of its calls, as a call with its PDUs is what fits into one request. The steps share one pool
 * of calls, so that a step makes as many as it needs. The records come back in batches, each
 * naming the index of its first record and the number of records produced so far, which is how
 * the host notices that the rig had to drop some.
 */

#include "link/batch.hpp"
#include "link/pdu.hpp"
#include "link/serialize.hpp"

#include <bluetoe/abs_time.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/connection_events.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the callbacks a step can wait for, and the two that are only recorded
     *
     * `start` is the host's start of the program; `radio_ready` cannot be waited for, since
     * the host loads a program only after it was reported.
     */
    enum class callback_kind : std::uint8_t
    {
        start,
        radio_ready,
        adv_received,
        adv_timeout,
        user_timer,
        connection_timeout,
        connection_end_event
    };

    /**
     * @brief the functions of the radio a step can call
     */
    enum class call_kind : std::uint8_t
    {
        start_advertising,
        schedule_advertising_event,
        schedule_timer,
        cancel_radio_event,
        cancel_timer,
        set_local_address,
        set_access_address_and_crc_init,
        schedule_connection_event,
        queue_pdu,
        read_received,
        switch_pdu_buffer,
        set_phy
    };

    /**
     * @brief one call of a step, with the parameters the call kind uses
     *
     * `delay` is added to the time the triggering callback carried, and so is `end_delay`,
     * the end of a connection event's receive window; `transmit` and
     * `response` are the advertising PDU and the scan response of an advertising event, and
     * `transmit` the data channel PDU a queue_pdu puts into the PDU buffer, as the link layer
     * would, which is why it is the only field that can hold one; a read_received takes what
     * the buffer received out of it, as the link layer would, and the rig keeps it for the host; a switch_pdu_buffer hands the radio the rig's other PDU
     * buffer from then on, as a link layer with a second connection would; `address`,
     * `access_address` and `crc_init` are what the two setup calls set, and `phy` what a set_phy
     * sets for both directions of the connection events that follow.
     */
    struct call
    {
        call_kind                   kind            = call_kind::cancel_radio_event;
        std::uint32_t               channel         = 0;
        link_layer::delta_time      delay           = {};
        link_layer::delta_time      end_delay       = {};
        pdu                         transmit        = {};
        adv_pdu                     response        = {};
        link_layer::device_address  address         = {};
        std::uint32_t               access_address  = 0;
        std::uint32_t               crc_init        = 0;
        link_layer::phy_ll_encoding::phy_ll_encoding_t phy = link_layer::phy_ll_encoding::le_1m_phy;

        friend bool operator==( const call&, const call& ) = default;
    };

    /**
     * @brief the steps a program holds, and the calls, in all its steps together
     */
    constexpr std::size_t max_steps = 16;
    constexpr std::size_t max_calls = 32;

    /**
     * @brief what a record is about
     */
    enum class record_kind : std::uint8_t
    {
        callback,
        call
    };

    /**
     * @brief one thing that happened: a callback the radio made, or a call a step made
     *
     * For a callback, `when` is the time it carried, `data` what adv_received() received, an
     * advertising PDU since no other callback carries one, and `events` what
     * connection_end_event() reported. For a call, `when` is the resolved
     * time it passed, `channel` its channel, and `result` what it returned; start_advertising(), the cancels and the setup calls
     * carry no time, and start_advertising() and the setup calls no result.
     */
    struct record
    {
        record_kind                         kind        = record_kind::callback;
        callback_kind                       callback    = callback_kind::start;
        call_kind                           call        = call_kind::cancel_radio_event;
        link_layer::abs_time                when;
        std::uint32_t                       channel     = 0;
        bool                                result      = false;
        adv_pdu                             data;
        link_layer::connection_event_events events;
    };

    constexpr std::size_t records_per_batch = 4;

    /**
     * @brief PDUs a response carries; one, since a PDU of the largest payload is most of a frame
     */
    constexpr std::size_t received_per_batch = 1;

    /**
     * @brief the PDUs received in connection events that the rig hands over in one response
     *
     * They are the ones the link layer's side of the PDU buffer reads, in the order they were
     * received; an empty batch means none is left.
     */
    struct received_batch
    {
        std::uint8_t                                    count       = 0;
        std::array< pdu, received_per_batch >           pdus;
    };

    template < sink Sink >
    bool serialize( Sink& out, const link_layer::connection_event_events& value )
    {
        return serialize( out, std::tie(
            value.unacknowledged_data,
            value.last_received_not_empty,
            value.last_transmitted_not_empty,
            value.last_received_had_more_data,
            value.pending_outgoing_data,
            value.error_occured ) );
    }

    template < source Source >
    bool deserialize( Source& in, link_layer::connection_event_events& value )
    {
        auto fields = std::tie(
            value.unacknowledged_data,
            value.last_received_not_empty,
            value.last_transmitted_not_empty,
            value.last_received_had_more_data,
            value.pending_outgoing_data,
            value.error_occured );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const received_batch& value )
    {
        return serialize( out, std::tie( value.count, value.pdus ) );
    }

    template < source Source >
    bool deserialize( Source& in, received_batch& value )
    {
        auto fields = std::tie( value.count, value.pdus );

        return deserialize( in, fields ) && value.count <= received_per_batch;
    }

    /**
     * @brief the records the rig hands over in one response, counted since the rig started
     */
    using record_batch = batch< record, records_per_batch >;

    template < sink Sink >
    bool serialize( Sink& out, const call& value )
    {
        return serialize( out, std::tie(
            value.kind,
            value.channel,
            value.delay,
            value.end_delay,
            value.transmit,
            value.response,
            value.address,
            value.access_address,
            value.crc_init,
            value.phy ) );
    }

    template < source Source >
    bool deserialize( Source& in, call& value )
    {
        auto fields = std::tie(
            value.kind,
            value.channel,
            value.delay,
            value.end_delay,
            value.transmit,
            value.response,
            value.address,
            value.access_address,
            value.crc_init,
            value.phy );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const record& value )
    {
        return serialize( out, std::tie(
            value.kind,
            value.callback,
            value.call,
            value.when,
            value.channel,
            value.result,
            value.data,
            value.events ) );
    }

    template < source Source >
    bool deserialize( Source& in, record& value )
    {
        auto fields = std::tie(
            value.kind,
            value.callback,
            value.call,
            value.when,
            value.channel,
            value.result,
            value.data,
            value.events );

        return deserialize( in, fields );
    }

}
}

#endif
