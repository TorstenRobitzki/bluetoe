#include <vector>

namespace {
    template < std::uint8_t ... Value >
    struct test_tuple;


    template < std::uint8_t V, std::uint8_t ... Values >
    struct test_tuple< V, Values ... >
    {
        static std::vector< std::uint8_t > values()
        {
            std::vector< std::uint8_t > result = { V };
            test_tuple< Values ... >::add_values( result );

            return result;
        }

        static void add_values( std::vector< std::uint8_t >& result )
        {
            result.push_back( V );
            test_tuple< Values ... >::add_values( result );
        }
    };

    template <>
    struct test_tuple<>
    {
        static std::vector< std::uint8_t > values()
        {
            return std::vector< std::uint8_t >();
        }

        static void add_values( std::vector< std::uint8_t >& )
        {
        }
    };
}