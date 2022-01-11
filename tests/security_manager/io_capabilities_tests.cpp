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
    void sm_pairing_numeric_output( int pass_key )
    {
        displayed = pass_key;
    }

    int displayed;
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

BOOST_AUTO_TEST_CASE( lesc_numeric_comparison_display )
{
    pairing_numeric_output_handler.displayed = ~0;

    using capa = bluetoe::details::io_capabilities_matrix<
        bluetoe::pairing_yes_no< pairing_yes_no_handler_t, pairing_yes_no_handler >,
        bluetoe::pairing_numeric_output< pairing_numeric_output_handler_t, pairing_numeric_output_handler > >;

    static const std::array< std::uint8_t, 32 > pka = {{
        0xe6, 0x9d, 0x35, 0x0e,
        0x48, 0x01, 0x03, 0xcc,
        0xdb, 0xfd, 0xf4, 0xac,
        0x11, 0x91, 0xf4, 0xef,
        0xb9, 0xa5, 0xf9, 0xe9,
        0xa7, 0x83, 0x2c, 0x5e,
        0x2c, 0xbe, 0x97, 0xf2,
        0xd2, 0x03, 0xb0, 0x20
    }};

    static const std::array< std::uint8_t, 32 > pkb = {{
        0xfd, 0xc5, 0x7f, 0xf4,
        0x49, 0xdd, 0x4f, 0x6b,
        0xfb, 0x7c, 0x9d, 0xf1,
        0xc2, 0x9a, 0xcb, 0x59,
        0x2a, 0xe7, 0xd4, 0xee,
        0xfb, 0xfc, 0x0a, 0x90,
        0x9a, 0xbb, 0xf6, 0x32,
        0x3d, 0x8b, 0x18, 0x55
    }};

    static const std::array< std::uint8_t, 16 > na = {{
        0xab, 0xae, 0x2b, 0x71,
        0xec, 0xb2, 0xff, 0xff,
        0x3e, 0x73, 0x77, 0xd1,
        0x54, 0x84, 0xcb, 0xd5
    }};

    static const std::array< std::uint8_t, 16 > nb = {{
        0xcf, 0xc4, 0x3d, 0xff,
        0xf7, 0x83, 0x65, 0x21,
        0x6e, 0x5f, 0xa7, 0x25,
        0xcc, 0xe7, 0xe8, 0xa6
    }};

    struct state_t {
        const std::array< std::uint8_t, 16 >& local_nonce() const
        {
            return nb;
        }

        const std::array< std::uint8_t, 16 >& remote_nonce() const
        {
            return na;
        }

        const std::uint8_t* local_public_key_x() const
        {
            return pkb.data();
        }

        const std::uint8_t* remote_public_key_x() const
        {
            return pka.data();
        }
    } state;

    struct functions_t {
        std::uint32_t g2( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& x, const std::array< std::uint8_t, 16 >& y )
        {
            BOOST_CHECK_EQUAL_COLLECTIONS( u, u + 32, pka.begin(), pka.end() );
            BOOST_CHECK_EQUAL_COLLECTIONS( v, v + 32, pkb.begin(), pkb.end() );
            BOOST_CHECK_EQUAL_COLLECTIONS( x.begin(), x.end(), na.begin(), na.end() );
            BOOST_CHECK_EQUAL_COLLECTIONS( y.begin(), y.end(), nb.begin(), nb.end() );

            return 0x22334455;
        }

    } functions;

    capa::sm_pairing_numeric_compare_output( state, functions );
    BOOST_CHECK_EQUAL( pairing_numeric_output_handler.displayed, 0x22334455 );
}
