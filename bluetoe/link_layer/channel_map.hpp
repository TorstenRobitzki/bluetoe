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
         * @brief sets a new list of used channels and a new hop value.
         *
         * The function returns true, if the given parameters are valid.
         * A valid map contains at least 2 channel.
         * A hop increment shall have a value in the range of 5 to 16.
         */
        bool reset( const std::uint8_t* map, const unsigned hop );

        /**
         * @brief sets a new list of used channels and keeps the old hop value.
         *
         * @pre reset( const std::uint8_t* map, const unsigned hop ) must have been called before, to have an old hop value
         */
        bool reset( const std::uint8_t* map );

        /**
         * the BLE channel hop sequence is 37 entries long, after 37 hops, the sequence starts again.
         * This function returns the entries in this sequence. The channel for the first entry is given
         * by calling the function with index = 0, the last entry with index = max_number_of_data_channels -1
         */
        unsigned data_channel( unsigned index ) const;

        /**
         * the number of channels, used as data channel.
         */
        static constexpr unsigned max_number_of_data_channels = 37;
    private:
        unsigned build_used_channel_map( const std::uint8_t* map, std::uint8_t* used ) const;

        std::uint8_t map_[ max_number_of_data_channels ];
        std::uint8_t hop_;
    };
}
}
#endif // include guard
