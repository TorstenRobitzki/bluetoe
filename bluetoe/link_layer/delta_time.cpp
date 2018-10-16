#include <bluetoe/delta_time.hpp>
#include <ostream>
#include <cassert>

namespace bluetoe {
namespace link_layer {

    delta_time delta_time::usec( std::uint32_t usec )
    {
        return delta_time( usec );
    }

    delta_time delta_time::msec( std::uint32_t msec )
    {
        std::uint32_t usec = msec * 1000;
        assert( usec >= msec );

        return delta_time( usec );
    }

    delta_time delta_time::seconds( int s )
    {
        return delta_time( s * 1000 * 1000 );
    }

    delta_time delta_time::now()
    {
        return delta_time( 0 );
    }

    void delta_time::print( std::ostream& output ) const
    {
        if ( usec_ == 0 || usec_ % 1000 == 0 )
            output << ( usec_ / 1000 ) << "ms";
        else
            output << usec_ << "µs";
    }

    delta_time& delta_time::operator+=( const delta_time& rhs )
    {
        auto const sum = usec_ + rhs.usec_;
        assert( sum >= usec_ && sum >= rhs.usec_ );

        usec_ = sum;

        return *this;
    }

    delta_time& delta_time::operator-=( const delta_time& rhs )
    {
        auto const diff = usec_ - rhs.usec_;
        assert( diff <= usec_ );

        usec_ = diff;

        return *this;
    }

    delta_time& delta_time::operator*=( unsigned rhs )
    {
        if ( rhs == 0 || usec_ == 0 )
        {
            usec_ = 0;
        }
        else if ( rhs > 1 )
        {
            if ( usec_ == 1 )
            {
                usec_ = rhs;
            }
            else
            {
                auto const prod = usec_ * rhs;
                assert( prod > usec_ );
                assert( prod > rhs );

                usec_ = prod;
            }
        }

        return *this;
    }

    unsigned delta_time::operator/(const delta_time& rhs )
    {
        assert( rhs.usec_ );
        return usec_ / rhs.usec_;
    }

    bool delta_time::operator<( const delta_time& rhs ) const
    {
        return usec_ < rhs.usec_;
    }

    bool delta_time::operator<=( const delta_time& rhs ) const
    {
        return usec_ <= rhs.usec_;
    }

    bool delta_time::operator>( const delta_time& rhs ) const
    {
        return usec_ > rhs.usec_;
    }

    bool delta_time::operator>=( const delta_time& rhs ) const
    {
        return usec_ >= rhs.usec_;
    }

    bool delta_time::operator==( const delta_time& rhs ) const
    {
        return usec_ == rhs.usec_;
    }

    bool delta_time::operator!=( const delta_time& rhs ) const
    {
        return usec_ != rhs.usec_;
    }

    std::uint32_t delta_time::usec() const
    {
        return usec_;
    }

    bool delta_time::zero() const
    {
        return usec_ == 0;
    }

    delta_time delta_time::ppm( unsigned part ) const
    {
        /*
         * Assumed that usec_ is at most 10^8 and the multiplication is done in a 64 bit
         * integer, the result will be in the order 10^8 * 1000 = 10^11 ~ 2^37,
         * the result can be scalled by 2^27 before shifting it to the right by 2^47
         */
        return delta_time( ( std::uint64_t( usec_ ) * part * 140737488 ) >> 47 );
    }

    std::ostream& operator<<( std::ostream& out, const delta_time& t )
    {
        t.print( out );

        return out;
    }

    delta_time operator+( delta_time lhs, delta_time rhs )
    {
        lhs += rhs;
        return lhs;
    }

    delta_time operator-( delta_time lhs, delta_time rhs )
    {
        lhs-= rhs;
        return lhs;
    }

    delta_time operator*( delta_time lhs, unsigned rhs )
    {
        lhs *= rhs;
        return lhs;
    }

    delta_time operator*( unsigned lhs, delta_time rhs )
    {
        rhs *= lhs;
        return rhs;
    }

}
}
