#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_TESTER_PROGRAM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_TESTER_PROGRAM_HPP

/**
 * @file tester_program.hpp
 *
 * What a program of the tester and the PDUs it captures look like on the wire, shared by the
 * tester and the host. See documentation/scheduled_radio_test_rig.md, decisions 14, 23 and 24.
 *
 * A program is a sequence of operations, each run for the duration it names, or until it
 * received the PDUs it counts, from the moment the previous one ended (decision 14). The
 * PDUs received during it, and the one an answer sends, come back in batches, each naming
 * the index of its first PDU and the number produced so far, the same loss detection the
 * device under test's records use (decision 7).
 */

#include "link/batch.hpp"
#include "link/pdu.hpp"
#include "link/serialize.hpp"

#include <bluetoe/address.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the rate of the tester's clock, its ticks per second
     *
     * The tester timestamps a reception at the resolution of this clock, 62.5 ns, and a
     * reworked board holds it to 50 ppb, so that the tester is the reference the setup is
     * measured against (decision 24). The host converts a tick interval to a time with
     * this rate; the tester never does, and never reports a time in any other unit.
     */
    constexpr std::uint32_t tester_ticks_per_second = 16'000'000;

    /**
     * @brief a time in the tester's domain, a count of its clock's ticks
     *
     * 64 bit, so that it never wraps within a run and the host takes plain differences.
     * Unrelated to the device under test's abs_time, which is microseconds: the two
     * instruments do not share a clock, and now not even a unit (decision 3).
     */
    struct tester_time
    {
        std::uint64_t   ticks = 0;

        friend bool operator==( const tester_time&, const tester_time& ) = default;
    };

    /**
     * @brief what an operation does
     */
    enum class operation_kind : std::uint8_t
    {
        receive,
        answer,
        set_access_address_and_crc_init,
        connection_event,
        set_phy
    };

    /**
     * @brief the most PDUs a connection_event sends
     */
    constexpr std::size_t max_event_pdus = 4;

    /**
     * @brief data channel PDUs the connection events of a program hold, all of them together
     *
     * An operation keeps only the place of its PDUs here, so that the room of a PDU of the
     * largest payload is reserved once per PDU of the program and not once per operation.
     */
    constexpr std::size_t max_program_pdus = 8;

    /**
     * @brief one operation of a tester program
     *
     * `window` is how long it runs, from the end of the previous one. A receive listens on
     * `channel` with the program's PHY and queues every PDU it hears. An answer listens the
     * same way and, the first time it hears an advertising PDU from `target`, answers it with
     * `response` one inter frame space after the PDU ended; the answer is queued too, as a
     * transmitted entry with the time its first bit was on air, and the operation ends with
     * the next PDU received, the reply. `target` and `response` are unused by a receive.
     *
     * A set_access_address_and_crc_init stops the radio, sets `access_address` and `crc_init`
     * for the operations that follow, and ends at once; it has no channel and no window. A
     * set_phy selects `phy` for the operations that follow in the same way; a program starts
     * at 1 Mbit.
     *
     * A connection_event is one connection event on `channel`, with the tester as the central.
     * Its PDUs are the ones added after it with add_event_pdu(); `first_pdu` and `pdu_count`
     * are where they are in the program's PDUs, which the tester fills in and the wire does
     * not carry. It sends the first of them so that its first bit, the event's anchor, is
     * on air `delay` after the anchor of the previous connection_event, or, before the first,
     * after the first bit of the PDU the program captured last. After each PDU it listens for the
     * device's reply and sends the next one `t_ifs` after the reply ended; it ends with the reply
     * to the last. A PDU whose bit is set in `crc_errors`, bit 0 for the first, is sent with an
     * invalid CRC. Every PDU sent and every reply is captured. A reply that does not come ends
     * the event with its `window`, not the program, as a device that does not answer is what
     * some tests observe.
     *
     * A `count` other than zero ends a receive or an answer early, once it received that many
     * PDUs with a valid CRC that passed the tester's filters. An operation whose window
     * ends before its count was reached, or before an answer heard its target, times out
     * and ends the program.
     */
    struct operation
    {
        operation_kind                                  kind            = operation_kind::receive;
        std::uint32_t                                   channel         = 0;
        link_layer::phy_ll_encoding::phy_ll_encoding_t  phy             = link_layer::phy_ll_encoding::le_1m_phy;
        link_layer::delta_time                          window          = {};
        link_layer::device_address                      target          = {};
        adv_pdu                                         response        = {};
        std::uint32_t                                   count           = 0;
        link_layer::delta_time                          delay           = {};
        std::uint32_t                                   access_address  = 0;
        std::uint32_t                                   crc_init        = 0;
        std::uint8_t                                    first_pdu       = 0;
        std::uint8_t                                    pdu_count       = 0;
        link_layer::delta_time                          t_ifs           = {};
        std::uint8_t                                    crc_errors      = 0;

        friend bool operator==( const operation&, const operation& ) = default;
    };

    constexpr std::size_t max_operations = 16;

    /**
     * @brief whether an entry is a PDU the tester heard or one it sent
     */
    enum class pdu_direction : std::uint8_t
    {
        received,
        transmitted
    };

    /**
     * @brief one PDU the tester received or sent, with the time its first bit was on air
     *
     * The two directions share one queue, so that the host reads an answer as an ordered
     * timeline: the advertising PDU heard, then the answer sent, each with its time, from
     * which an inter frame space is a plain difference.
     *
     * `crc_ok` is false for a PDU received with a CRC error, which is still reported,
     * because a test may assert that the device under test transmitted something wrong.
     *
     * `rssi` is the received signal strength, as a positive count of decibels below one
     * milliwatt (the nRF52 RSSISAMPLE), so a smaller number is a stronger signal. It lets a
     * test tell the device, strong over a cable, from the air leaking in weakly, and it is
     * how the threshold for that is found. A transmitted entry carries neither: its
     * `crc_ok` is true and its `rssi` zero.
     */
    struct captured_pdu
    {
        pdu_direction           direction = pdu_direction::received;
        tester_time             when;
        bool                    crc_ok    = false;
        std::uint8_t            rssi      = 0;
        pdu                     data;
    };

    /**
     * @brief captured PDUs a response carries; one, since a PDU of the largest payload is most
     *        of a frame
     */
    constexpr std::size_t captured_per_batch = 1;

    /**
     * @brief the PDUs the tester hands over in one response, counted since the program started
     */
    using captured_batch = batch< captured_pdu, captured_per_batch >;

    template < sink Sink >
    bool serialize( Sink& out, const tester_time& value )
    {
        return serialize( out, value.ticks );
    }

    template < source Source >
    bool deserialize( Source& in, tester_time& value )
    {
        return deserialize( in, value.ticks );
    }

    template < sink Sink >
    bool serialize( Sink& out, const operation& value )
    {
        return serialize( out, std::tie( value.kind, value.channel, value.phy, value.window, value.target, value.response, value.count, value.delay, value.access_address, value.crc_init,
            value.t_ifs, value.crc_errors ) );
    }

    template < source Source >
    bool deserialize( Source& in, operation& value )
    {
        auto fields = std::tie( value.kind, value.channel, value.phy, value.window, value.target, value.response, value.count, value.delay, value.access_address, value.crc_init,
            value.t_ifs, value.crc_errors );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const captured_pdu& value )
    {
        return serialize( out, std::tie( value.direction, value.when, value.crc_ok, value.rssi, value.data ) );
    }

    template < source Source >
    bool deserialize( Source& in, captured_pdu& value )
    {
        auto fields = std::tie( value.direction, value.when, value.crc_ok, value.rssi, value.data );

        return deserialize( in, fields );
    }

}
}

#endif
