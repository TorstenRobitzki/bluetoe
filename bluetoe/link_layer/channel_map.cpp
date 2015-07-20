#include <bluetoe/link_layer/channel_map.hpp>
#include <cassert>

namespace bluetoe {
namespace link_layer {

    channel_map::channel_map()
    {
    }

    bool channel_map::reset( const std::uint8_t* map, const unsigned hop )
    {
        assert( map );

        if ( hop < 5 || hop > 16 )
            return false;

        map_[ 0 ] = hop % max_number_of_data_channels;

        for ( unsigned channel = hop; channel != 0; channel = map_[ channel ] )
            map_[ channel ] = ( channel + hop ) % max_number_of_data_channels;

        return true;
    }

    unsigned channel_map::next_channel( unsigned last_unmapped_channel ) const
    {
        return map_[ last_unmapped_channel ];
    }


}
}
