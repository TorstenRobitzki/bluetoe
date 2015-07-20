#ifndef BLUETOE_LINK_LAYER_CHANNEL_MAP_HPP
#define BLUETOE_LINK_LAYER_CHANNEL_MAP_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief map that keeps track of the list of used channels and calculates the next channel based on the last used channel
     */
    class channel_map
    {
    public:
        channel_map();

        void reset( const std::uint8_t* map, const unsigned hop );

        unsigned next_channel( unsigned ) const;

    private:
        unsigned hop_;
    };
}
}
#endif // include guard
