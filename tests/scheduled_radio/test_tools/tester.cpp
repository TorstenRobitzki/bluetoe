#include "test_tools/tester.hpp"

#include "host/errors.hpp"
#include "instrument/tester_rig.hpp"
#include "test_tools/environment.hpp"

#include <optional>
#include <string>

namespace bluetoe {
namespace test_rig {

    namespace {

        std::string as_text( const bytes< tester::name_size >& name )
        {
            return { reinterpret_cast< const char* >( name.data.data() ), name.size };
        }

        std::string named_tester_device()
        {
            const auto device = tester_device();

            if ( !device )
                throw rig_error( "BLUETOE_TESTER is not set; it names the serial device of the tester" );

            return *device;
        }
    }

    tester_connection::tester_connection()
        : transport_( named_tester_device(), request_timeout() )
        , remote_( transport_ )
    {
        std::uint16_t version = 0;

        try
        {
            version = remote_.call< &tester::protocol_version >();
        }
        catch ( const instrument_restarted& earlier_session )
        {
            remote_.expect_token( earlier_session.received );
            version = remote_.call< &tester::protocol_version >();
        }

        if ( version != tester_protocol_version )
            throw rig_error( "the tester speaks protocol version " + std::to_string( version )
                + ", these tests version " + std::to_string( tester_protocol_version ) );

        const std::uint32_t token = random_session_token();
        remote_.call< &tester::set_session_token >( token );
        remote_.expect_token( token );

        implementation_name_ = as_text( remote_.call< &tester::implementation_name >() );
        build_identifier_    = as_text( remote_.call< &tester::build_identifier >() );

        if ( const auto limit = tester_rssi_limit() )
            remote_.call< &tester::set_rssi_limit >( *limit );
    }

    namespace {
        std::optional< tester_connection > connection;
    }

    void connect_tester_if_named()
    {
        if ( tester_device() )
            connection.emplace();
    }

    void disconnect_tester()
    {
        connection.reset();
    }

    bool tester_connected()
    {
        return connection.has_value();
    }

    tester_connection& the_tester()
    {
        if ( !connection )
            throw rig_error( "no tester is connected; BLUETOE_TESTER names its serial device" );

        return *connection;
    }

    boost::test_tools::assertion_result tester_present::operator()( boost::unit_test::test_unit_id ) const
    {
        boost::test_tools::assertion_result result( tester_connected() );

        if ( !result )
            result.message() << "needs a tester, and BLUETOE_TESTER is not set";

        return result;
    }
}
}
