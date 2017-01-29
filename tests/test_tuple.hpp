#include <vector>

namespace test {
    template < std::uint8_t ... Value >
    struct tuple;


    template < std::uint8_t V, std::uint8_t ... Values >
    struct tuple< V, Values ... >
    {
        static std::vector< std::uint8_t > values()
        {
            std::vector< std::uint8_t > result = { V };
            tuple< Values ... >::add_values( result );

            return result;
        }

        static void add_values( std::vector< std::uint8_t >& result )
        {
            result.push_back( V );
            tuple< Values ... >::add_values( result );
        }
    };

    template <>
    struct tuple<>
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