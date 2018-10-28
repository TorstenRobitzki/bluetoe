#ifndef BLUE_TESTS_TEST_UUID_HPP
#define BLUE_TESTS_TEST_UUID_HPP

#include <cstdint>
#include <cstddef>
#include <iosfwd>
#include <vector>

namespace test {

    class dynamic_uuid
    {
    public:
        dynamic_uuid();

        dynamic_uuid( const std::uint8_t*, std::size_t );

        template < class UUID >
        explicit dynamic_uuid( const UUID* )
            : uuid_( std::begin( UUID::bytes ), std::end( UUID::bytes ) )
        {
        }

        bool operator==( const dynamic_uuid& rhs ) const;
        bool operator<( const dynamic_uuid& rhs ) const;
        void print( std::ostream& ) const;

    private:
        std::vector< std::uint8_t > uuid_;
    };

    std::ostream& operator<<( std::ostream& out, const dynamic_uuid& );


}

#endif
