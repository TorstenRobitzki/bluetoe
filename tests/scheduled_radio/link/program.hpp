#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_PROGRAM_HPP

/**
 * @file program.hpp
 *
 * What a program of the device under test and its records look like on the wire, shared by
 * the rig and the host. See documentation/scheduled_radio_test_rig.md, decisions 7, 9
 * and 14.
 *
 * A program is loaded one step at a time, because a step with its PDUs is what fits into
 * one request. The records come back in batches, each naming the index of its first record
 * and the number of records produced so far, which is how the host notices that the rig had
 * to drop some.
 */

#include "link/pdu.hpp"
#include "link/serialize.hpp"

#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>

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
        user_timer
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
        cancel_timer
    };

    /**
     * @brief one call of a step, with the parameters the call kind uses
     *
     * `delay` is added to the time the triggering callback carried; `transmit` and
     * `response` are the advertising PDU and the scan response of an advertising event.
     */
    struct call
    {
        call_kind               kind    = call_kind::cancel_radio_event;
        std::uint32_t           channel = 0;
        link_layer::delta_time  delay;
        pdu                     transmit;
        pdu                     response;

        friend bool operator==( const call&, const call& ) = default;
    };

    constexpr std::size_t max_calls_per_step = 2;

    /**
     * @brief the callback a step waits for and the calls it makes then
     */
    struct step
    {
        callback_kind                               on          = callback_kind::start;
        std::uint8_t                                call_count  = 0;
        std::array< call, max_calls_per_step >      calls;

        friend bool operator==( const step&, const step& ) = default;
    };

    constexpr std::size_t max_steps = 16;

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
     * For a callback, `when` is the time it carried and `data` what adv_received()
     * received. For a call, `when` is the resolved time it passed, `channel` its channel,
     * and `result` what it returned; start_advertising() and the cancels carry no time.
     */
    struct record
    {
        record_kind             kind        = record_kind::callback;
        callback_kind           callback    = callback_kind::start;
        call_kind               call        = call_kind::cancel_radio_event;
        link_layer::abs_time    when;
        std::uint32_t           channel     = 0;
        bool                    result      = false;
        pdu                     data;
    };

    constexpr std::size_t records_per_batch = 4;

    /**
     * @brief the records the rig hands over in one response
     *
     * `first` is the index of records[ 0 ] among all records since the rig started,
     * `produced` how many exist so far. A record with an index below `produced` that
     * never arrives was dropped by a full queue.
     */
    struct record_batch
    {
        std::uint32_t                                   first       = 0;
        std::uint32_t                                   produced    = 0;
        std::uint8_t                                    count       = 0;
        std::array< record, records_per_batch >         records;
    };

    template < sink Sink >
    bool serialize( Sink& out, const call& value )
    {
        return serialize( out, std::tie( value.kind, value.channel, value.delay, value.transmit, value.response ) );
    }

    template < source Source >
    bool deserialize( Source& in, call& value )
    {
        auto fields = std::tie( value.kind, value.channel, value.delay, value.transmit, value.response );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const step& value )
    {
        return serialize( out, std::tie( value.on, value.call_count, value.calls ) );
    }

    template < source Source >
    bool deserialize( Source& in, step& value )
    {
        auto fields = std::tie( value.on, value.call_count, value.calls );

        return deserialize( in, fields ) && value.call_count <= max_calls_per_step;
    }

    template < sink Sink >
    bool serialize( Sink& out, const record& value )
    {
        return serialize( out, std::tie( value.kind, value.callback, value.call, value.when, value.channel, value.result, value.data ) );
    }

    template < source Source >
    bool deserialize( Source& in, record& value )
    {
        auto fields = std::tie( value.kind, value.callback, value.call, value.when, value.channel, value.result, value.data );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const record_batch& value )
    {
        return serialize( out, std::tie( value.first, value.produced, value.count, value.records ) );
    }

    template < source Source >
    bool deserialize( Source& in, record_batch& value )
    {
        auto fields = std::tie( value.first, value.produced, value.count, value.records );

        return deserialize( in, fields ) && value.count <= records_per_batch;
    }
}
}

#endif
