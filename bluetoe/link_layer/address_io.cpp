#include <bluetoe/link_layer/address.hpp>
#include <ostream>
#include <iomanip>
#include <boost/io/ios_state.hpp>

namespace bluetoe {
namespace link_layer {

    std::ostream& address::print( std::ostream& out ) const
    {
        boost::io::ios_all_saver save_flags( out );

        auto begin = std::begin( value_ );
        auto end   = std::end( value_ );

        out << std::hex << std::setfill( '0' ) << std::setw( 2 ) << static_cast< unsigned >( *begin );
        ++begin;

        for ( ; begin != end; ++begin )
        {
            out << ':' << std::setw( 2 ) << static_cast< unsigned >( *begin );
        }

        return out;
    }

    std::ostream& operator<<( std::ostream& out, const address& a )
    {
        return a.print( out );
    }

    std::ostream& operator<<( std::ostream& out, const device_address& a )
    {
        return out << static_cast< const address& >( a ) << ( a.is_public() ? " (public)" : " (random)");
    }

}
}
