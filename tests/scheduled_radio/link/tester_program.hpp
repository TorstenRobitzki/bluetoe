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
        answer
    };

    /**
     * @brief one operation of a tester program
     *
     * `window` is how long it runs, from the end of the previous one. A receive listens
     * on `channel` with `phy` and queues every PDU it hears. An answer listens the same
     * way and, the first time it hears an advertising PDU from `target`, answers it with
     * `response` one inter frame space after the PDU ended; the answer is queued too, as
     * a transmitted entry with the time its first bit was on air, and the operation ends
     * with the next PDU received, the reply. `target` and `response` are unused by a
     * receive.
     *
     * A `count` other than zero ends either operation early, once it received that many
     * PDUs with a valid CRC that passed the tester's filters; the window still bounds it.
     */
    struct operation
    {
        operation_kind                                  kind     = operation_kind::receive;
        std::uint32_t                                   channel  = 0;
        link_layer::phy_ll_encoding::phy_ll_encoding_t  phy      = link_layer::phy_ll_encoding::le_1m_phy;
        link_layer::delta_time                          window   = {};
        link_layer::device_address                      target   = {};
        pdu                                             response = {};
        std::uint32_t                                   count    = 0;

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

    constexpr std::size_t captured_per_batch = 4;

    /**
     * @brief the PDUs the tester hands over in one response
     *
     * `first` is the index of captured[ 0 ] among all PDUs of the program, `produced` how
     * many it captured so far; a PDU with an index below `produced` that never arrives was
     * dropped by a full queue.
     */
    struct captured_batch
    {
        std::uint32_t                                   first       = 0;
        std::uint32_t                                   produced    = 0;
        std::uint8_t                                    count       = 0;
        std::array< captured_pdu, captured_per_batch >  captured;
    };

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
        return serialize( out, std::tie( value.kind, value.channel, value.phy, value.window, value.target, value.response, value.count ) );
    }

    template < source Source >
    bool deserialize( Source& in, operation& value )
    {
        auto fields = std::tie( value.kind, value.channel, value.phy, value.window, value.target, value.response, value.count );

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

    template < sink Sink >
    bool serialize( Sink& out, const captured_batch& value )
    {
        return serialize( out, std::tie( value.first, value.produced, value.count, value.captured ) );
    }

    template < source Source >
    bool deserialize( Source& in, captured_batch& value )
    {
        auto fields = std::tie( value.first, value.produced, value.count, value.captured );

        return deserialize( in, fields ) && value.count <= captured_per_batch;
    }
}
}

#endif
