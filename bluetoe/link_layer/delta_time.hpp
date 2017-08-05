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

        /**
         * @brief creates a delta_time from a positive number of µs (Microseconds).
         */
        constexpr explicit delta_time( std::uint32_t usec ) : usec_( usec )
        {
        }

        /**
         * @copydoc delta_time::delta_time( std::uint32_t )
         */
        static delta_time usec( std::uint32_t usec );

        /**
         * @brief creates a delta_time from a positive number of ms (milliseconds).
         */
        static delta_time msec( std::uint32_t msec );

        /**
         * @brief creates a delta_time from a positive number of s (seconds).
         */
        static delta_time seconds( int s );

        /**
         * @brief creates a delta_time denoting the distance from now to now. Aka 0.
         */
        static delta_time now();

        /**
         * @brief prints this on the given stream in a human readable manner
         */
        void print( std::ostream& ) const;

        /**
         * @brief adds rhs to this
         */
        delta_time& operator+=( const delta_time& rhs );

        /**
         * @brief substracts rhs from this.
         * @pre *this >= rhs
         */
        delta_time& operator-=( const delta_time& rhs );

        /**
         * @brief scales this by rhs
         */
        delta_time& operator*=( unsigned rhs );

        /**
         * @brief devides this by rhs
         * @pre this* != delta_time()
         */
        unsigned operator/(const delta_time& rhs );

        /**
         * @brief returns true, if this is smaller than rhs
         */
        bool operator<( const delta_time& rhs ) const;

        /**
         * @brief returns true, if this is smaller than or equal to rhs
         */
        bool operator<=( const delta_time& rhs ) const;

        /**
         * @brief returns true, if this is larger than rhs
         */
        bool operator>( const delta_time& rhs ) const;

        /**
         * @brief returns true, if this is larger than or equal to rhs
         */
        bool operator>=( const delta_time& rhs ) const;

        /**
         * @brief returns true, if this is equal to rhs
         */
        bool operator==( const delta_time& rhs ) const;

        /**
         * @brief returns false, if this is equal to rhs
         */
        bool operator!=( const delta_time& rhs ) const;

        /**
         * @brief returns represented time distance as a number of µs.
         *
         * delta_time( x ).usec() == x
         */
        std::uint32_t usec() const;

        /**
         * @brief returns true, if the represented time distance is zero.
         *
         * delta_time( 0 ).zero() == true
         */
        bool zero() const;

        /**
         * @brief returns the given parts per million.
         *
         * delta_time::usec( 1000000 ).ppm( 44 ).usec() == 44
         * @param part the parts of a million. The parameter have to in the range 0-1000
         *        to not cause overflows.
         */
        delta_time ppm( unsigned part ) const;
    private:
        std::uint32_t usec_;
    };

    /**
     * @brief prints the given delta_time on the given stream in a human readable manner
     * @relates delta_time
     */
    std::ostream& operator<<( std::ostream&, const delta_time& );

    /**
     * @brief returns the sum of both delta_times
     * @relates delta_time
     */
    delta_time operator+( delta_time lhs, delta_time rhs );

    /**
     * @brief returns the difference of both delta_times
     * @relates delta_time
     */
    delta_time operator-( delta_time lhs, delta_time rhs );

    /**
     * @brief scales the given delta_time by the given factor
     * @relates delta_time
     */
    delta_time operator*( delta_time lhs, unsigned rhs );

    /**
     * @copydoc operator*( delta_time, unsigned )
     */
    delta_time operator*( unsigned lhs, delta_time rhs );

}
}
#endif
