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
        struct input_inverter_meta_type {};
        struct pin_address_meta_type {};
        struct pin_pullup_config_meta_type {};
        struct pin_initial_output_activity_meta_type {};
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
    struct inverted_input {
        struct meta_type
            : details::valid_input_pin_parameter_meta_type
            , details::input_inverter_meta_type
        {};

        static bool invert( bool b )
        {
            return !b;
        }
    };

    struct not_inverted_input {
        struct meta_type
            : details::valid_input_pin_parameter_meta_type
            , details::input_inverter_meta_type
        {};

        static bool invert( bool b )
        {
            return b;
        }
    };

    struct enable_pullup {
        struct meta_type
            : details::valid_input_pin_parameter_meta_type
            , details::pin_pullup_config_meta_type
        {};

    };

    struct no_pullup {
        struct meta_type
            : details::valid_input_pin_parameter_meta_type
            , details::pin_pullup_config_meta_type
        {};

    };

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

    struct initial_output_active {
        struct meta_type
            : details::valid_output_pin_parameter_meta_type
            , details::pin_initial_output_activity_meta_type
        {};
    };

    struct initial_output_inactive {
        struct meta_type
            : details::valid_output_pin_parameter_meta_type
            , details::pin_initial_output_activity_meta_type
        {};
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
        using pin      = typename bluetoe::details::find_by_meta_type< details::pin_address_meta_type, Options... >::type;
        using pullup   = typename bluetoe::details::find_by_meta_type< details::pin_pullup_config_meta_type, Options..., no_pullup >::type;
        using init_val = typename bluetoe::details::find_by_meta_type< details::pin_initial_output_activity_meta_type, Options..., initial_output_inactive >::type;

        static_assert( !std::is_same< pin, bluetoe::details::no_such_type >::value, "Missing PIN configuration." );

        static constexpr bool use_pullup     = std::is_same< pullup, enable_pullup >::value;
        static constexpr bool initial_active = std::is_same< init_val, initial_output_active >::value;
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
        struct meta_type
            : details::valid_output_pin_parameter_meta_type
            , details::pin_address_meta_type
        {};

        static constexpr std::uint32_t base_address = NRF_P0_BASE;

        template < bool Pullup >
        static void init_output( bool start_value )
        {
            set( start_value );

            reinterpret_cast< NRF_GPIO_Type* >( base_address )->PIN_CNF[ Pin ] =
                ( ( Pullup ? GPIO_PIN_CNF_PULL_Pullup : GPIO_PIN_CNF_PULL_Disabled ) << GPIO_PIN_CNF_PULL_Pos )
              | ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );
        }

        static void set( bool new_value )
        {
            if ( new_value )
                reinterpret_cast< NRF_GPIO_Type* >( base_address )->OUTSET = ( 1 << Pin );
            else
                reinterpret_cast< NRF_GPIO_Type* >( base_address )->OUTCLR = ( 1 << Pin );
        }
    };

    // implementation:
    template < typename ... Options >
    output_pin< Options... >::output_pin()
    {
        pin::template init_output< use_pullup >( inverter::invert( initial_active ) );
    }

    template < typename ... Options >
    void output_pin< Options... >::value( bool new_value )
    {
        pin::set( inverter::invert( new_value ) );
    }
}

#endif
