#include "buffer_io.hpp"
#include "hexdump.hpp"

#include <bluetoe/buffer.hpp>
#include <ostream>

namespace bluetoe {
namespace link_layer {

    std::ostream& operator<<( std::ostream& out, const read_buffer& buffer )
    {
        return out << write_buffer{ buffer.buffer, buffer.size };
    }

    std::ostream& operator<<( std::ostream& out, const write_buffer& buffer )
    {
        hex_dump( out, buffer.buffer, buffer.buffer + buffer.size );

        return out;
    }
}
}
