#define BOOST_TEST_MODULE
#include <boost/test/included/unit_test.hpp>

#include <bluetoe/io_capabilities.hpp>

BOOST_AUTO_TEST_CASE( by_default_no_input_no_output )
{
    using capa = bluetoe::details::io_capabilities_matrix<>;
    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::no_input_no_output );
}

BOOST_AUTO_TEST_CASE( explicitly_state_no_output )
{
    using capa = bluetoe::details::io_capabilities_matrix< bluetoe::pairing_no_output >;
    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::no_input_no_output );
}

BOOST_AUTO_TEST_CASE( explicitly_state_no_input )
{
    using capa = bluetoe::details::io_capabilities_matrix< bluetoe::pairing_no_input >;
    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::no_input_no_output );
}

struct pairing_yes_no_handler_t
{
     bool sm_pairing_yes_no();
} pairing_yes_no_handler;

BOOST_AUTO_TEST_CASE( no_output_yes_no_input )
{
    using capa = bluetoe::details::io_capabilities_matrix< bluetoe::pairing_yes_no< pairing_yes_no_handler_t, pairing_yes_no_handler > >;
    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::no_input_no_output );
}

struct pairing_keyboard_handler_t
{

} pairing_keyboard_handler;

BOOST_AUTO_TEST_CASE( no_output_keyboard_input )
{
    using capa = bluetoe::details::io_capabilities_matrix<
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler > >;

    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::keyboard_only );
}

struct pairing_numeric_output_handler_t
{

} pairing_numeric_output_handler;

BOOST_AUTO_TEST_CASE( Numeric_output__No_input )
{
    using capa = bluetoe::details::io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::display_only );
}

BOOST_AUTO_TEST_CASE( Numeric_output__Yes_No_input )
{
    using capa = bluetoe::details::io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler >,
        bluetoe::pairing_yes_no< pairing_yes_no_handler_t, pairing_yes_no_handler > >;

    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::display_yes_no );
}

BOOST_AUTO_TEST_CASE( Numeric_output__keyboard_input )
{
    using capa = bluetoe::details::io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler >,
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler > >;

    BOOST_CHECK( capa::get_io_capabilities() == bluetoe::details::io_capabilities::keyboard_display );
}
