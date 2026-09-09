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

        /**
         * @brief is this point in time behind reference by less than max_distance?
         *
         * The question a caller asks with this is whether a time it wants to schedule
         * something for has already gone by, or has come so close that there is no longer
         * enough time to act on it. The caller adds whatever margin it needs to act to the
         * reference it passes, so a radio would ask:
         *
         * @code
         * if ( when.is_in_near_past( time_now() + hardware_setup_time ) )
         *     return false;
         * @endcode
         *
         * abs_time values lie on a ring, so this is deliberately not an ordering. For a
         * reference further away than max_distance in either direction the answer is false,
         * which is the right answer to the question asked: such a time is neither just gone
         * nor imminent. Passing values that are far apart is therefore ordinary use and not
         * an error.
         *
         * That the answer is trustworthy rests on how far into the past a time can plausibly
         * be. A time computed by the link layer lands in the past only by the interrupts and
         * the arithmetic that happened since it was computed, which is milliseconds, and
         * never by anything approaching max_distance. A time that really is ahead has to lie
         * within max_distance to be recognised as such, which is what sizes the constant.
         *
         * A time is not in the near past of itself.
         */
        bool is_in_near_past( abs_time reference ) const
        {
            return reference.rep_ > rep_
                ? ( reference.rep_ - rep_ ) < max_distance
                : ( ~rep_ + reference.rep_ ) < max_distance;
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

    /**
     * @brief the time from rhs to lhs
     *
     * @pre lhs is not behind rhs. delta_time counts microseconds without a sign, so a
     *      difference taken the wrong way round is not negative but close to the 71 minutes
     *      the representation spans, which no caller is likely to notice.
     *
     * A link layer that follows several connections will want to ask which of them is due
     * next, by taking each distance from the current time and picking the smallest. Whether
     * that is better served by a signed delta_time is left to that work.
     */
    inline delta_time operator-(abs_time lhs, abs_time rhs)
    {
        return delta_time( lhs.data() - rhs.data() );
    }
}
}

#endif
