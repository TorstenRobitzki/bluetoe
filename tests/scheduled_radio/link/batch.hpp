#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_BATCH_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_BATCH_HPP

/**
 * @file batch.hpp
 *
 * How an instrument hands over what it reported, a few items per response, so that the
 * host notices what a full queue dropped.
 */

#include "link/serialize.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the items an instrument hands over in one response
     *
     * `first` is the index of items[ 0 ] among everything the instrument reported since it
     * started counting, `produced` how much that is so far. An item with an index below
     * `produced` that never arrives was dropped by a full queue.
     */
    template < typename T, std::size_t PerBatch >
    struct batch
    {
        std::uint32_t                   first       = 0;
        std::uint32_t                   produced    = 0;
        std::uint8_t                    count       = 0;
        std::array< T, PerBatch >       items;
    };

    template < sink Sink, typename T, std::size_t PerBatch >
    bool serialize( Sink& out, const batch< T, PerBatch >& value )
    {
        return serialize( out, std::tie( value.first, value.produced, value.count, value.items ) );
    }

    template < source Source, typename T, std::size_t PerBatch >
    bool deserialize( Source& in, batch< T, PerBatch >& value )
    {
        auto fields = std::tie( value.first, value.produced, value.count, value.items );

        return deserialize( in, fields ) && value.count <= PerBatch;
    }
}
}

#endif
