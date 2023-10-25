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
     */
    class abs_time
    {
    public:
        using representation_type = std::uint32_t;

        abs_time() = default;

        explicit abs_time( representation_type time_us );

        bool operator<( abs_time& rhs ) const;
        bool operator<=( abs_time& rhs ) const;
        bool operator>( abs_time& rhs ) const;
        bool operator>=( abs_time& rhs ) const;
        bool operator==( abs_time& rhs ) const;
        bool operator!=( abs_time& rhs ) const;

        abs_time& operator-=( delta_time rhs );
        abs_time& operator+=( delta_time rhs );

        representation_type data() const;
    private:
        representation_type rep_;
    };

    /**
     * @brief human readable interpretation of t
     */
    std::ostream& operator<<( std::ostream& out, const abs_time& t );

    abs_time operator+(abs_time, delta_time);
    abs_time operator+(delta_time, abs_time);
    abs_time operator-(abs_time, delta_time);
    delta_time operator-(abs_time, abs_time);
}
}

#endif
