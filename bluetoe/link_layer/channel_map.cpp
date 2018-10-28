#include <bluetoe/channel_map.hpp>
#include <cassert>

namespace bluetoe {
namespace link_layer {

    channel_map::channel_map()
        : hop_( 0 )
    {
    }

    static bool in_map( const std::uint8_t* map, unsigned index )
    {
        return map[ index / 8 ] & ( 1 << ( index % 8 ) );
    }

    unsigned channel_map::build_used_channel_map( const std::uint8_t* map, std::uint8_t* used ) const
    {
        unsigned count = 0;

        for ( unsigned channel = 0; channel != max_number_of_data_channels; ++channel )
        {
            if ( in_map( map, channel ) )
            {
                used[ count ] = channel;
                ++count;
            }
        }

        return count;
    }

    bool channel_map::reset( const std::uint8_t* map, const unsigned hop )
    {
        assert( map );

        if ( hop < 5 || hop > 16 )
            return false;

        hop_ = hop;

        std::uint8_t   used_channels[ max_number_of_data_channels ];
        const unsigned used_channels_count = build_used_channel_map( map, used_channels );

        if ( used_channels_count < 2 )
            return false;

        for ( unsigned index = 0, channel = hop; index != max_number_of_data_channels; ++index )
        {
            if ( in_map( map, channel ) )
            {
                map_[ index ] = channel;
            }
            else
            {
                map_[ index ] = used_channels[ channel % used_channels_count ];
            }

            channel = ( channel + hop ) % max_number_of_data_channels;
        }

        return true;
    }

    bool channel_map::reset( const std::uint8_t* map )
    {
        return reset( map, hop_ );
    }

    unsigned channel_map::data_channel( unsigned index ) const
    {
        assert( index < max_number_of_data_channels );
        return map_[ index ];
    }


}
}
