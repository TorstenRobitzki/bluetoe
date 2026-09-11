#include "link/status.hpp"

namespace bluetoe {
namespace test_rig {

    const char* describe( status s )
    {
        switch ( s )
        {
            case status::ok:                  return "ok";
            case status::unknown_function:    return "the instrument does not know the function";
            case status::malformed_arguments: return "the instrument could not read the arguments";
        }

        return "unknown status";
    }
}
}
