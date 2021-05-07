#ifndef BLUETOE_TESTS_TOOLS_ATTRIBUTE_IO_HPP
#define BLUETOE_TESTS_TOOLS_ATTRIBUTE_IO_HPP

#include <bluetoe/attribute.hpp>

#include <iosfwd>

namespace bluetoe {
namespace details {

    std::ostream& operator<<( std::ostream&, const attribute_access_result& );
}
}

#endif
