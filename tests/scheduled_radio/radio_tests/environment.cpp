#include "radio_tests/environment.hpp"

#include "host/errors.hpp"

#include <cstdlib>

namespace bluetoe {
namespace test_rig {

    namespace {

        std::optional< std::string > variable( const char* name )
        {
            const char* value = std::getenv( name );

            if ( !value || !*value )
                return std::nullopt;

            return std::string( value );
        }
    }

    std::string dut_device()
    {
        const auto device = variable( "BLUETOE_DUT" );

        if ( !device )
            throw rig_error( "BLUETOE_DUT is not set; it names the serial device of the device under test" );

        return *device;
    }

    std::optional< std::string > tester_device()
    {
        return variable( "BLUETOE_TESTER" );
    }

    std::chrono::milliseconds request_timeout()
    {
        const auto value = variable( "BLUETOE_DUT_TIMEOUT_MS" );

        return std::chrono::milliseconds( value ? std::stoi( *value ) : 2000 );
    }
}
}
