#ifndef BLUETOE_OPTIONS_HPP
#define BLUETOE_OPTIONS_HPP

#include <utility>
#include <type_traits>
#include <tuple>

/**
 * @file bluetoe/options.hpp
 * This file contains all the generic meta template functions that are required by bluetoe
 */
namespace bluetoe {
namespace details {

    /*
     * Select A or B by Select. If Select == true, the result is A; B otherwise
     */
    template < bool Select, typename A, typename B >
    struct select_type {
        typedef A type;
    };

    template < typename A, typename B >
    struct select_type< false, A, B > {
        typedef B type;
    };

    /*
     * Selects a template according to the given Select parameter. If select is true,
     * the result will be A; B otherwise
     *
     * The result is named type, but it is a template.
     */
    template < bool Select, template < typename > class A, template < typename > class B >
    struct select_template_t1 {
        template < typename T1 >
        using type = A< T1 >;
    };

    template < template < typename > class A, template < typename > class B >
    struct select_template_t1< false, A, B > {
        template < typename T1 >
        using type = B< T1 >;
    };

    /*
     * return A if A is not Null, otherwise return B if B is not Null, otherwise Null
     */
    template < typename Null, typename A, typename B >
    struct or_type {
        typedef A type;
    };

    template < typename Null, typename B >
    struct or_type< Null, Null, B > {
        typedef B type;
    };

    /*
     *  add A to B
     */
    template < class A, class B >
    struct add_type
    {
        typedef std::tuple< A, B > type;
    };

    template < class T, class ... Ts >
    struct add_type< T, std::tuple< Ts... > >
    {
        typedef std::tuple< T, Ts... > type;
    };

    template < class ...Ts, class T >
    struct add_type< std::tuple< Ts... >, T >
    {
        typedef std::tuple< Ts..., T > type;
    };

    template < class ...As, class ...Bs >
    struct add_type< std::tuple< As... >, std::tuple< Bs...> >
    {
        typedef std::tuple< As..., Bs... > type;
    };

    /*
     * wildcard to be used when templates should be used within remove_if_equal
     */
    struct wildcard {};

    /*
     * Removes from a typelist (std::tuple) all elments that are equal to Zero
     */
    template < typename A, typename Zero >
    struct remove_if_equal;

    template < typename Zero >
    struct remove_if_equal< std::tuple<>, Zero >
    {
        typedef std::tuple<> type;
    };

    template <
        typename Type,
        typename Zero
     >
    struct remove_if_equal< std::tuple< Type >, Zero >
    {
        typedef std::tuple< Type > type;
    };

    // to types are equal if they are the same types
    template <
        typename Zero
     >
    struct remove_if_equal< std::tuple< Zero >, Zero >
    {
        typedef std::tuple<> type;
    };

    // to types are equal if they are both templates and the Zero type has it's parameters replaces with wildcards
    template <
        template < typename > class Templ,
        typename Type
     >
    struct remove_if_equal< std::tuple< Templ< Type > >, Templ< wildcard > >
    {
        typedef std::tuple<> type;
    };

    // to types are equal if they are both templates and the Zero type has it's parameters replaces with wildcards
    template <
        template < typename ... > class Templ,
        typename Type
     >
    struct remove_if_equal< std::tuple< Templ< Type > >, Templ< wildcard > >
    {
        typedef std::tuple<> type;
    };

    template <
        typename Type,
        typename ... Types,
        typename Zero
     >
    struct remove_if_equal< std::tuple< Type, Types... >, Zero >
    {
        typedef typename add_type<
            typename remove_if_equal< std::tuple< Type >, Zero >::type,
            typename remove_if_equal< std::tuple< Types... >, Zero >::type
        >::type type;
    };

    /*
     * a tuple type with two elements
     */
    template < typename A, typename B >
    struct pair {
        typedef A first_type;
        typedef B second_type;
    };

    template < typename T >
    struct not_type
    {
        typedef std::integral_constant< bool, !T::value > type;
    };

    /*
     * empty result
     */
    struct no_such_type {};

    template < typename T >
    struct extract_meta_type
    {
        template < class U >
        static typename U::meta_type check( U* )  { return U::meta_type(); }
        static no_such_type          check( ... ) { return no_such_type(); }

        typedef decltype( check( static_cast< T* >( nullptr ) ) ) meta_type;
        typedef meta_type                                         type;
        typedef typename not_type<
            typename std::is_same<
                meta_type, no_such_type >::type >::type  has_meta_type;
    };

    /*
     * finds the first type that has the embedded meta_type
     */
    template <
        typename MetaType,
        typename ... Types >
    struct find_by_meta_type;

    template <
        typename MetaType >
    struct find_by_meta_type< MetaType >
    {
        typedef no_such_type type;
    };

    template <
        typename MetaType,
        typename Type >
    struct find_by_meta_type< MetaType, Type >
    {
        typedef extract_meta_type< Type > meta_type;
        typedef typename select_type<
            std::is_convertible< typename meta_type::type*, MetaType* >::value,
            Type, no_such_type >::type type;
    };

    template <
        typename MetaType,
        typename Type,
        typename ... Types >
    struct find_by_meta_type< MetaType, Type, Types... >
    {
        typedef typename or_type<
            no_such_type,
            typename find_by_meta_type< MetaType, Type >::type,
            typename find_by_meta_type< MetaType, Types... >::type >::type type;
    };

    /*
     * finds all types that has the embedded meta_type
     */
    template <
        typename MetaType,
        typename ... Types >
    struct find_all_by_meta_type;

    template <
        typename MetaType >
    struct find_all_by_meta_type< MetaType >
    {
        typedef std::tuple<> type;
    };

    template <
        typename MetaType,
        typename Type,
        typename ... Types >
    struct find_all_by_meta_type< MetaType, Type, Types... >
    {
        typedef extract_meta_type< Type > meta_type;
        typedef typename select_type<
            std::is_convertible< typename meta_type::type*, MetaType* >::value,
            typename add_type< Type, typename find_all_by_meta_type< MetaType, Types... >::type >::type,
            typename find_all_by_meta_type< MetaType, Types... >::type >::type type;
    };

    template <
        typename MetaType,
        typename ... Types >
    struct find_all_by_meta_type< MetaType, std::tuple< Types... > > : find_all_by_meta_type< MetaType, Types... > {};

    /*
     * groups a list of types by there meta types
     *
     * Returns a std::tuple with as much elements as MetaTypes where given. Every element in the result is a tuple containing
     * the meta type followed by the result of a call to find_all_by_meta_type<>
     */
    template <
        typename Types,
        typename ... MetaTypes >
    struct group_by_meta_types;

    template <
        typename ... Types >
    struct group_by_meta_types< std::tuple< Types... > >
    {
        typedef std::tuple<> type;
    };

    template <
        typename ... Types,
        typename MetaType >
    struct group_by_meta_types< std::tuple< Types... >, MetaType >
    {
        typedef std::tuple<
            typename add_type<
                MetaType,
                typename find_all_by_meta_type< MetaType, Types... >::type
            >::type
        > type;
    };

    template <
        typename ... Types,
        typename MetaType,
        typename ... MetaTypes >
    struct group_by_meta_types< std::tuple< Types... >, MetaType, MetaTypes... >
    {
        typedef typename add_type<
            typename group_by_meta_types< std::tuple< Types... >, MetaType >::type,
            typename group_by_meta_types< std::tuple< Types... >, MetaTypes... >::type >::type type;
    };

    /**
     * @brief just like group_by_meta_types, but the result has all std::pair<> removed
     */
    template <
        typename Types,
        typename ... MetaTypes >
    struct group_by_meta_types_without_empty_groups;

    template <
        typename ... Types,
        typename ... MetaTypes >
    struct group_by_meta_types_without_empty_groups< std::tuple< Types... >, MetaTypes...>
    {
        typedef typename remove_if_equal<
            typename group_by_meta_types< std::tuple< Types... >, MetaTypes... >::type,
            std::tuple< wildcard >
        >::type type;
    };


    /*
     * counts the number of Types with a given MetaType
     */
    template <
        typename MetaType,
        typename ... Types >
    struct count_by_meta_type;

    template <
        typename MetaType >
    struct count_by_meta_type< MetaType > {
        enum { count = 0 };
    };

    template <
        typename MetaType,
        typename Type,
        typename ... Types >
    struct count_by_meta_type< MetaType, Type, Types... > {
        typedef extract_meta_type< Type > meta_type;

        enum { count = std::is_same< typename meta_type::type, MetaType >::value
            ? 1 + count_by_meta_type< MetaType, Types... >::count
            : 0 + count_by_meta_type< MetaType, Types... >::count
        };
    };

    /*
     * counts the number of types, the predicate returned true for an element applied to it
     */
    template <
        typename List,
        template < typename > class Predicate
    >
    struct count_if;

    template <
        template < typename > class Predicate
    >
    struct count_if< std::tuple<>, Predicate > : std::integral_constant< int, 0 > {};

    template <
        typename T,
        template < typename > class Predicate
    >
    struct count_if< std::tuple< T >, Predicate > : std::integral_constant< int, Predicate< T >::value ? 1 : 0 > {};

    template <
        typename T,
        typename ...Ts,
        template < typename > class Predicate
    >
    struct count_if< std::tuple< T, Ts... >, Predicate > : std::integral_constant< int,
        count_if< std::tuple< T >, Predicate >::value
      + count_if< std::tuple< Ts... >, Predicate >::value > {};

    /*
     * sums up the result of a call to Predicate with every element from List
     */
    template <
        typename List,
        template < typename > class Predicate
    >
    struct sum_by;

    template <
        template < typename > class Predicate
    >
    struct sum_by< std::tuple<>, Predicate > : std::integral_constant< int, 0 > {};

    template <
        typename T,
        template < typename > class Predicate
    >
    struct sum_by< std::tuple< T >, Predicate > : std::integral_constant< int, Predicate< T >::value > {};

    template <
        typename T,
        typename ...Ts,
        template < typename > class Predicate
    >
    struct sum_by< std::tuple< T, Ts... >, Predicate > : std::integral_constant< int,
        sum_by< std::tuple< T >, Predicate >::value
      + sum_by< std::tuple< Ts... >, Predicate >::value > {};

    /*
     * transform a list into an other list of the same length
     */
    template <
        typename List,
        template < typename > class Transform
    >
    struct transform_list;

    template <
        template < typename > class Transform
    >
    struct transform_list< std::tuple<>, Transform > {
        typedef std::tuple<> type;
    };

    template <
        typename E,
        typename ... Es,
        template < typename > class Transform
    >
    struct transform_list< std::tuple< E, Es... >, Transform > {
        typedef typename add_type<
            typename Transform< E >::type,
            typename transform_list< std::tuple< Es... >, Transform >::type
        >::type type;
    };

    /**
     * @brief returns true, if Option is given in Options
     */
    template <
        typename Option,
        typename ... Options >
    struct has_option;

    template <
        typename Option >
    struct has_option< Option > {
        static bool constexpr value = false;
    };

    template <
        typename Option,
        typename FirstOption,
        typename ... Options >
    struct has_option< Option, FirstOption, Options... > {
        static bool constexpr value =
            std::is_same< Option, FirstOption >::value || has_option< Option, Options... >::value;
    };

    /**
     * @brief executes f.each< O >() for every O in Options.
     *
     * For
     * Example:
     * @code
    struct value_printer
    {
        template< typename O >
        void each()
        {
            std::cout << x << 'n';
        }
    };

    int main()
    {
        for_< Options... >::each( value_printer() );
    }
     * @endcode
     */
    template <
        typename ... Options >
    struct for_impl;

    template <>
    struct for_impl<>
    {
        template < typename Function >
        static void each( Function )
        {}
    };

    template <
        typename Option,
        typename ... Options >
    struct for_impl< Option, Options... >
    {
        template < typename Function >
        static void each( Function f )
        {
            f.template each< Option >();
            for_impl< Options... >::each( f );
        }
    };

    template <
        typename ... Options >
    struct for_ : for_impl< Options... >
    {
    };

    template <
        typename ... Options >
    struct for_< std::tuple< Options... > > : for_impl< Options... >
    {
    };

    /**
     * @brief finds the first element in the list, for which Func< O >::value is true. With O beeing one of List
     */
    template <
        typename List,
        template < typename > class Func
    >
    struct find_if;

    template <
        template < typename > class Func
    >
    struct find_if< std::tuple<>, Func > {
        typedef no_such_type type;
    };

    template <
        typename T,
        typename ... Ts,
        template < typename > class Func
    >
    struct find_if< std::tuple< T, Ts...>, Func > {
        typedef typename select_type<
            Func< T >::value,
            T,
            typename find_if< std::tuple< Ts... >, Func >::type >::type type;
    };


    template < typename ... Ts >
    struct last_from_pack;

    template < typename T >
    struct last_from_pack< T > {
        typedef T type;
    };

    template <
        typename T,
        typename ... Ts >
    struct last_from_pack< T, Ts... > {
        typedef typename last_from_pack< Ts... >::type type;
    };

    // defines an empty type with the given meta_type
    template < typename MetaType >
    struct empty_meta_type {
        typedef MetaType meta_type;
    };

    template < typename ... Ts >
    struct derive_from_impl;

    template <>
    struct derive_from_impl<> {};

    template < typename T, typename ... Ts >
    struct derive_from_impl< T, Ts... > : T, derive_from_impl< Ts... > {};

    // derives from all types within the given typelist List
    template < typename List >
    struct derive_from;

    template < typename ... Ts >
    struct derive_from< std::tuple< Ts... > > : derive_from_impl< Ts... > {};

    // fold a list with an operation
    template <
        typename List,
        template < typename ListP, typename ElementP > class Operation,
        typename Start = std::tuple<> >
    struct fold;

    template <
        template < typename List, typename Element > class Operation,
        typename Start >
    struct fold< std::tuple<>, Operation, Start >
    {
        typedef Start type;
    };

    template <
        typename T,
        typename ... Ts,
        template < typename List, typename Element > class Operation,
        typename Start >
    struct fold< std::tuple< T, Ts... >, Operation, Start >
    {
        typedef typename Operation< typename fold< std::tuple< Ts... >, Operation, Start >::type, T >::type type;
    };

    // for a T in Ts, find the index of T in Ts
    template < typename T, typename ... Ts >
    struct index_of;

    template < typename T, typename ... Ts >
    struct index_of< T, T, Ts... > {
        static constexpr unsigned value = 0;
    };

    template < typename T, typename O, typename ... Ts >
    struct index_of< T, O, Ts... > {
        static constexpr unsigned value = index_of< T, Ts... >::value + 1;
    };

    template < typename T, typename ... Ts >
    struct index_of< T, std::tuple< Ts... > > : index_of< T, Ts... > {};
}
}

#endif

