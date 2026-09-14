#include "radio_tests/dut.hpp"

#include "host/errors.hpp"
#include "instrument/dut_rig.hpp"
#include "radio_tests/environment.hpp"

#include <boost/test/unit_test.hpp>

#include <optional>
#include <string>

namespace bluetoe {
namespace test_rig {

    namespace {

        /*
         * Requests the device may not answer while it boots after a reset; each one costs
         * the timeout, so this bounds the wait for a device that does not come back.
         */
        constexpr int requests_after_reset = 3;

        std::string as_text( const bytes< dut::name_size >& name )
        {
            return { reinterpret_cast< const char* >( name.data.data() ), name.size };
        }
    }

    dut_connection::dut_connection()
        : transport_( dut_device(), request_timeout() )
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

    /*
     * The poll after the reset is the request that sets the new token: its response carries
     * the token in effect before, which is zero after a reset and the old one if the device
     * never reset. A request the device missed while booting times out and is repeated; a
     * response that got lost the same way shows as the new token coming back, since the
     * repetition then reads what the first request set.
     */
    void dut_connection::restart( tester_connection& tester )
    {
        tester.call< &test_rig::tester::reset_device_under_test >();

        const std::uint32_t token = random_session_token();

        for ( int request = 0; request != requests_after_reset; ++request )
        {
            try
            {
                remote_.call< &dut::set_session_token >( token );

                throw rig_error( "the device answered with the session token from before the reset; it did not reset" );
            }
            catch ( const instrument_restarted& previous )
            {
                if ( previous.received != 0 && previous.received != token )
                    throw rig_error( "the device answered with a foreign session token after the reset" );

                remote_.expect_token( token );

                return;
            }
            catch ( const link_error& )
            {
                // booting, or the request was lost in the reset; ask again
            }
        }

        throw rig_error( "the device did not answer after the reset" );
    }

    namespace {

        std::optional< dut_connection > connection;

        /*
         * Opens the connections before the first test and closes them after the last; an
         * error here ends the run with its message instead of failing every test. With a
         * tester, the device is reset through it first, so that the run starts from a
         * known state.
         */
        struct open_the_dut
        {
            open_the_dut()
            {
                connection.emplace();

                BOOST_TEST_MESSAGE( "device under test: " << connection->implementation_name()
                    << ", build " << connection->build_identifier() );

                connect_tester_if_named();

                if ( tester_connected() )
                {
                    BOOST_TEST_MESSAGE( "tester: " << the_tester().implementation_name()
                        << ", build " << the_tester().build_identifier() );

                    connection->restart( the_tester() );
                }
                else
                {
                    BOOST_TEST_MESSAGE( "no tester; BLUETOE_TESTER is not set" );
                }
            }

            ~open_the_dut()
            {
                disconnect_tester();
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
