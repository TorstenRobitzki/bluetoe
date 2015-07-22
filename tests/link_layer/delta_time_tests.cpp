#include <bluetoe/link_layer/delta_time.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

namespace bll = bluetoe::link_layer;

BOOST_FIXTURE_TEST_CASE( is_default_constructable, bluetoe::link_layer::delta_time )
{
    BOOST_CHECK_EQUAL( usec(), 0 );
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::now().usec(), 0 );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_micro_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time( 4711 ).usec(), 4711 );
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::usec( 4711 ).usec(), 4711 );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_milli_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::msec( 4711 ).usec(), 4711000 );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::seconds( 11 ).usec(), 11000000 );
}

BOOST_AUTO_TEST_CASE( provides_equal_comparison )
{
    BOOST_CHECK( bll::delta_time::usec( 5000 ) == bll::delta_time::msec( 5 ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) == bll::delta_time::usec( 5001 ) ) );
}

BOOST_AUTO_TEST_CASE( provides_inequal_comparison )
{
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) != bll::delta_time::msec( 5 ) ) );
    BOOST_CHECK( bll::delta_time::usec( 5000 ) != bll::delta_time::usec( 5001 ) );
}

BOOST_AUTO_TEST_CASE( provides_less_than_comparison )
{
    BOOST_CHECK( bll::delta_time::usec( 5000 ) < bll::delta_time::usec( 5001 ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) < bll::delta_time::usec( 5000 ) ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) < bll::delta_time::usec( 4999 ) ) );
}

BOOST_AUTO_TEST_CASE( provides_greater_than_comparison )
{
    BOOST_CHECK( bll::delta_time::usec( 5000 ) > bll::delta_time::usec( 4999 ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) > bll::delta_time::usec( 5000 ) ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) > bll::delta_time::usec( 5001 ) ) );
}

BOOST_AUTO_TEST_CASE( provides_less_than_or_equal_comparison )
{
    BOOST_CHECK( bll::delta_time::usec( 5000 ) <= bll::delta_time::usec( 5001 ) );
    BOOST_CHECK( bll::delta_time::usec( 5000 ) <= bll::delta_time::usec( 5000 ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) <= bll::delta_time::usec( 4999 ) ) );
}

BOOST_AUTO_TEST_CASE( provides_greater_than_or_equal_comparison )
{
    BOOST_CHECK( bll::delta_time::usec( 5000 ) >= bll::delta_time::usec( 4999 ) );
    BOOST_CHECK( bll::delta_time::usec( 5000 ) >= bll::delta_time::usec( 5000 ) );
    BOOST_CHECK( !( bll::delta_time::usec( 5000 ) >= bll::delta_time::usec( 5001 ) ) );
}

BOOST_AUTO_TEST_CASE( provides_a_zero_check )
{
    BOOST_CHECK( bll::delta_time::now().zero() );
    BOOST_CHECK( !bll::delta_time::usec( 5000 ).zero() );
}

BOOST_AUTO_TEST_CASE( can_calculate_part_per_million )
{
    BOOST_CHECK_EQUAL( bll::delta_time::usec( 5000 ).ppm( 1000000).usec(), 5000 );
}