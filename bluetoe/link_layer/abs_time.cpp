#include <bluetoe/abs_time.hpp>
#include <ostream>
#include <iomanip>

namespace bluetoe {
namespace link_layer {


    void abs_time::print( std::ostream& out ) const
    {
        representation_type val = rep_;
        const int us = val % 1000;
        val /= 1000;
        const int ms = val % 1000;
        val /= 1000;
        const int s = val % 60;
        val /= 60;

        out << val << ':'
            << std::setw( 2 ) << std::setfill( '0' ) << s << '.'
            << std::setw( 3 ) << std::setfill( '0' ) << ms << '.'
            << std::setw( 3 ) << std::setfill( '0' ) << us;
    }

    std::ostream& operator<<( std::ostream& out, const abs_time& t )
    {
        t.print( out );
        return out;
    }
}
}
