#include "test_tools/dut.hpp"

#include "host/errors.hpp"
#include "instrument/dut_rig.hpp"
#include "test_tools/environment.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <thread>
#include <optional>
#include <string>

namespace bluetoe {
namespace test_rig {

    namespace {

        /*
         * Requests the device may not answer after a reset; each one costs the poll timeout,
         * so this bounds the wait for a device that does not come back. One is enough while
         * the reset returns only once the device runs; the others are for a
         * device that was reset while a frame of the host was still on its way, whose tail it
         * reads as a length and whose poll it then drops with that frame. The
         * first poll that goes unanswered ends that frame, so the next one is answered.
         */
        constexpr int requests_after_reset = 3;

        /*
         * A device that has booted answers a poll within a millisecond; one whose poll was
         * dropped does not answer at all. Nothing is gained by waiting the timeout of a
         * request that may take time.
         */
        constexpr std::chrono::milliseconds poll_timeout{ 250 };

        /*
         * The polls after a reset run with that timeout, and the connection's own returns
         * whichever way restart() leaves.
         */
        class polling
        {
        public:
            explicit polling( serial_transport& transport )
                : transport_( transport )
            {
                transport_.set_timeout( poll_timeout );
            }

            ~polling()
            {
                transport_.set_timeout( request_timeout() );
            }

            polling( const polling& ) = delete;
            polling& operator=( const polling& ) = delete;

        private:
            serial_transport& transport_;
        };

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

        wait_until_ready();
    }

    void dut_connection::wait_until_ready()
    {
        constexpr auto poll_interval = std::chrono::milliseconds( 20 );
        constexpr int  polls         = 150;

        for ( int poll = 0; poll != polls; ++poll )
        {
            if ( remote_.call< &dut::radio_is_ready >() )
                return;

            std::this_thread::sleep_for( poll_interval );
        }

        throw rig_error( "the device's radio did not get ready within " + std::to_string( polls * poll_interval.count() ) + " ms" );
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
        const polling       polls( transport_ );

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
                wait_until_ready();

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
