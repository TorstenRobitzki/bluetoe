#include <bluetoe/link_layer/channel_map.hpp>

namespace bluetoe {
namespace link_layer {

    channel_map::channel_map()
    {
    }

    void channel_map::reset( const std::uint8_t* map, const unsigned hop )
    {
        hop_ = hop;
    }

    unsigned channel_map::next_channel( unsigned last_unmapped_channel ) const
    {
        return ( last_unmapped_channel + hop_ ) % 37;
    }


}
}
