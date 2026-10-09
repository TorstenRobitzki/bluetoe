#ifndef BLUETOE_TEST_TEST_TOOLS_PROCEDURES_IO_HPP
#define BLUETOE_TEST_TEST_TOOLS_PROCEDURES_IO_HPP

#include <bluetoe/procedures.hpp>

#include <iosfwd>

namespace bluetoe {
namespace link_layer {
namespace details {

    std::ostream& operator<<( std::ostream&, procedure_outcome );
    std::ostream& operator<<( std::ostream&, const procedure_result& );

}
}
}
#endif
