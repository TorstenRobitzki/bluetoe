#ifndef BLUETOE_EXAMPLES_RESOURCES_HPP
#define BLUETOE_EXAMPLES_RESOURCES_HPP

#include <spl/gpio.hpp>

namespace examples {

    template < std::uint32_t Pin >
    using nordic_output = spl::output_pin<
        spl::pin_p0< Pin >,
        spl::inverted_output
    >;

    template < std::uint32_t Pin >
    using nordic_input = spl::input_pin<
        spl::pin_p0< Pin >,
        spl::inverted_input,
        spl::enable_pullup
    >;

#if defined BLUETOE_BOARD_PCA10056

    using led1    = nordic_output < 13 >;
    using led2    = nordic_output < 14 >;
    using led3    = nordic_output < 15 >;
    using led4    = nordic_output < 16 >;
    using button1 = nordic_input< 11 >;
    using button2 = nordic_input< 12 >;
    using button3 = nordic_input< 24 >;
    using button4 = nordic_input< 25 >;

#elif defined BLUETOE_BOARD_PCA10040

    using led1    = nordic_output < 17 >;
    using led2    = nordic_output < 18 >;
    using led3    = nordic_output < 19 >;
    using led4    = nordic_output < 20 >;
    using button1 = nordic_input< 13 >;
    using button2 = nordic_input< 14 >;
    using button3 = nordic_input< 15 >;
    using button4 = nordic_input< 16 >;

#else

    using led1    = nordic_output < 13 >;
    using led2    = nordic_output < 14 >;
    using led3    = nordic_output < 15 >;
    using led4    = nordic_output < 16 >;
    using button1 = nordic_input< 11 >;
    using button2 = nordic_input< 12 >;
    using button3 = nordic_input< 24 >;
    using button4 = nordic_input< 25 >;

#endif

    using led    = led1;
    using button = button1;

} // namespace examples

#endif
