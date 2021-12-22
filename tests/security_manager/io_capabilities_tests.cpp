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

BOOST_AUTO_TEST_CASE( legacy_Display_Only_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_only ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_yes_no ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_only ) == legacy_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::no_input_no_output ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_display ) == legacy_pairing_algorithm::passkey_entry_display );
}

BOOST_AUTO_TEST_CASE( lesc_Display_Only_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_only ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_yes_no ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_only ) == lesc_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::no_input_no_output ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_display ) == lesc_pairing_algorithm::passkey_entry_display );
}

BOOST_AUTO_TEST_CASE( legacy_Display_Yes_No_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler >,
        bluetoe::pairing_yes_no< pairing_yes_no_handler_t, pairing_yes_no_handler > >;

    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_only ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_yes_no ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_only ) == legacy_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::no_input_no_output ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_display ) == legacy_pairing_algorithm::passkey_entry_display );
}

BOOST_AUTO_TEST_CASE( lesc_Display_Yes_No_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler >,
        bluetoe::pairing_yes_no< pairing_yes_no_handler_t, pairing_yes_no_handler > >;

    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_only ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_yes_no ) == lesc_pairing_algorithm::numeric_comparison );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_only ) == lesc_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::no_input_no_output ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_display ) == lesc_pairing_algorithm::numeric_comparison );
}

BOOST_AUTO_TEST_CASE( legacy_Keyboard_Only_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler > >;

    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_only ) == legacy_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_yes_no ) == legacy_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_only ) == legacy_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::no_input_no_output ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_display ) == legacy_pairing_algorithm::passkey_entry_input );
}

BOOST_AUTO_TEST_CASE( lesc_Keyboard_Only_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler > >;

    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_only ) == lesc_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_yes_no ) == lesc_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_only ) == lesc_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::no_input_no_output ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_display ) == lesc_pairing_algorithm::passkey_entry_input );
}

BOOST_AUTO_TEST_CASE( legacy_No_Input_No_Output_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<>;

    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_only ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_yes_no ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_only ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::no_input_no_output ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_display ) == legacy_pairing_algorithm::just_works );
}

BOOST_AUTO_TEST_CASE( lesc_No_Input_No_Output_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<>;

    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_only ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_yes_no ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_only ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::no_input_no_output ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_display ) == lesc_pairing_algorithm::just_works );
}

BOOST_AUTO_TEST_CASE( legacy_Keyboard_Display_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler >,
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_only ) == legacy_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::display_yes_no ) == legacy_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_only ) == legacy_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::no_input_no_output ) == legacy_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_legacy_pairing_algorithm( io_capabilities::keyboard_display ) == legacy_pairing_algorithm::passkey_entry_input );
}

BOOST_AUTO_TEST_CASE( lesc_Keyboard_Display_Pairing_Algorithm )
{
    using namespace bluetoe::details;

    using capa = io_capabilities_matrix<
        bluetoe::pairing_keyboard< pairing_keyboard_handler_t, pairing_keyboard_handler >,
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_only ) == lesc_pairing_algorithm::passkey_entry_input );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::display_yes_no ) == lesc_pairing_algorithm::numeric_comparison );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_only ) == lesc_pairing_algorithm::passkey_entry_display );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::no_input_no_output ) == lesc_pairing_algorithm::just_works );
    BOOST_CHECK( capa::select_lesc_pairing_algorithm( io_capabilities::keyboard_display ) == lesc_pairing_algorithm::numeric_comparison );
}
