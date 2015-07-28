#include "connected.hpp"

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE( slave_nacks_data_with_crc_error, connecting )
{
    respond_with_crc_error( 10 );

    find_scheduling(
        []( const test::schedule_data& d )
        {
            return d.channel == 20
                && sn( d ) == 0
                && nesn( d ) == 0;
        },
        "slave_nacks_data_with_crc_error" );
}

BOOST_FIXTURE_TEST_CASE( slave_nacks_data_with_wrong_sn, connected )
{

}

BOOST_FIXTURE_TEST_CASE( slave_acks_data, connected )
{

}

BOOST_FIXTURE_TEST_CASE( slave_uses_the_correct_channel_selection, connected )
{
}