#ifndef BLUETOE_TEST_PAIRING_STATUS_HPP
#define BLUETOE_TEST_PAIRING_STATUS_HPP

#include <bluetoe/pairing_status.hpp>

#include <iosfwd>

namespace bluetoe {

    std::ostream& operator<<( std::ostream& output, device_pairing_status status );
}

#endif // include guard


