#ifndef BLUETOE_LINK_LAYER_ABS_TIME_HPP
#define BLUETOE_LINK_LAYER_ABS_TIME_HPP

#include <bluetoe/delta_time.hpp>

#include <cstdint>
#include <iosfwd>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief Type to denote a point in time
     *
     * The denoted point in time is not ment to have any fixed relation to a
     * wall clock time. The resolution of this type is 1µs and should be able
     * to span over the ranges used with Bluetooth LE (4s interval; peripheral latency
     * of 500).
     *
     * The type forms a ring, where quantums of passed time can be added and substructed,
     * with move the represenation on the ring, clock wise or counter clock wise. The
     * type can be used to form relations between two values only if they are resonable
     * close to each othere (a fraction of the circumference of the ring).
     *
     * Two points on that ring can have a `less than` relation if the distance on the ring
     * is less than a certain amount.
     */
    class abs_time
    {
    public:
        using representation_type = std::uint32_t;

        /**
         * @brief the maximum distance between a and b to consider a less than b
         *
         * That's ~16 seconds.
         */
        static constexpr representation_type max_distance = 0x01000000;

        /**
         * @brief default the time representation to 0
         *
         * There is no specific meaning in the representation of 0.
         */
        abs_time() : rep_( 0 ) {}

        /**
         * @brief initilize to a specific representation
         *
         * c'tor is ment to be used from a hardware clock to create
         * abs_times.
         */
        explicit abs_time( representation_type time_us ) : rep_( time_us ) {}

        bool operator<( abs_time rhs ) const
        {
            return rhs.rep_ > rep_
                ? ( rhs.rep_ - rep_ ) < max_distance
                : ( ~rep_ + rhs.rep_ ) < max_distance;
        }

        bool operator<=( abs_time rhs ) const
        {
            return *this < rhs || *this == rhs;
        }

        bool operator>( abs_time rhs ) const
        {
            return !( *this <= rhs );
        }

        bool operator>=( abs_time rhs ) const
        {
            return !( *this < rhs );
        }

        bool operator==( abs_time rhs ) const
        {
            return rep_ == rhs.rep_;
        }

        bool operator!=( abs_time rhs ) const
        {
            return rep_ != rhs.rep_;
        }

        abs_time& operator-=( delta_time rhs )
        {
            rep_ -= rhs.usec();

            return *this;
        }

        abs_time& operator+=( delta_time rhs )
        {
            rep_ += rhs.usec();

            return *this;
        }

        /**
         * @brief returns the internal representation value
         */
        representation_type data() const
        {
            return rep_;
        }

        /**
         * @brief prints this on the given stream in a human readable manner
         */
        void print( std::ostream& ) const;

    private:
        representation_type rep_;
    };

    /**
     * @brief human readable interpretation of t
     */
    std::ostream& operator<<( std::ostream& out, const abs_time& t );

    inline abs_time operator+(abs_time lhs, delta_time rhs)
    {
        lhs += rhs;

        return lhs;
    }

    inline abs_time operator+(delta_time lhs, abs_time rhs)
    {
        return rhs + lhs;
    }

    inline abs_time operator-(abs_time lhs, delta_time rhs)
    {
        lhs -= rhs;

        return lhs;
    }

    inline delta_time operator-(abs_time lhs, abs_time rhs)
    {
        return delta_time( lhs.data() - rhs.data() );
    }
}
}

#endif
