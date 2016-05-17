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

BOOST_AUTO_TEST_CASE( multiplication_with_zero )
{
    bll::delta_time z( 0 );
    bll::delta_time o( 1 );
    bll::delta_time h( 100 );

    z *= 0;
    o *= 0;
    h *= 0;

    BOOST_CHECK_EQUAL( z, bll::delta_time::now() );
    BOOST_CHECK_EQUAL( o, bll::delta_time::now() );
    BOOST_CHECK_EQUAL( h, bll::delta_time::now() );
}

BOOST_AUTO_TEST_CASE( multiplication_with_one )
{
    bll::delta_time z( 0 );
    bll::delta_time o( 1 );
    bll::delta_time h( 100 );

    z *= 1;
    o *= 1;
    h *= 1;

    BOOST_CHECK_EQUAL( z, bll::delta_time( 0 ) );
    BOOST_CHECK_EQUAL( o, bll::delta_time( 1 ) );
    BOOST_CHECK_EQUAL( h, bll::delta_time( 100 ) );
}

BOOST_AUTO_TEST_CASE( multiplication_with_greater_than_1 )
{
    bll::delta_time z( 0 );
    bll::delta_time o( 1 );
    bll::delta_time h( 100 );

    z *= 5;
    o *= 5;
    h *= 5;

    BOOST_CHECK_EQUAL( z, bll::delta_time( 0 ) );
    BOOST_CHECK_EQUAL( o, bll::delta_time( 5 ) );
    BOOST_CHECK_EQUAL( h, bll::delta_time( 500 ) );
}

BOOST_AUTO_TEST_CASE( binary_multiplication_operators )
{
    BOOST_CHECK_EQUAL( bll::delta_time( 5 ) * 5, bll::delta_time( 25 ) );
    BOOST_CHECK_EQUAL( 5 * bll::delta_time( 5 ), bll::delta_time( 25 ) );
}

BOOST_AUTO_TEST_CASE( division_by_delta_time )
{
    bll::delta_time h( 100 );
    bll::delta_time z( 20 );
    bll::delta_time s( 1000 );

    BOOST_CHECK_EQUAL( h / z, 5 );
    BOOST_CHECK_EQUAL( h / s, 0 );
    BOOST_CHECK_EQUAL( h / h, 1 );
}