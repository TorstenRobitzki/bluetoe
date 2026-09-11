#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include "instrument/dispatcher.hpp"

#include <array>
#include <cstdint>
#include <vector>

using namespace bluetoe::test_rig;

namespace {

    /*
     * An object with one function of every shape the protocol has to handle: no
     * arguments, several arguments, no result, a result, arrays and a byte sequence.
     */
    struct calculator
    {
        std::uint16_t add( std::uint16_t a, std::uint16_t b )
        {
            return static_cast< std::uint16_t >( a + b );
        }

        void store( std::uint8_t value )
        {
            stored = value;
        }

        std::uint8_t recall() const
        {
            return stored;
        }

        std::array< std::uint8_t, 2 > swap( const std::array< std::uint8_t, 2 >& pair )
        {
            return { pair[ 1 ], pair[ 0 ] };
        }

        std::uint16_t sum( const bytes< 8 >& values )
        {
            std::uint16_t result = 0;

            for ( std::uint8_t value : values.span() )
                result = static_cast< std::uint16_t >( result + value );

            return result;
        }

        std::uint8_t stored = 0;
    };

    struct counter
    {
        void increment()
        {
            ++count;
        }

        int count = 0;
    };

    using functions = function_list<
        &calculator::add,
        &calculator::store,
        &calculator::recall,
        &calculator::swap,
        &calculator::sum,
        &counter::increment >;

    static_assert( opcode_of< &calculator::add, functions >::value == 0 );
    static_assert( opcode_of< &counter::increment, functions >::value == 5 );
    static_assert( opcode_of< &counter::increment, function_list< &calculator::add > >::value == 1 );

    struct fixture
    {
        calculator                                  calc;
        counter                                     count;
        dispatcher< functions, calculator, counter > dispatch{ calc, count };

        std::vector< std::uint8_t > response;
        std::array< std::uint8_t, 64 > storage = {};

        std::vector< std::uint8_t > request( std::initializer_list< std::uint8_t > bytes, std::uint32_t token = 0x11223344 )
        {
            buffer_sink out( storage );

            BOOST_REQUIRE( dispatch.dispatch( std::span< const std::uint8_t >( bytes.begin(), bytes.size() ), token, out ) );

            return { storage.begin(), storage.begin() + out.size() };
        }
    };

    const std::vector< std::uint8_t > token_and_ok = { 0x44, 0x33, 0x22, 0x11, 0x00 };
}

BOOST_FIXTURE_TEST_CASE( a_function_with_a_result_answers_token_status_result, fixture )
{
    // add( 0x0102, 0x0304 )
    BOOST_TEST( request( { 0x00, 0x02, 0x01, 0x04, 0x03 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06, 0x04 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_function_without_a_result_answers_token_and_status, fixture )
{
    BOOST_TEST( request( { 0x01, 0x2a } ) == token_and_ok, boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( calc.stored, 0x2a );
}

BOOST_FIXTURE_TEST_CASE( a_function_without_arguments_takes_an_empty_request, fixture )
{
    calc.stored = 0x17;

    BOOST_TEST( request( { 0x02 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x17 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( arrays_and_byte_sequences_are_arguments_and_results, fixture )
{
    BOOST_TEST( request( { 0x03, 0xaa, 0xbb } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x00, 0xbb, 0xaa } ),
        boost::test_tools::per_element() );

    // sum( { 1, 2, 3 } )
    BOOST_TEST( request( { 0x04, 0x03, 0x00, 0x01, 0x02, 0x03 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x00, 0x06, 0x00 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( every_object_of_the_list_is_reachable, fixture )
{
    request( { 0x05 } );
    request( { 0x05 } );

    BOOST_CHECK_EQUAL( count.count, 2 );
}

BOOST_FIXTURE_TEST_CASE( the_response_carries_the_token_it_was_given, fixture )
{
    BOOST_TEST( request( { 0x05 }, 0 ) == std::vector< std::uint8_t >( { 0x00, 0x00, 0x00, 0x00, 0x00 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( an_unknown_opcode_is_reported_and_nothing_is_called, fixture )
{
    BOOST_TEST( request( { 0x06, 0x01, 0x02 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x01 } ),
        boost::test_tools::per_element() );
    BOOST_TEST( request( { 0xff } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x01 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( too_few_argument_bytes_are_reported_and_nothing_is_called, fixture )
{
    BOOST_TEST( request( { 0x01 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x02 } ),
        boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( calc.stored, 0 );
}

BOOST_FIXTURE_TEST_CASE( trailing_bytes_are_reported_and_nothing_is_called, fixture )
{
    BOOST_TEST( request( { 0x01, 0x2a, 0x00 } ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x02 } ),
        boost::test_tools::per_element() );
    BOOST_CHECK_EQUAL( calc.stored, 0 );
}

BOOST_FIXTURE_TEST_CASE( an_empty_request_is_malformed, fixture )
{
    BOOST_TEST( request( {} ) == std::vector< std::uint8_t >( { 0x44, 0x33, 0x22, 0x11, 0x02 } ),
        boost::test_tools::per_element() );
}

BOOST_FIXTURE_TEST_CASE( a_response_that_does_not_fit_is_reported, fixture )
{
    std::array< std::uint8_t, 5 > small = {};
    buffer_sink                   out( small );

    const std::uint8_t add[] = { 0x00, 0x02, 0x01, 0x04, 0x03 };

    BOOST_CHECK( !dispatch.dispatch( add, 0, out ) );
}
