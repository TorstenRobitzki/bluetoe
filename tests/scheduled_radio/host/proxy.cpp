#include "host/proxy.hpp"

#include <random>

namespace bluetoe {
namespace test_rig {

    std::uint32_t random_session_token()
    {
        std::random_device                              entropy;
        std::uniform_int_distribution< std::uint32_t >  non_zero( 1 );

        return non_zero( entropy );
    }
}
}
