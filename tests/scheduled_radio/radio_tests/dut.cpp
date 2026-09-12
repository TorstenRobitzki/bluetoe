#include "radio_tests/dut.hpp"

#include "host/errors.hpp"
#include "instrument/dut_rig.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <cstdlib>
#include <optional>
#include <random>
#include <string>

namespace bluetoe {
namespace test_rig {

    namespace {

        std::string required_environment( const char* name )
        {
            const char* value = std::getenv( name );

            if ( !value || !*value )
                throw rig_error( std::string( name ) + " is not set; it names the serial device of the device under test" );

            return value;
        }

        std::chrono::milliseconds timeout_from_environment()
        {
            const char* value = std::getenv( "BLUETOE_DUT_TIMEOUT_MS" );

            return std::chrono::milliseconds( value && *value ? std::atoi( value ) : 2000 );
        }

        std::uint32_t random_session_token()
        {
            std::random_device                              entropy;
            std::uniform_int_distribution< std::uint32_t >  non_zero( 1 );

            return non_zero( entropy );
        }

        std::string as_text( const bytes< dut::name_size >& name )
        {
            return { reinterpret_cast< const char* >( name.data.data() ), name.size };
        }
    }

    dut_connection::dut_connection()
        : transport_( required_environment( "BLUETOE_DUT" ), timeout_from_environment() )
        , remote_( transport_ )
    {
        std::uint16_t version = 0;

        try
        {
            version = remote_.call< &dut::protocol_version >();
        }
        catch ( const instrument_restarted& earlier_session )
        {
            remote_.expect_token( earlier_session.received );
            version = remote_.call< &dut::protocol_version >();
        }

        if ( version != dut_protocol_version )
            throw rig_error( "the device speaks protocol version " + std::to_string( version )
                + ", these tests version " + std::to_string( dut_protocol_version ) );

        const std::uint32_t token = random_session_token();
        remote_.call< &dut::set_session_token >( token );
        remote_.expect_token( token );

        implementation_name_ = as_text( remote_.call< &dut::implementation_name >() );
        build_identifier_    = as_text( remote_.call< &dut::build_identifier >() );
        properties_          = remote_.call< &dut::properties >();
    }

    namespace {

        std::optional< dut_connection > connection;

        /*
         * Opens the connection before the first test and closes it after the last; an
         * error here ends the run with its message instead of failing every test.
         */
        struct open_the_dut
        {
            open_the_dut()
            {
                connection.emplace();

                BOOST_TEST_MESSAGE( "device under test: " << connection->implementation_name()
                    << ", build " << connection->build_identifier() );
            }

            ~open_the_dut()
            {
                connection.reset();
            }
        };
    }

    BOOST_TEST_GLOBAL_FIXTURE( open_the_dut );

    dut_connection& the_dut()
    {
        if ( !connection )
            throw rig_error( "the device under test is not connected" );

        return *connection;
    }

    boost::test_tools::assertion_result dut_supports::operator()( boost::unit_test::test_unit_id ) const
    {
        boost::test_tools::assertion_result result( the_dut().properties().*feature );

        if ( !result )
            result.message() << "not supported by the device under test";

        return result;
    }
}
}
