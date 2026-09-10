#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_RIG_SERIALIZE_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_RIG_SERIALIZE_HPP

/**
 * @file serialize.hpp
 *
 * The serialisation of the rig's interface, see documentation/scheduled_radio_test_rig.md,
 * decision 19. Type driven: what goes on the wire is determined by the C++ type alone,
 * so a function is serialisable if every parameter and its result is.
 *
 * Integers are little endian at their fixed width; bool is one byte; an enum is its
 * underlying integer; abs_time and delta_time are their microseconds as 32 bits; arrays,
 * pairs and tuples are their elements in order; a variable length byte sequence is a
 * 16 bit length followed by the bytes.
 *
 * Nothing here throws. serialize() returns false if the sink has no room, deserialize()
 * returns false if the source ends early or a length exceeds its bound. In both cases the
 * output is unspecified and the caller reports a link error.
 */

#include <bluetoe/abs_time.hpp>
#include <bluetoe/delta_time.hpp>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief where serialised bytes go
     *
     * write() returns false, and writes nothing, if `size` bytes do not fit.
     */
    template < typename T >
    concept sink = requires ( T s, const std::uint8_t* data, std::size_t size )
    {
        { s.write( data, size ) } -> std::same_as< bool >;
    };

    /**
     * @brief where serialised bytes come from
     *
     * read() returns false, and reads nothing, if fewer than `size` bytes are left.
     */
    template < typename T >
    concept source = requires ( T s, std::uint8_t* data, std::size_t size )
    {
        { s.read( data, size ) } -> std::same_as< bool >;
    };

    /**
     * @brief a variable length byte sequence with a fixed maximum
     *
     * The receiving side of a byte sequence; the sending side passes a span.
     */
    template < std::size_t MaxSize >
    struct bytes
    {
        std::array< std::uint8_t, MaxSize > data = {};
        std::size_t                         size = 0;

        std::span< const std::uint8_t > span() const
        {
            return { data.data(), size };
        }

        friend bool operator==( const bytes& lhs, const bytes& rhs )
        {
            return std::ranges::equal( lhs.span(), rhs.span() );
        }
    };

    template < typename T >
    concept serializable_integer = std::unsigned_integral< T > && !std::same_as< T, bool >;

    template < sink Sink, serializable_integer T >
    bool serialize( Sink& out, T value )
    {
        std::uint8_t buffer[ sizeof( T ) ];

        for ( std::size_t i = 0; i != sizeof( T ); ++i )
            buffer[ i ] = static_cast< std::uint8_t >( value >> ( 8 * i ) );

        return out.write( buffer, sizeof( T ) );
    }

    template < source Source, serializable_integer T >
    bool deserialize( Source& in, T& value )
    {
        std::uint8_t buffer[ sizeof( T ) ];

        if ( !in.read( buffer, sizeof( T ) ) )
            return false;

        value = 0;
        for ( std::size_t i = 0; i != sizeof( T ); ++i )
            value = static_cast< T >( value | ( static_cast< T >( buffer[ i ] ) << ( 8 * i ) ) );

        return true;
    }

    template < sink Sink >
    bool serialize( Sink& out, bool value )
    {
        return serialize( out, static_cast< std::uint8_t >( value ? 1 : 0 ) );
    }

    template < source Source >
    bool deserialize( Source& in, bool& value )
    {
        std::uint8_t byte;

        if ( !deserialize( in, byte ) )
            return false;

        value = byte != 0;

        return true;
    }

    template < sink Sink, typename E >
        requires std::is_enum_v< E >
    bool serialize( Sink& out, E value )
    {
        return serialize( out, static_cast< std::underlying_type_t< E > >( value ) );
    }

    template < source Source, typename E >
        requires std::is_enum_v< E >
    bool deserialize( Source& in, E& value )
    {
        std::underlying_type_t< E > underlying;

        if ( !deserialize( in, underlying ) )
            return false;

        value = static_cast< E >( underlying );

        return true;
    }

    template < sink Sink >
    bool serialize( Sink& out, link_layer::abs_time value )
    {
        return serialize( out, value.data() );
    }

    template < source Source >
    bool deserialize( Source& in, link_layer::abs_time& value )
    {
        std::uint32_t usec;

        if ( !deserialize( in, usec ) )
            return false;

        value = link_layer::abs_time( usec );

        return true;
    }

    template < sink Sink >
    bool serialize( Sink& out, link_layer::delta_time value )
    {
        return serialize( out, value.usec() );
    }

    template < source Source >
    bool deserialize( Source& in, link_layer::delta_time& value )
    {
        std::uint32_t usec;

        if ( !deserialize( in, usec ) )
            return false;

        value = link_layer::delta_time( usec );

        return true;
    }

    template < sink Sink, typename T, std::size_t N >
    bool serialize( Sink& out, const std::array< T, N >& value )
    {
        for ( const T& element : value )
            if ( !serialize( out, element ) )
                return false;

        return true;
    }

    template < source Source, typename T, std::size_t N >
    bool deserialize( Source& in, std::array< T, N >& value )
    {
        for ( T& element : value )
            if ( !deserialize( in, element ) )
                return false;

        return true;
    }

    template < sink Sink, typename... Ts >
    bool serialize( Sink& out, const std::tuple< Ts... >& value )
    {
        return std::apply( [ & ]( const Ts&... elements ) {
            return ( serialize( out, elements ) && ... );
        }, value );
    }

    template < source Source, typename... Ts >
    bool deserialize( Source& in, std::tuple< Ts... >& value )
    {
        return std::apply( [ & ]( Ts&... elements ) {
            return ( deserialize( in, elements ) && ... );
        }, value );
    }

    template < sink Sink, typename A, typename B >
    bool serialize( Sink& out, const std::pair< A, B >& value )
    {
        return serialize( out, value.first ) && serialize( out, value.second );
    }

    template < source Source, typename A, typename B >
    bool deserialize( Source& in, std::pair< A, B >& value )
    {
        return deserialize( in, value.first ) && deserialize( in, value.second );
    }

    template < sink Sink >
    bool serialize( Sink& out, std::span< const std::uint8_t > value )
    {
        return value.size() <= 0xffff
            && serialize( out, static_cast< std::uint16_t >( value.size() ) )
            && out.write( value.data(), value.size() );
    }

    template < sink Sink, std::size_t MaxSize >
    bool serialize( Sink& out, const bytes< MaxSize >& value )
    {
        return serialize( out, value.span() );
    }

    template < source Source, std::size_t MaxSize >
    bool deserialize( Source& in, bytes< MaxSize >& value )
    {
        std::uint16_t size;

        if ( !deserialize( in, size ) || size > MaxSize )
            return false;

        value.size = size;

        return in.read( value.data.data(), size );
    }

    /**
     * @brief a sink over a fixed buffer
     */
    class buffer_sink
    {
    public:
        buffer_sink( std::uint8_t* buffer, std::size_t capacity )
            : buffer_( buffer ), capacity_( capacity ), size_( 0 ) {}

        template < std::size_t N >
        explicit buffer_sink( std::array< std::uint8_t, N >& buffer )
            : buffer_sink( buffer.data(), N ) {}

        bool write( const std::uint8_t* data, std::size_t size )
        {
            if ( size > capacity_ - size_ )
                return false;

            std::copy( data, data + size, buffer_ + size_ );
            size_ += size;

            return true;
        }

        /**
         * @brief number of bytes written so far
         */
        std::size_t size() const
        {
            return size_;
        }

    private:
        std::uint8_t*   buffer_;
        std::size_t     capacity_;
        std::size_t     size_;
    };

    /**
     * @brief a source over a fixed buffer
     */
    class buffer_source
    {
    public:
        buffer_source( const std::uint8_t* buffer, std::size_t size )
            : buffer_( buffer ), size_( size ), position_( 0 ) {}

        explicit buffer_source( std::span< const std::uint8_t > buffer )
            : buffer_source( buffer.data(), buffer.size() ) {}

        bool read( std::uint8_t* data, std::size_t size )
        {
            if ( size > size_ - position_ )
                return false;

            std::copy( buffer_ + position_, buffer_ + position_ + size, data );
            position_ += size;

            return true;
        }

        /**
         * @brief number of bytes not read yet
         *
         * A complete request leaves nothing; a caller checks this to reject trailing bytes.
         */
        std::size_t remaining() const
        {
            return size_ - position_;
        }

    private:
        const std::uint8_t* buffer_;
        std::size_t         size_;
        std::size_t         position_;
    };
}
}

#endif
