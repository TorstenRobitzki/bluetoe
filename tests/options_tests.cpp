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
    };

    struct type11 {
        typedef meta1 meta_type;
    };

    struct type2 {
        typedef meta2 meta_type;
    };

    struct type3 {
        typedef meta3 meta_type;
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




