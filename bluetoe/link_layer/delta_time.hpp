#ifndef BLUETOE_LINK_LAYER_DELTA_TIME_HPP
#define BLUETOE_LINK_LAYER_DELTA_TIME_HPP

#include <cstdint>
#include <iosfwd>

namespace bluetoe {
namespace link_layer {

    class delta_time
    {
    public:
        explicit delta_time( std::uint32_t usec );
        static delta_time usec( std::uint32_t usec );
        static delta_time msec( std::uint32_t msec );
        static delta_time seconds( int s );
        static delta_time now();

        void print( std::ostream& ) const;

        delta_time& operator+=( const delta_time& rhs );

        bool operator<( const delta_time& rhs ) const;
    private:
        std::uint32_t usec_;
    };


    std::ostream& operator<<( std::ostream&, const delta_time& );
}
}
#endif
