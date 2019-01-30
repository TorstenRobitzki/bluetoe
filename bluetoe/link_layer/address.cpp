#include <bluetoe/address.hpp>
#include <cassert>
#include <algorithm>
#include <iterator>

namespace bluetoe {
namespace link_layer {

    address::address()
    {
        std::fill( std::begin( value_ ), std::end( value_ ), 0 );
    }

    address::address( const std::initializer_list< std::uint8_t >& initial_values )
    {
        assert( initial_values.size() == address_size_in_bytes );
        std::copy( initial_values.begin(), initial_values.end(), &value_[ 0 ] );
    }

    address::address( const std::uint8_t* initial_values )
    {
        std::copy( initial_values + 0, initial_values + address_size_in_bytes, &value_[ 0 ] );
    }

    random_device_address address::generate_static_random_address( std::uint32_t seed )
    {
        std::uint8_t initial_values[ address_size_in_bytes ] = {
            static_cast<uint8_t>( seed >> 24 ),
            static_cast<uint8_t>( seed >> 16 ),
            static_cast<uint8_t>( seed >> 8 ),
            static_cast<uint8_t>( seed ),
            0x0f,
            0xC0 };

        return random_device_address( initial_values );
    }

    std::uint8_t address::msb() const
    {
        return value_[ address_size_in_bytes - 1 ];
    }

    bool address::operator==( const address& rhs ) const
    {
        return std::equal( std::begin( value_ ), std::end( value_ ), std::begin( rhs.value_ ) );
    }

    bool address::operator!=( const address& rhs ) const
    {
        return !( *this == rhs );
    }

    address::const_iterator address::begin() const
    {
        return std::begin( value_ );
    }

    address::const_iterator address::end() const
    {
        return std::end( value_ );
    }

    device_address::device_address()
        : address()
        , is_random_( true )
    {
    }

    bool device_address::operator==( const device_address& rhs ) const
    {
        return static_cast< const address& >( *this ) == rhs
            && is_random_ == rhs.is_random_;
    }

    bool device_address::operator!=( const device_address& rhs ) const
    {
        return !( *this == rhs );
    }
}
}
