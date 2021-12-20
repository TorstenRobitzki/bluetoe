#include "pairing_status_io.hpp"

#include <ostream>

namespace bluetoe {

    std::ostream& operator<<( std::ostream& output, device_pairing_status status )
    {
        switch ( status )
        {
        case device_pairing_status::no_key:
            return output << "no_key";
        case device_pairing_status::unauthenticated_key:
            return output << "unauthenticated_key";
        case device_pairing_status::authenticated_key:
            return output << "authenticated_key";
        case device_pairing_status::authenticated_key_with_secure_connection:
            return output << "authenticated_key_with_secure_connection";
        }

        return output << "invalid device_pairing_status-value (" << static_cast< int >( status ) << ")";
    }
}
