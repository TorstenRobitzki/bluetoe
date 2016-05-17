#include "test_uuid.hpp"
#include "hexdump.hpp"

#include <cassert>

namespace test {

    dynamic_uuid::dynamic_uuid()
    {
    }

    dynamic_uuid::dynamic_uuid( const std::uint8_t* p, std::size_t s )
        : uuid_( p, p + s )
    {
        assert( s == 2 || s == 16 );
    }

    bool dynamic_uuid::operator==( const dynamic_uuid& rhs ) const
    {
        return uuid_.size() == rhs.uuid_.size()
            && std::equal( uuid_.begin(), uuid_.end(), rhs.uuid_.begin() );
    }

    void dynamic_uuid::print( std::ostream& o ) const
    {
        for ( auto c = uuid_.begin(); c != uuid_.end(); ++c )
        {
            print_hex( o, *c );

            if ( c + 1 != uuid_.end() )
                o << ':';
        }
    }

    std::ostream& operator<<( std::ostream& out, const dynamic_uuid& uuid )
    {
        out << "[";
        uuid.print( out );
        out << "]";

        return out;
    }

}
