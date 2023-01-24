#ifndef SPL_GPIO_HPP
#define SPL_GPIO_HPP

#include <bluetoe/meta_tools.hpp>

#include <type_traits>

#include <nrf.h>

namespace spl {

    namespace details {
        struct valid_input_pin_parameter_meta_type {};
        struct valid_output_pin_parameter_meta_type {};
        struct output_inverter_meta_type {};
    }

    /**
     * @brief an input pin
     */
    template < typename ... Options >
    struct input_pin
    {
    public:
        input_pin();

        bool value();
    };

    /**
     * @brief inverts the output corresponding to the given value
     */
    struct inverted_input {};

    struct enable_pullup {};

    struct not_inverted_output {
        struct meta_type
            : details::valid_output_pin_parameter_meta_type
            , details::output_inverter_meta_type
        {};

        static bool invert( bool b )
        {
            return b;
        }
    };

    template < typename ... Options >
    struct output_pin
    {
    public:
        output_pin();

        void value( bool new_value );

    private:
        static_assert( std::is_same<
                typename bluetoe::details::find_by_not_meta_type<
                    details::valid_output_pin_parameter_meta_type,
                    Options...
                >::type, bluetoe::details::no_such_type
            >::value, "Parameter passed to a output_pin that is not a valid output_pin option!" );

        using inverter = typename bluetoe::details::find_by_meta_type< details::output_inverter_meta_type, Options..., not_inverted_output >::type;
    };

    /**
     * @brief inverts the output corresponding to the given value
     */
    struct inverted_output {
        struct meta_type
            : details::valid_output_pin_parameter_meta_type
            , details::output_inverter_meta_type
        {};

        static bool invert( bool b )
        {
            return not b;
        }
    };

    /**
     * @brief a pin for an input or output on port0
     */
    template < std::uint32_t Pin >
    struct pin_p0 {
        struct meta_type : details::valid_output_pin_parameter_meta_type {};
    };

    // implementation:
    template < typename ... Options >
    output_pin< Options... >::output_pin()
    {
    }

    template < typename ... Options >
    void output_pin< Options... >::value( bool new_value )
    {
        new_value = inverter::invert( new_value );
    }
}

#endif
