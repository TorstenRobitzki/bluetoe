#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "host/dummy_platform.hpp"
#include "host/proxy.hpp"
#include "host/tester_functions.hpp"
#include "instrument/tester_rig.hpp"
#include "link/frame.hpp"
#include "self_tests/observed_port.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

using namespace bluetoe::test_rig;
using namespace bluetoe::test_rig::self_test;

namespace {

    /*
     * The dummy platform of host/, instrumented: it counts what the tester calls on it.
     * It keeps its instance, since the tester owns the object.
     */
    class counting_platform : public dummy_platform
    {
    public:
        counting_platform()
        {
            instance = this;
        }

        void reset_device_under_test()
        {
            ++resets;
        }

        void run()
        {
            ++runs;
        }

        void wake_up()
        {
            ++wake_ups;
        }

        int resets   = 0;
        int runs     = 0;
        int wake_ups = 0;

        static inline counting_platform* instance = nullptr;
    };

    // a platform that lacks a requirement is rejected by the concept
    struct platform_without_reset
    {
        void run() {}
        void wake_up() {}
    };

    static_assert( tester_platform< dummy_platform > );
    static_assert( tester_platform< counting_platform > );
    static_assert( !tester_platform< platform_without_reset > );

    using rig_t = tester_rig< observed_port, counting_platform >;

    // the host's list is the tester's list, whatever the tester is instantiated with
    static_assert( tester_functions::size == rig_t::functions::size );

    /*
     * The host end of the observed port: a request goes as a frame into the tester's
     * receive buffer, the tester runs one iteration, and the response frame is taken
     * out of its transmit buffer.
     */
    struct rig_transport
    {
        rig_t                                   rig{ "counting platform on the host", "unit test build" };
        counting_platform&                      platform = *counting_platform::instance;
        observed_port_base&                     port     = *observed_port_base::instance;
        frame_sender< erased_buffer >                           sender{ port.receive() };
        frame_receiver< default_max_payload, erased_buffer >     receiver{ port.transmit() };

        std::vector< std::uint8_t > transact( std::span< const std::uint8_t > request )
        {
            if ( !sender.send( request ) )
                throw link_error( "request does not fit" );

            rig.run();

            if ( receiver.receive() != receive_result::frame )
                throw link_error( "no response" );

            return { receiver.payload().begin(), receiver.payload().end() };
        }
    };

    struct fixture
    {
        rig_transport                               transport;
        proxy< rig_t::functions, rig_transport >    remote{ transport };
    };

    std::string_view as_text( const bytes< rig_t::name_size >& name )
    {
        return { reinterpret_cast< const char* >( name.data.data() ), name.size };
    }
}

BOOST_FIXTURE_TEST_CASE( the_port_is_started_on_construction, fixture )
{
    BOOST_CHECK( transport.port.started );
}

BOOST_FIXTURE_TEST_CASE( the_port_wakes_the_platform, fixture )
{
    transport.port.wake();

    BOOST_CHECK_EQUAL( transport.platform.wake_ups, 1 );
}

BOOST_FIXTURE_TEST_CASE( every_iteration_lets_the_platform_run, fixture )
{
    transport.rig.run();
    transport.rig.run();

    BOOST_CHECK_EQUAL( transport.platform.runs, 2 );
    BOOST_CHECK_EQUAL( transport.port.transmissions, 0 );
}

BOOST_FIXTURE_TEST_CASE( the_protocol_version_is_reported, fixture )
{
    BOOST_CHECK_EQUAL( remote.call< &rig_t::protocol_version >(), tester_protocol_version );
}

BOOST_FIXTURE_TEST_CASE( the_names_are_reported_as_given, fixture )
{
    BOOST_CHECK_EQUAL( as_text( remote.call< &rig_t::implementation_name >() ), "counting platform on the host" );
    BOOST_CHECK_EQUAL( as_text( remote.call< &rig_t::build_identifier >() ), "unit test build" );
}

BOOST_FIXTURE_TEST_CASE( set_session_token_answers_with_the_previous_token, fixture )
{
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::set_session_token >( 0x12345678 ) );

    BOOST_CHECK_THROW( remote.call< &rig_t::protocol_version >(), instrument_restarted );

    remote.expect_token( 0x12345678 );
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::protocol_version >() );
}

BOOST_FIXTURE_TEST_CASE( a_reset_request_pulses_the_reset_line_once, fixture )
{
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::reset_device_under_test >() );

    BOOST_CHECK_EQUAL( transport.platform.resets, 1 );
}

BOOST_FIXTURE_TEST_CASE( a_reset_does_not_touch_the_testers_own_token, fixture )
{
    remote.call< &rig_t::set_session_token >( 0x42 );
    remote.expect_token( 0x42 );

    BOOST_CHECK_NO_THROW( remote.call< &rig_t::reset_device_under_test >() );
    BOOST_CHECK_NO_THROW( remote.call< &rig_t::protocol_version >() );
}
