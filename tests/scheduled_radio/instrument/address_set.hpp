#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_ADDRESS_SET_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_INSTRUMENT_ADDRESS_SET_HPP

/**
 * @file address_set.hpp
 *
 * The acceptance filter of both instruments: the addresses they accept, a
 * set that is only ever added to, since a reset or the next program is what empties it.
 */

#include <bluetoe/address.hpp>

#include <algorithm>
#include <array>
#include <cstddef>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief a set of at most N device addresses
     */
    template < std::size_t N >
    class address_set
    {
    public:
        /**
         * @brief adds `address`; true if it is in the set afterwards, false if the set is full
         */
        bool add( const link_layer::device_address& address )
        {
            if ( contains( address ) )
                return true;

            if ( count_ == N )
                return false;

            addresses_[ count_ ] = address;
            ++count_;

            return true;
        }

        bool contains( const link_layer::device_address& address ) const
        {
            return std::find( addresses_.begin(), end(), address ) != end();
        }

        bool empty() const
        {
            return count_ == 0;
        }

        std::size_t size() const
        {
            return count_;
        }

    private:
        typename std::array< link_layer::device_address, N >::const_iterator end() const
        {
            return addresses_.begin() + count_;
        }

        std::array< link_layer::device_address, N >     addresses_;
        std::size_t                                     count_ = 0;
    };
}
}

#endif
