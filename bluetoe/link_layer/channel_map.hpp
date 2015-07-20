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

        /**
         * sets a new list of used channels and a new hop value.
         *
         * The function returns true, if the given parameters are valid.
         * A valid map contains at least 2 channel.
         * A hop increment shall have a value in the range of 5 to 16.
         */
        bool reset( const std::uint8_t* map, const unsigned hop );

        unsigned next_channel( unsigned ) const;

    private:
        static constexpr unsigned max_number_of_data_channels = 37;
        std::uint8_t map_[ max_number_of_data_channels ];
    };
}
}
#endif // include guard
