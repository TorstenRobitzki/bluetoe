#ifndef BLUETOE_LINK_LAYER_BUFFER_HPP
#define BLUETOE_LINK_LAYER_BUFFER_HPP

#include <cstdlib>
#include <cstdint>

namespace bluetoe {
namespace link_layer {

    struct read_buffer
    {
        std::uint8_t*   buffer;
        std::size_t     size;

        bool empty() const
        {
            return buffer == nullptr && size == 0;
        }
    };

    struct write_buffer
    {
        const std::uint8_t* buffer;
        std::size_t         size;
    };
}
}

#endif
