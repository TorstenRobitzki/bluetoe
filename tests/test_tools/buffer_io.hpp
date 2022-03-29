#ifndef BLUETOE_TESTS_LINK_LAYER_BUFFER_HPP
#define BLUETOE_TESTS_LINK_LAYER_BUFFER_HPP

#include <iosfwd>

namespace bluetoe {

    struct read_buffer;
    struct write_buffer;

    std::ostream& operator<<( std::ostream&, const read_buffer& );
    std::ostream& operator<<( std::ostream&, const write_buffer& );
}

#endif
