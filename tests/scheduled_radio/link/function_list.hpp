#ifndef BLUETOE_TESTS_SCHEDULED_RADIO_LINK_FUNCTION_LIST_HPP
#define BLUETOE_TESTS_SCHEDULED_RADIO_LINK_FUNCTION_LIST_HPP

/**
 * @file function_list.hpp
 *
 * The request and response protocol on top of the frames, as both ends see it. See
 * documentation/scheduled_radio_test_rig.md, decisions 6, 8 and 19.
 *
 * Both sides share a function_list of member function pointers. A function's opcode is
 * its position in the list; its arguments and result are taken from its signature and
 * serialised as serialize.hpp defines. The payloads are
 *
 *     request:  opcode | arguments
 *     response: token | status | result
 *
 * The opcode is one byte. The token is the session token of decision 8, four bytes. The
 * status is one byte. The result is present only for status::ok and only if the function
 * returns something.
 *
 * The instrument side is instrument/dispatcher.hpp, the host side is host/proxy.hpp.
 */

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace bluetoe {
namespace test_rig {

    /**
     * @brief the functions callable over the link, in opcode order
     */
    template < auto... Functions >
    struct function_list
    {
        static constexpr std::size_t size = sizeof...( Functions );
    };

    template < typename F >
    struct member_function_traits;

    template < typename R, typename C, typename... Args >
    struct member_function_traits< R ( C::* )( Args... ) >
    {
        using result    = R;
        using object    = C;
        using arguments = std::tuple< std::remove_cvref_t< Args >... >;
    };

    template < typename R, typename C, typename... Args >
    struct member_function_traits< R ( C::* )( Args... ) const >
        : member_function_traits< R ( C::* )( Args... ) > {};

    namespace details {

        template < auto A, auto B >
        inline constexpr bool same_function = false;

        template < auto A >
        inline constexpr bool same_function< A, A > = true;

        template < auto F, auto... Functions >
        constexpr std::size_t index_of()
        {
            constexpr bool matches[] = { same_function< F, Functions >..., false };

            for ( std::size_t i = 0; i != sizeof...( Functions ); ++i )
                if ( matches[ i ] )
                    return i;

            return sizeof...( Functions );
        }
    }

    /**
     * @brief the opcode of F in the list, or the size of the list if F is not in it
     */
    template < auto F, typename List >
    struct opcode_of;

    template < auto F, auto... Functions >
    struct opcode_of< F, function_list< Functions... > >
        : std::integral_constant< std::size_t, details::index_of< F, Functions... >() > {};
}
}

#endif
