#include <delta_time.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>
#include <algorithm>

namespace bll = bluetoe::link_layer;

BOOST_FIXTURE_TEST_CASE( is_default_constructable, bluetoe::link_layer::delta_time )
{
    BOOST_CHECK_EQUAL( usec(), 0u );
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::now().usec(), 0u );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_micro_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time( 4711 ).usec(), 4711u );
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::usec( 4711 ).usec(), 4711u );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_milli_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::msec( 4711 ).usec(), 4711000u );
}

BOOST_AUTO_TEST_CASE( constructable_from_number_of_seconds )
{
    BOOST_CHECK_EQUAL( bluetoe::link_layer::delta_time::seconds( 11 ).usec(), 11000000u );
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

    BOOST_CHECK_EQUAL( h / z, 5u );
    BOOST_CHECK_EQUAL( h / s, 0u );
    BOOST_CHECK_EQUAL( h / h, 1u );
}

BOOST_AUTO_TEST_SUITE( Abort_Operation_Procedure )

static constexpr long required_accuary = 1000000;

static void check_close( const bll::delta_time& result, std::uint32_t expected )
{
    const std::uint32_t tolerance = std::max< std::uint32_t >( 1, expected / required_accuary );

    BOOST_CHECK_GE(
        static_cast< std::int64_t >( result.usec() ),
        std::int64_t( expected ) - tolerance );

    BOOST_CHECK_LE(
        static_cast< std::int64_t >( result.usec() ),
        std::int64_t( expected ) + tolerance );
}

BOOST_AUTO_TEST_CASE( can_calculate_part_per_million )
{
    check_close( bll::delta_time::usec( 5000 ).ppm( 1000 ), 5 );
}

BOOST_AUTO_TEST_CASE( ppm_with_calculations_that_should_overflow_32bit_math )
{
    check_close(
        bll::delta_time::usec( 100 * 1000 * 1000 ).ppm( 1000 ),
        100 * 1000 );
}

BOOST_AUTO_TEST_CASE( ppm_with_some_extrem_values )
{
    check_close( bll::delta_time::usec( 1000 * 1000 * 1000 ).ppm( 0 ), 0 );
    check_close( bll::delta_time::usec( 1000 * 1000 * 1000 ).ppm( 1 ), 1000 );
}

BOOST_AUTO_TEST_CASE( ppm_rounded_down )
{
    check_close( bll::delta_time::usec( 1000 * 1000 - 2 ).ppm( 44 ), 43 );
    check_close( bll::delta_time::usec( 44 ).ppm( 1000 * 1000 - 2 ), 43 );
}

BOOST_AUTO_TEST_SUITE_END()
