#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_REPORTED_QUEUE_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_REPORTED_QUEUE_HPP

/**
 * @file reported_queue.hpp
 *
 * The queue an instrument keeps what it reports in until the host collects it, the loss
 * detection of decision 7 (documentation/scheduled_radio_test_rig.md): both instruments
 * report through one of these, and hand it over in batches (link/batch.hpp).
 */

#include "link/batch.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a bounded queue that counts what it could not hold
     *
     * A full queue drops the newest item and counts it anyway, so that the host sees the
     * gap in the count rather than a rewritten history.
     */
    template < typename T, std::size_t Size >
    class reported_queue
    {
    public:
        void push( const T& item )
        {
            ++produced_;

            if ( queued_ == Size )
                return;

            items_[ ( head_ + queued_ ) % Size ] = item;
            ++queued_;
        }

        /**
         * @brief the oldest items, up to a batch of them, and they are forgotten
         *
         * An empty batch is a meaningful answer: it is how a host learns that nothing is
         * left, and what was produced meanwhile.
         */
        template < std::size_t PerBatch >
        batch< T, PerBatch > collect()
        {
            batch< T, PerBatch > result;

            result.first    = collected_;
            result.produced = produced_;

            while ( result.count != PerBatch && queued_ != 0 )
            {
                result.items[ result.count ] = items_[ head_ ];

                head_ = ( head_ + 1 ) % Size;
                --queued_;
                ++collected_;
                ++result.count;
            }

            return result;
        }

        /**
         * @brief forgets everything, and counts from zero again
         */
        void clear()
        {
            head_      = 0;
            queued_    = 0;
            produced_  = 0;
            collected_ = 0;
        }

    private:
        std::array< T, Size >   items_;
        std::size_t             head_       = 0;
        std::size_t             queued_     = 0;
        std::uint32_t           produced_   = 0;
        std::uint32_t           collected_  = 0;
    };
}
}

#endif
