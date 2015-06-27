#include <bluetoe/link_layer/delta_time.hpp>

namespace bluetoe {
namespace link_layer {

    delta_time::delta_time( std::uint32_t usec )
        : usec_( usec )
    {
    }

    delta_time delta_time::usec( std::uint32_t usec )
    {
        return delta_time( usec );
    }

    delta_time delta_time::now()
    {
        return delta_time( 0 );
    }

}
}
