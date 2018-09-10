#ifndef BLUETOE_SCATTERED_ACCESS_HPP
#define BLUETOE_SCATTERED_ACCESS_HPP

#include <attribute.hpp>
#include <algorithm>

namespace bluetoe {
namespace details {

    template < int Size >
    std::uint8_t* copy( int offset, const std::uint8_t (&source)[ Size ], std::uint8_t* begin, std::uint8_t* end )
    {
        offset = std::max( 0, offset );
        const int source_size = std::max( 0, Size - offset );
        const int copy_size   = std::min( source_size, std::max< int >( 0, end - begin ) );

        std::copy( std::begin( source ) + offset, std::begin( source ) + offset + copy_size, begin );

        return begin + copy_size;
    }

    template < int a_size, int b_size, int c_size >
    void scattered_read_access(
        int offset,
        const std::uint8_t (&a)[ a_size ], const std::uint8_t (&b)[ b_size ], const std::uint8_t (&c)[ c_size ],
        std::uint8_t* out_buffer, int out_buffer_size )
    {
        std::uint8_t* const end = out_buffer + out_buffer_size;
        std::uint8_t*       out = out_buffer;

        out = copy( offset,                   a, out, end );
        out = copy( offset - a_size,          b, out, end );
              copy( offset - a_size - b_size, c, out, end );
    }
}
}
#endif
