#include <bluetoe/options.hpp>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <type_traits>

BOOST_AUTO_TEST_CASE( select_type )
{
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::select_type< true, int, bool >::type, int >::value ) );
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::select_type< false, int, bool >::type, bool >::value ) );
}

BOOST_AUTO_TEST_CASE( or_type )
{
    BOOST_CHECK( ( std::is_same< bluetoe::details::or_type< int, char, long >::type, char >::value ) );
    BOOST_CHECK( ( std::is_same< bluetoe::details::or_type< int, int, long >::type, long >::value ) );
    BOOST_CHECK( ( std::is_same< bluetoe::details::or_type< int, int, int >::type, int >::value ) );
}

BOOST_AUTO_TEST_CASE( not_type )
{
    BOOST_CHECK( !( bluetoe::details::not_type< std::true_type >::type::value ) );
    BOOST_CHECK( ( bluetoe::details::not_type< std::false_type >::type::value ) );
}

namespace {

    struct meta1;
    struct meta2;
    struct meta3;

    struct type1 {
        typedef meta1 meta_type;

        static std::string name() {
            return "type1";
        }
    };

    struct type11 {
        typedef meta1 meta_type;

        static std::string name() {
            return "type11";
        }
    };

    struct type2 {
        typedef meta2 meta_type;

        static std::string name() {
            return "type2";
        }
    };

    struct type3 {
        typedef meta3 meta_type;

        static std::string name() {
            return "type3";
        }
    };

    struct type4 {};
}

BOOST_AUTO_TEST_CASE( extract_meta_type )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::extract_meta_type< type1 >::meta_type,
            meta1
        >::value ) );

    BOOST_CHECK( ( bluetoe::details::extract_meta_type< type1 >::has_meta_type::value ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::extract_meta_type< type4 >::meta_type,
            bluetoe::details::no_such_type
        >::value ) );

    BOOST_CHECK( ( !bluetoe::details::extract_meta_type< type4 >::has_meta_type::value ) );
}

BOOST_AUTO_TEST_CASE( find_meta_type_in_empty_list )
{
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::find_by_meta_type< meta1 >::type, bluetoe::details::no_such_type >::value ) );
}

BOOST_AUTO_TEST_CASE( find_meta_type_first_and_only_element )
{
    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type1 >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2 >::type, bluetoe::details::no_such_type >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type4 >::type, bluetoe::details::no_such_type >::value ) );
}

BOOST_AUTO_TEST_CASE( find_meta_type_in_larger_list )
{
    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type1, type2 >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2, type1, type3 >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2, type3, type4, type1  >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2, type3, type4, type1, type11 >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2, type3, type4, type1 >::type, type1 >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type2, type3, type4 >::type, bluetoe::details::no_such_type >::value ) );
}

BOOST_AUTO_TEST_CASE( count_by_meta_type_in_empty_list )
{
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1 >::count ), 0 );
}

BOOST_AUTO_TEST_CASE( count_by_meta_type )
{
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type1 >::count ), 1 );
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type2 >::count ), 0 );
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type1, type2 >::count ), 1 );
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type2, type1 >::count ), 1 );
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type4, type1, type3, type1 >::count ), 2 );
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type4, type3, type2 >::count ), 0 );
}

BOOST_AUTO_TEST_CASE( option_is_not_set_in_an_empty_list )
{
    BOOST_CHECK( !bluetoe::details::has_option< int >::value );
}

BOOST_AUTO_TEST_CASE( option_is_set )
{
    BOOST_CHECK( !( bluetoe::details::has_option< int, char >::value ) );
    BOOST_CHECK( !( bluetoe::details::has_option< int, char, float >::value ) );
    BOOST_CHECK( !( bluetoe::details::has_option< int, char, float, bool >::value ) );

    BOOST_CHECK( ( bluetoe::details::has_option< int, char, int >::value ) );
    BOOST_CHECK( ( bluetoe::details::has_option< int, int, char >::value ) );
    BOOST_CHECK( ( bluetoe::details::has_option< int, char, int, char >::value ) );
}

BOOST_AUTO_TEST_CASE( find_all_by_meta_type_empty_list )
{
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< int >::type, std::tuple<> >::value ) );
}

BOOST_AUTO_TEST_CASE( find_all_by_meta_type )
{
    // no resulting element
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type2, type3 >::type,
        std::tuple<> >::value ) );

    // one resulting element
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type2, type3, type1 >::type,
        std::tuple< type1 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type2, type1, type3 >::type,
        std::tuple< type1 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type1, type3, type2 >::type,
        std::tuple< type1 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type1 >::type,
        std::tuple< type1 > >::value ) );

    // more than one result elements
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type11, type2, type3, type1 >::type,
        std::tuple< type11, type1 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type2, type1, type11, type3 >::type,
        std::tuple< type1, type11 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type1, type3, type11, type2 >::type,
        std::tuple< type1, type11 > >::value ) );
}

namespace {
    typedef std::vector< std::string > string_list;

    struct register_calls
    {
        explicit register_calls( string_list& l ) : list( l )
        {
        }

        template< typename O >
        void each()
        {
            list.push_back( O::name() );
        }

        string_list& list;
    };
}

BOOST_AUTO_TEST_CASE( for_each_empty )
{
    string_list list;

    bluetoe::details::for_<>::each( register_calls( list ) );

    BOOST_CHECK( list.empty() );
}

BOOST_AUTO_TEST_CASE( for_each_one_element )
{
    string_list list;

    bluetoe::details::for_< type1 >::each( register_calls( list ) );

    const string_list expected_result = { "type1" };

    BOOST_CHECK_EQUAL_COLLECTIONS( list.begin(), list.end(), expected_result.begin(), expected_result.end() );
}

BOOST_AUTO_TEST_CASE( for_each_many_elements )
{
    string_list list;

    bluetoe::details::for_< type1, type1, type2, type3 >::each( register_calls( list ) );

    const string_list expected_result = { "type1", "type1", "type2", "type3" };

    BOOST_CHECK_EQUAL_COLLECTIONS( list.begin(), list.end(), expected_result.begin(), expected_result.end() );
}

BOOST_AUTO_TEST_CASE( for_each_feed_by_an_tuple )
{
    string_list list;

    bluetoe::details::for_< std::tuple< type1, type1, type2, type3 > >::each( register_calls( list ) );

    const string_list expected_result = { "type1", "type1", "type2", "type3" };

    BOOST_CHECK_EQUAL_COLLECTIONS( list.begin(), list.end(), expected_result.begin(), expected_result.end() );
}
