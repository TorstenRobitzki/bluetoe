/**
 * @file reset_tests.cpp
 *
 * The reset line from the tester to the device under test (decision 4), proven the way
 * decision 8 describes: the device carries a session token, the tester pulls the line,
 * and the device answers with a zero token afterwards. These are the first tests that
 * need the tester, and they are skipped without one.
 */

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "test_tools/dut.hpp"
#include "test_tools/tester.hpp"

#include <string>

using namespace bluetoe::test_rig;

namespace {

    const auto if_tester = boost::unit_test::precondition( tester_present{} );

    std::string as_text( const bytes< dut::name_size >& name )
    {
        return { reinterpret_cast< const char* >( name.data.data() ), name.size };
    }

    struct both_instruments : dut_fixture
    {
        tester_connection& tester = the_tester();
    };
}

BOOST_AUTO_TEST_SUITE( reset_line, *if_tester )

BOOST_FIXTURE_TEST_CASE( a_reset_through_the_tester_restarts_the_device, both_instruments )
{
    BOOST_CHECK_NO_THROW( device.restart( tester ) );
}

BOOST_FIXTURE_TEST_CASE( the_device_comes_back_as_the_same_firmware, both_instruments )
{
    device.restart( tester );

    BOOST_CHECK_EQUAL( device.call< &dut::protocol_version >(), dut_protocol_version );
    BOOST_CHECK_EQUAL( as_text( device.call< &dut::build_identifier >() ), device.build_identifier() );
}

BOOST_FIXTURE_TEST_CASE( the_device_can_be_reset_repeatedly, both_instruments )
{
    for ( int round = 0; round != 5; ++round )
        BOOST_CHECK_NO_THROW( device.restart( tester ) );
}

BOOST_FIXTURE_TEST_CASE( the_testers_own_session_survives_a_reset_of_the_device, both_instruments )
{
    device.restart( tester );

    BOOST_CHECK_EQUAL( tester.call< &bluetoe::test_rig::tester::protocol_version >(), tester_protocol_version );
}

BOOST_AUTO_TEST_SUITE_END()
