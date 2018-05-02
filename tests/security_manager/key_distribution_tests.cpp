#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/sm/security_manager.hpp>

#include "test_sm.hpp"

struct void_{};

BOOST_FIXTURE_TEST_CASE( no_distribution_over_unsecure_connection, test::pairing_random_exchanged )
{
    bluetoe::details::link_state< void_ > link( 23, false );
    BOOST_CHECK( !connection_data().outgoing_security_manager_data_available( link ) );
}

#if 0
BOOST_FIXTURE_TEST_CASE( no_distribution_when_in_wrong_state, test::pairing_confirm_exchanged )
{
    bluetoe::details::link_state< void_ > link( 23, true );
    BOOST_CHECK( !connection_data().outgoing_security_manager_data_available( link ) );
}

BOOST_FIXTURE_TEST_CASE( no_distribution_when_no_key_was_being_asked_for, test::pairing_random_exchanged )
{
    bluetoe::details::link_state< void_ > link( 23, true );
    BOOST_CHECK( !connection_data().outgoing_security_manager_data_available( link ) );
}
#endif