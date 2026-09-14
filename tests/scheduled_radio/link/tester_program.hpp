#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_TESTER_PROGRAM_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_TESTER_PROGRAM_HPP

/**
 * @file tester_program.hpp
 *
 * What a program of the tester and its received PDUs look like on the wire, shared by the
 * tester and the host. See tests/scheduled_radio/tester.hpp for the contract and
 * documentation/scheduled_radio_test_rig.md, decisions 14 and 24.
 *
 * A program is a sequence of operations, each run for the duration it names from the moment
 * the previous one ended (decision 14). The PDUs received during it come back in batches,
 * each naming the index of its first PDU and the number produced so far, the same loss
 * detection the device under test's records use (decision 7).
 */

#include "link/pdu.hpp"
#include "link/serialize.hpp"

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
     * @brief what an operation does; only listening exists yet
     */
    enum class operation_kind : std::uint8_t
    {
        receive
    };

    /**
     * @brief one operation of a tester program
     *
     * `window` is how long it runs, from the end of the previous one. A receive listens
     * on `channel` with `phy` and queues every PDU it hears.
     */
    struct operation
    {
        operation_kind                                  kind    = operation_kind::receive;
        std::uint32_t                                   channel = 0;
        link_layer::phy_ll_encoding::phy_ll_encoding_t  phy     = link_layer::phy_ll_encoding::le_1m_phy;
        link_layer::delta_time                          window;

        friend bool operator==( const operation&, const operation& ) = default;
    };

    constexpr std::size_t max_operations = 16;

    /**
     * @brief one PDU the tester received, with the time its first bit was on air
     *
     * `crc_ok` is false for a PDU received with a CRC error, which is still reported,
     * because a test may assert that the device under test transmitted something wrong.
     */
    struct received_pdu
    {
        tester_time             when;
        bool                    crc_ok  = false;
        pdu                     data;
    };

    constexpr std::size_t received_per_batch = 4;

    /**
     * @brief the PDUs the tester hands over in one response
     *
     * `first` is the index of received[ 0 ] among all PDUs of the program, `produced` how
     * many it received so far; a PDU with an index below `produced` that never arrives was
     * dropped by a full queue.
     */
    struct received_batch
    {
        std::uint32_t                                   first       = 0;
        std::uint32_t                                   produced    = 0;
        std::uint8_t                                    count       = 0;
        std::array< received_pdu, received_per_batch >  received;
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
        return serialize( out, std::tie( value.kind, value.channel, value.phy, value.window ) );
    }

    template < source Source >
    bool deserialize( Source& in, operation& value )
    {
        auto fields = std::tie( value.kind, value.channel, value.phy, value.window );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const received_pdu& value )
    {
        return serialize( out, std::tie( value.when, value.crc_ok, value.data ) );
    }

    template < source Source >
    bool deserialize( Source& in, received_pdu& value )
    {
        auto fields = std::tie( value.when, value.crc_ok, value.data );

        return deserialize( in, fields );
    }

    template < sink Sink >
    bool serialize( Sink& out, const received_batch& value )
    {
        return serialize( out, std::tie( value.first, value.produced, value.count, value.received ) );
    }

    template < source Source >
    bool deserialize( Source& in, received_batch& value )
    {
        auto fields = std::tie( value.first, value.produced, value.count, value.received );

        return deserialize( in, fields ) && value.count <= received_per_batch;
    }
}
}

#endif
