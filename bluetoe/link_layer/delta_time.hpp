#ifndef BLUETOE_LINK_LAYER_DELTA_TIME_HPP
#define BLUETOE_LINK_LAYER_DELTA_TIME_HPP

#include <cstdint>
#include <iosfwd>

namespace bluetoe {
namespace link_layer {

    class delta_time
    {
    public:
        constexpr delta_time() : usec_( 0 )
        {
        }

        constexpr explicit delta_time( std::uint32_t usec ) : usec_( usec )
        {
        }

        static delta_time usec( std::uint32_t usec );
        static delta_time msec( std::uint32_t msec );
        static delta_time seconds( int s );
        static delta_time now();

        void print( std::ostream& ) const;

        delta_time& operator+=( const delta_time& rhs );
        delta_time& operator-=( const delta_time& rhs );

        bool operator<( const delta_time& rhs ) const;
        bool operator<=( const delta_time& rhs ) const;
        bool operator>( const delta_time& rhs ) const;
        bool operator>=( const delta_time& rhs ) const;
        bool operator==( const delta_time& rhs ) const;
        bool operator!=( const delta_time& rhs ) const;

        std::uint32_t usec() const;

        bool zero() const;
    private:
        std::uint32_t usec_;
    };


    std::ostream& operator<<( std::ostream&, const delta_time& );

    delta_time operator+( delta_time lhs, delta_time rhs );
    delta_time operator-( delta_time lhs, delta_time rhs );
}
}
#endif
