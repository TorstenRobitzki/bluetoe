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

        for ( unsigned index = 0, channel = hop; index != max_number_of_data_channels; ++index )
        {
            map_[ index ] = channel;
            channel = ( channel + hop ) % max_number_of_data_channels;
        }

        return true;
    }

    unsigned channel_map::data_channel( unsigned index ) const
    {
        assert( index < max_number_of_data_channels );
        return map_[ index ];
    }


}
}
