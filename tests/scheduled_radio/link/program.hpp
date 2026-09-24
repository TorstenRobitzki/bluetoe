#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP

/**
 * @file program.hpp
 *
 * What a program of the device under test and its records look like on the wire, shared by
 * the rig and the host.
 *
 * A program is loaded one request at a time: a step, the callback it waits for and how often
 * it runs, and then each of its calls, as a call with its PDUs is what fits into one request.
 * The steps share one pool of calls, so that a step makes as many as it needs. The records come
 * back in batches, each naming the index of its first record and the number of records produced
 * so far, which is how the host notices that the rig had to drop some. A step that runs many
 * times is recorded once and counted from then on, in the summary of the program.
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
        start_advertising_event,
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
        set_phy,
        switch_encryption
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
     * sets for both directions of the connection events that follow. `receive_encrypted` and
     * `transmit_encrypted` are the switches a switch_encryption sets in the connection's
     * encryption, which the rig's setup_encryption() set up, as the link layer does at the
     * steps of the encryption start.
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
        bool                        receive_encrypted  = false;
        bool                        transmit_encrypted = false;

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
     * time it passed, `channel` its channel, and `result` what it returned; start_advertising_event(), the cancels and the setup calls
     * carry no time, and start_advertising_event() and the setup calls no result.
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
     * @brief the callback kinds, for a count per kind
     */
    constexpr std::size_t callback_kinds = 7;

    /**
     * @brief the limits of the summary's anchor error bins, in microseconds
     *
     * An error whose magnitude is below the first limit goes into the first bin, one below
     * the second into the second, and so on; one at or beyond the last limit goes into the
     * bin after the last.
     */
    constexpr std::array< std::uint32_t, 4 > anchor_error_limits = { 10, 50, 100, 250 };
    constexpr std::size_t                    anchor_error_bins   = anchor_error_limits.size() + 1;

    /**
     * @brief what a program did, in numbers, since it was started
     *
     * A step that runs many times would flood the records, so the rig counts instead:
     * every callback by its kind, radio_ready() among them, the calls the steps made and
     * those the radio refused. Of every connection event that a step scheduled from the
     * anchor of the one before, from a connection_end_event, and that ended with an anchor
     * of its own, the anchor the radio reported minus the centre of the receive window the
     * step asked for, in microseconds, as the smallest, the largest, and a count per bin of
     * magnitude (anchor_error_limits). A link layer centres the window on the anchor it
     * expects, so this is the drift of the device's sleep clock over the interval and the
     * placement, and how they are distributed over a long run. An event placed from
     * anything else, the advertising before the first, has no anchor to be measured from.
     *
     * The last three are the radio's clock statistics, if it keeps them (the nRF52 radio
     * does with bluetoe::nrf::clock_statistics), and zero otherwise: how often the high
     * frequency crystal was started, how many ticks of the sleep clock it ran in all, and
     * how often the sleep clock was calibrated. They count since the radio started, not
     * since the program did.
     */
    struct program_summary
    {
        std::array< std::uint32_t, callback_kinds >     callbacks           = {};
        std::uint32_t                                   calls               = 0;
        std::uint32_t                                   refused_calls       = 0;
        std::uint32_t                                   anchors             = 0;
        std::int32_t                                    anchor_error_min    = 0;
        std::int32_t                                    anchor_error_max    = 0;
        std::array< std::uint32_t, anchor_error_bins >  anchor_errors       = {};
        std::uint32_t                                   crystal_starts      = 0;
        std::uint32_t                                   crystal_ticks       = 0;
        std::uint32_t                                   calibrations        = 0;

        friend bool operator==( const program_summary&, const program_summary& ) = default;
    };

    /**
     * @brief how often the callbacks of `kind` were made
     */
    inline std::uint32_t count_of( const program_summary& summary, callback_kind kind )
    {
        return summary.callbacks[ static_cast< std::size_t >( kind ) ];
    }

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
    bool serialize( Sink& out, const program_summary& value )
    {
        return serialize( out, std::tie(
            value.callbacks,
            value.calls,
            value.refused_calls,
            value.anchors,
            value.anchor_error_min,
            value.anchor_error_max,
            value.anchor_errors,
            value.crystal_starts,
            value.crystal_ticks,
            value.calibrations ) );
    }

    template < source Source >
    bool deserialize( Source& in, program_summary& value )
    {
        auto fields = std::tie(
            value.callbacks,
            value.calls,
            value.refused_calls,
            value.anchors,
            value.anchor_error_min,
            value.anchor_error_max,
            value.anchor_errors,
            value.crystal_starts,
            value.crystal_ticks,
            value.calibrations );

        return deserialize( in, fields );
    }

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
            value.phy,
            value.receive_encrypted,
            value.transmit_encrypted ) );
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
            value.phy,
            value.receive_encrypted,
            value.transmit_encrypted );

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
