#ifndef BLUETOE_LINK_LAYER_DELTA_TIME_HPP
#define BLUETOE_LINK_LAYER_DELTA_TIME_HPP

#include <cstdint>
#include <iosfwd>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief positiv time quantum used to express distance in time.
     */
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
        delta_time& operator*=( unsigned rhs );

        bool operator<( const delta_time& rhs ) const;
        bool operator<=( const delta_time& rhs ) const;
        bool operator>( const delta_time& rhs ) const;
        bool operator>=( const delta_time& rhs ) const;
        bool operator==( const delta_time& rhs ) const;
        bool operator!=( const delta_time& rhs ) const;

        std::uint32_t usec() const;

        bool zero() const;

        /**
         * @brief returns the given parts per million.
         *
         * delta_time::usec( 1000000 ).ppm( 44 ).usec() == 44
         */
        delta_time ppm( unsigned part ) const;
    private:
        std::uint32_t usec_;
    };


    std::ostream& operator<<( std::ostream&, const delta_time& );

    delta_time operator+( delta_time lhs, delta_time rhs );
    delta_time operator-( delta_time lhs, delta_time rhs );
    delta_time operator*( delta_time lhs, unsigned rhs );
    delta_time operator*( unsigned lhs, delta_time rhs );

}
}
#endif
