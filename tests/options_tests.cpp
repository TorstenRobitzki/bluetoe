#include <meta_tools.hpp>
#include <string>

#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <type_traits>

template < typename >
struct template_a {};

template < typename >
struct template_b {};

BOOST_AUTO_TEST_CASE( select_type )
{
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::select_type< true, int, bool >::type, int >::value ) );
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::select_type< false, int, bool >::type, bool >::value ) );
}

BOOST_AUTO_TEST_CASE( select_template_t1 )
{
    BOOST_CHECK( (
        std::is_same<
            bluetoe::details::select_template_t1< true, template_a, template_b >::template type< int >,
            template_a< int > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            bluetoe::details::select_template_t1< false, template_a, template_b >::template type< int >,
            template_b< int > >::value
    ) );

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

    struct meta1 {};
    struct meta2 {};
    struct meta3 {};
    struct meta4 {};

    struct meta1a : meta1 {};

    struct meta12 : meta1, meta2 {};

    struct type1 {
        typedef meta1 meta_type;

        static std::string name() {
            return "type1";
        }
    };

    struct type1a {
        typedef meta1a meta_type;

        static std::string name() {
            return "type1a";
        }
    };

    struct type12 {
        typedef meta12 meta_type;

        static std::string name() {
            return "type12";
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
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type1a >::type, type1a >::value ) );

    BOOST_CHECK( (
        std::is_same< typename bluetoe::details::find_by_meta_type< meta1, type12 >::type, type12 >::value ) );

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

BOOST_AUTO_TEST_CASE( find_by_not_meta_type_in_empty_list )
{
    BOOST_CHECK( ( std::is_same< typename bluetoe::details::find_by_not_meta_type< meta1 >::type, bluetoe::details::no_such_type >::value ) );
}

BOOST_AUTO_TEST_CASE( find_by_not_meta_type_in_one_element )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type1 >::type,
            bluetoe::details::no_such_type
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type2 >::type,
            type2
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type12 >::type,
            bluetoe::details::no_such_type
        >::value
    ) );
}

BOOST_AUTO_TEST_CASE( find_by_not_meta_type_in_list )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type1, type2, type12 >::type,
            type2
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type1, type12 >::type,
            bluetoe::details::no_such_type
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_by_not_meta_type< meta1, type2, type3 >::type,
            type2
        >::value
    ) );
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

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_by_meta_type< meta1, type1, type3, type2, type12 >::count ), 2 );
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

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type1a, type3, type2 >::type,
        std::tuple< type1a > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type12 >::type,
        std::tuple< type12 > >::value ) );

    // more than one result elements
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type11, type2, type3, type12, type1 >::type,
        std::tuple< type11, type12, type1 > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::find_all_by_meta_type< meta1, type2, type1a, type11, type3 >::type,
        std::tuple< type1a, type11 > >::value ) );

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

    struct count_calls
    {
        explicit count_calls( int& c ) : counts( c )
        {
        }

        template< typename O >
        void each()
        {
            ++counts;
        }

        int& counts;
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

BOOST_AUTO_TEST_CASE( for_each_over_tuple_of_empty_tuple )
{
    int count = 0;

    bluetoe::details::for_<
        std::tuple<
            std::tuple<>
        > >::each( count_calls( count ) );

    BOOST_CHECK_EQUAL( count, 1 );
}

BOOST_AUTO_TEST_CASE( for_each_over_tuple_of_tuples )
{
    int count = 0;

    bluetoe::details::for_<
        std::tuple<
            std::tuple< int, char >,
            std::tuple< bool, double >
        > >::each( count_calls( count ) );

    BOOST_CHECK_EQUAL( count, 2 );
}

BOOST_AUTO_TEST_CASE( group_by_meta_type_empty )
{
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple<> >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple<>, meta1 >::type,
        std::tuple< std::tuple< meta1 > > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple<>, meta1, meta2 >::type,
        std::tuple< std::tuple< meta1 >, std::tuple< meta2 > > >::value ) );
}

BOOST_AUTO_TEST_CASE( group_by_meta_type )
{
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple< type1, type2, type3 > >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple< type1, type2, type3, type12 >, meta1 >::type,
        std::tuple< std::tuple< meta1, type1, type12 > > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple< type1, type2, type3, type12 >, meta1, meta2 >::type,
        std::tuple< std::tuple< meta1, type1, type12 >, std::tuple< meta2, type2, type12 > > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types< std::tuple< type1, type2, type3, type12 >, meta1, meta2, meta4 >::type,
        std::tuple< std::tuple< meta1, type1, type12 >, std::tuple< meta2, type2, type12 >, std::tuple< meta4 > > >::value ) );
}

BOOST_AUTO_TEST_CASE( group_by_meta_types_without_empty_groups )
{

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::group_by_meta_types_without_empty_groups< std::tuple< type1, type2, type3 > >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::group_by_meta_types_without_empty_groups<
                std::tuple< type1, type2, type3, type12 >,
                meta1, meta2, meta4
            >::type,
            std::tuple< std::tuple< meta1, type1, type12 >, std::tuple< meta2, type2, type12 > >
        >::value ) );
}

BOOST_AUTO_TEST_CASE( remove_if_equal )
{
    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple<>, int >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< int >, int >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< float >, int >::type,
        std::tuple< float > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< float, double, int, int, char >, int >::type,
        std::tuple< float, double, char > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< int, float, double, char, int >, int >::type,
        std::tuple< float, double, char > >::value ) );
}

namespace {
    template < class T >
    struct templ {};

    template < class T >
    struct other_templ {};
}

BOOST_AUTO_TEST_CASE( remove_if_with_one_wildcard )
{
    typedef templ< bluetoe::details::wildcard > with_wildcard;

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple<>, with_wildcard >::type,
        std::tuple<> >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< int >, with_wildcard >::type,
        std::tuple< int > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< int, other_templ< int > >, with_wildcard >::type,
        std::tuple< int, other_templ< int > > >::value ) );

    BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< templ< char >, int, templ< int > >, with_wildcard >::type,
        std::tuple< int > >::value ) );

   BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< std::tuple< int > >, std::tuple< bluetoe::details::wildcard > >::type,
        std::tuple<> >::value ) );

   BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal< std::tuple< int, std::tuple< int, int >, std::tuple< int > >, std::tuple< bluetoe::details::wildcard > >::type,
        std::tuple< int, std::tuple< int, int > > >::value ) );

   BOOST_CHECK( ( std::is_same<
        typename bluetoe::details::remove_if_equal<
            std::tuple< std::tuple< meta1, type1, type12 >, std::tuple< meta2, type2, type12 >, std::tuple< meta4 > >,
            std::tuple< bluetoe::details::wildcard >
        >::type,
        std::tuple< std::tuple< meta1, type1, type12 >, std::tuple< meta2, type2, type12 > > >::value ) );

}

namespace {
    template < typename T >
    struct is_int : std::false_type {};

    template <>
    struct is_int< int > : std::true_type {};
}

BOOST_AUTO_TEST_CASE( count_if )
{
    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple<>, is_int >::value ), 0 );

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple< int >, is_int >::value ), 1 );

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple< char >, is_int >::value ), 0 );

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple< int, char, int >, is_int >::value ), 2 );

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple< char, bool, int, float >, is_int >::value ), 1 );

    BOOST_CHECK_EQUAL( ( bluetoe::details::count_if< std::tuple< char, int >, is_int >::value ), 1 );
}

namespace {
    template < class T >
    struct by_value : T {};
}

BOOST_AUTO_TEST_CASE( sum_by )
{
    BOOST_CHECK_EQUAL( ( bluetoe::details::sum_by< std::tuple<>, by_value >::value ), 0 );

    BOOST_CHECK_EQUAL( (
        bluetoe::details::sum_by<
            std::tuple<
                std::integral_constant< int, 4 >
            >, by_value >::value ),
        4 );

    BOOST_CHECK_EQUAL( (
        bluetoe::details::sum_by<
            std::tuple<
                std::integral_constant< int, 4 >,
                std::integral_constant< int, 1 >
            >, by_value >::value ),
        5 );
}

BOOST_AUTO_TEST_CASE( find_if )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple< float, char, int >, is_int >::type,
            int >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple< int, float, char >, is_int >::type,
            int >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple< float, int, char >, is_int >::type,
            int >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple< int >, is_int >::type,
            int >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple<>, is_int >::type,
            bluetoe::details::no_such_type >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::find_if< std::tuple< float, bool >, is_int >::type,
            bluetoe::details::no_such_type >::value
    ) );
}

BOOST_AUTO_TEST_CASE( last_from_pack )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_from_pack< int >::type,
            int >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_from_pack< float, int >::type,
            int >::value
    ) );

}

namespace {
    std::string ctor_order;

    template < char C >
    struct tag
    {
        tag()
        {
            ctor_order += C;
        }
    };

    template < typename List >
    static void test_derive_from( const std::string& expected )
    {
        ctor_order = "";
        bluetoe::details::derive_from< List > instance;
        static_cast< void >( instance );

        BOOST_CHECK_EQUAL( ctor_order, expected );
    }
}

BOOST_AUTO_TEST_CASE( derive_from )
{
    test_derive_from< std::tuple<> >( "" );
    test_derive_from< std::tuple< tag<'A'> > >( "A" );
    test_derive_from< std::tuple< tag<'A'>, tag<'B'> > >( "AB" );
    test_derive_from< std::tuple< tag<'A'>, tag<'B'>, tag<'C'> > >( "ABC" );
}

namespace {

    template <
        typename List,
        typename E >
    struct add_only_tags;

    template <
        char C,
        typename ... Es >
    struct add_only_tags< std::tuple< Es... >, tag< C > >
    {
        typedef std::tuple< tag< C >, Es... > type;
    };

    template < typename ... Ms, typename T >
    struct add_only_tags< std::tuple< Ms... >, T >
    {
        typedef std::tuple< Ms... > type;
    };
}

BOOST_AUTO_TEST_CASE( fold )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::fold< std::tuple<>, add_only_tags >::type,
            std::tuple<> >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::fold< std::tuple< int, tag<'A'>, float, tag<'B'> >, add_only_tags >::type,
            std::tuple< tag<'A'>, tag<'B'> > >::value
    ) );
}

BOOST_AUTO_TEST_CASE( transform_list )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::transform_list< std::tuple<>, bluetoe::details::extract_meta_type >::type,
            std::tuple<> >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::transform_list<
                std::tuple< type1 >,
                bluetoe::details::extract_meta_type
            >::type,
            std::tuple< meta1 > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::transform_list<
                std::tuple< type1, type2 >,
                bluetoe::details::extract_meta_type
            >::type,
            std::tuple< meta1, meta2 > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::transform_list<
                std::tuple< type1, type2, type3 >,
                bluetoe::details::extract_meta_type
            >::type,
            std::tuple< meta1, meta2, meta3 > >::value
    ) );
}

BOOST_AUTO_TEST_CASE( index_of )
{
    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< int, int >::value ) );
    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< int, std::tuple< int > >::value ) );

    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< int, int, float >::value ) );
    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< int, std::tuple< int, float > >::value ) );

    BOOST_CHECK_EQUAL( 1, int( bluetoe::details::index_of< float, int, float >::value ) );
    BOOST_CHECK_EQUAL( 1, int( bluetoe::details::index_of< float, std::tuple< int, float > >::value ) );

    BOOST_CHECK_EQUAL( 2, int( bluetoe::details::index_of< bool, int, float, bool >::value ) );
    BOOST_CHECK_EQUAL( 1, int( bluetoe::details::index_of< float, std::tuple< int, float, bool > >::value ) );

    // not in list
    BOOST_CHECK_EQUAL( 3, int( bluetoe::details::index_of< double, std::tuple< int, float, bool > >::value ) );

    // not in empty list
    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< double, std::tuple<> >::value ) );
    BOOST_CHECK_EQUAL( 0, int( bluetoe::details::index_of< double >::value ) );
}

template < class A, class B >
struct sort_createria
{
    using type = typename bluetoe::details::select_type< A::value < B::value, std::true_type, std::false_type >::type;
};

template < int I >
using int_ = std::integral_constant< int, I >;

BOOST_AUTO_TEST_CASE( stable_sort_order )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria >::type,
            std::tuple<> >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, std::tuple<> >::type,
            std::tuple<> >::value
    ) );


    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 1 > >::type,
            std::tuple< int_< 1 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 1 >, int_< 2 > >::type,
            std::tuple< int_< 1 >, int_< 2 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 2 >, int_< 1 > >::type,
            std::tuple< int_< 1 >, int_< 2 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 1 >, int_< 2 >, int_< 3 > >::type,
            std::tuple< int_< 1 >, int_< 2 >, int_< 3 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 2 >, int_< 1 >, int_< 3 > >::type,
            std::tuple< int_< 1 >, int_< 2 >, int_< 3 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 3 >, int_< 2 >, int_< 1 > >::type,
            std::tuple< int_< 1 >, int_< 2 >, int_< 3 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 1 >, int_< 3 >, int_< 2 > >::type,
            std::tuple< int_< 1 >, int_< 2 >, int_< 3 > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< sort_createria, int_< 1 >, int_< 3 >, int_< 5 >, int_< 2 >, int_< 1 > >::type,
            std::tuple< int_< 1 >, int_< 1 >, int_< 2 >, int_< 3 >, int_< 5 > > >::value
    ) );
}

template < char A, char B >
struct item {
    static const char key = A;
};

template < class first, class second >
struct order_item
{
    using type = typename bluetoe::details::select_type<
        first::key < second::key,
        std::true_type,
        std::false_type >::type;
};

BOOST_AUTO_TEST_CASE( stable_sort_stability )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< order_item, item< 'A', 'A' > >::type,
            std::tuple< item< 'A', 'A' > > >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< order_item, item< 'A', 'A' >, item< 'A', 'B' > >::type,
            std::tuple< item< 'A', 'A' >, item< 'A', 'B' > > >::value
    ) );


    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::stable_sort< order_item, item< 'B', 'A' >, item< 'A', 'B' >, item< 'B', 'B' >, item< 'A', 'C' > >::type,
            std::tuple< item< 'A', 'B' >, item< 'A', 'C' >, item< 'B', 'A' >, item< 'B', 'B' > > >::value
    ) );
}

BOOST_AUTO_TEST_CASE( last_type )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_type< std::tuple<> >::type,
            bluetoe::details::no_such_type >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_type< std::tuple<>, char >::type,
            char >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_type< std::tuple< char > >::type,
            char >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::last_type< std::tuple< char, double > >::type,
            double >::value
    ) );
}

BOOST_AUTO_TEST_CASE( map_find )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_find< std::tuple<>, int >::type,
            bluetoe::details::no_such_type
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_find< std::tuple<>, int, float >::type,
            float
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_find<
                std::tuple<
                    bluetoe::details::pair< int, char >
                >, int >::type,
            char
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_find<
                std::tuple<
                    bluetoe::details::pair< int, char >, bluetoe::details::pair< float, bool >
                >, float >::type,
            bool
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_find<
                std::tuple<
                    bluetoe::details::pair< int, char >, bluetoe::details::pair< float, bool >
                >, double >::type,
            bluetoe::details::no_such_type
        >::value
    ) );
}

BOOST_AUTO_TEST_CASE( map_erase )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_erase< std::tuple<>, int >::type,
            std::tuple<>
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_erase< std::tuple< bluetoe::details::pair< int, char > >, int >::type,
            std::tuple<>
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_erase< std::tuple< bluetoe::details::pair< int, char > >, bool >::type,
            std::tuple< bluetoe::details::pair< int, char > >
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_erase<
                std::tuple<
                    bluetoe::details::pair< int, char >,
                    bluetoe::details::pair< bool, char >,
                    bluetoe::details::pair< float, char >
                >, bool >::type,
            std::tuple< bluetoe::details::pair< int, char >, bluetoe::details::pair< float, char > >
        >::value
    ) );
}

BOOST_AUTO_TEST_CASE( map_insert )
{
    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_insert< std::tuple<>, int, char >::type,
            std::tuple< bluetoe::details::pair< int, char > >
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_insert< std::tuple< bluetoe::details::pair< int, char > >, int, char >::type,
            std::tuple< bluetoe::details::pair< int, char > >
        >::value
    ) );

    BOOST_CHECK( (
        std::is_same<
            typename bluetoe::details::map_insert< std::tuple< bluetoe::details::pair< int, bool > >, int, char >::type,
            std::tuple< bluetoe::details::pair< int, char > >
        >::value
    ) );
}
