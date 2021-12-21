#ifndef BLUETOE_SM_INCLUDE_IO_CAPABILTIES_HPP
#define BLUETOE_SM_INCLUDE_IO_CAPABILTIES_HPP

#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_meta_types.hpp>
#include <bluetoe/bits.hpp>

namespace bluetoe
{
    namespace details {
        struct pairing_input_capabilty_meta_type {};
        struct pairing_output_capabilty_meta_type {};
        struct pairing_just_works_meta_type {};

        enum class io_capabilities : std::uint8_t {
            display_only            = 0x00,
            display_yes_no          = 0x01,
            keyboard_only           = 0x02,
            no_input_no_output      = 0x03,
            keyboard_display        = 0x04,
            last = keyboard_display
        };

        enum class legacy_pairing_algorithm : std::uint8_t {
            just_works,
            oob_authentication,
            passkey_entry_display,
            passkey_entry_input
        };

    }

    /**
     * @brief defines that the device has no way to receive input from the user during pairing
     *
     * This is the default, if no other pairing input capability is given.
     *
     * @sa pairing_yes_no
     * @sa pairing_keyboard
     */
    struct pairing_no_input
    {
        /** @cond HIDDEN_SYMBOLS */
        static std::array< std::uint8_t, 16 > sm_pairing_passkey()
        {
            return {{0}};
        }

        struct meta_type :
            details::pairing_input_capabilty_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief Device has at least two buttons that can be easily mapped to 'yes'
     *        and 'no' or the device has a mechanism whereby the user can
     *        indicate either 'yes' or 'no'.
     *
     * The parameter T have to be a class type with following none static member
     * function:
     *
     * bool sm_pairing_yes_no();
     *
     * @sa pairing_keyboard
     * @sa pairing_no_input
     */
    template < typename T, T& Obj >
    struct pairing_yes_no
    {
        /** @cond HIDDEN_SYMBOLS */
        static std::array< std::uint8_t, 16 > sm_pairing_passkey()
        {
            return {{0}};
        }

        struct meta_type :
            details::pairing_input_capabilty_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief Device has a numeric keyboard that can input the numbers '0' to '9' and a confirmation.
     *
     * Device also has at least two buttons that can be easily mapped to 'yes' and 'no' or the device
     * has a mechanism whereby the user can indicate either 'yes' or 'no'.
     *
     * The parameter T have to be a class type with following none static member
     * functions:
     *
     * int sm_pairing_passkey();
     * bool sm_pairing_yes_no();
     *
     * @sa pairing_yes_no
     * @sa pairing_no_input
     */
    template < typename T, T& Obj >
    struct pairing_keyboard
    {
        /** @cond HIDDEN_SYMBOLS */
        static std::array< std::uint8_t, 16 > sm_pairing_passkey()
        {
            std::array< std::uint8_t, 16 > result = {{ 0 }};
            details::write_32bit( result.data(), Obj.sm_pairing_passkey() );

            return result;
        }

        struct meta_type :
            details::pairing_input_capabilty_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief defines that the device has no means to output a numeric value
     */
    struct pairing_no_output
    {
        /** @cond HIDDEN_SYMBOLS */
        static details::io_capabilities get_io_capabilities( const pairing_no_input& )
        {
            return details::io_capabilities::no_input_no_output;
        }

        template < typename T, T& Obj >
        static details::io_capabilities get_io_capabilities( const pairing_yes_no< T, Obj >& )
        {
            return details::io_capabilities::no_input_no_output;
        }

        template < typename T, T& Obj >
        static details::io_capabilities get_io_capabilities( const pairing_keyboard< T, Obj >& )
        {
            return details::io_capabilities::keyboard_only;
        }

        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_no_input&, details::io_capabilities /* io_capability */ )
        {
            return details::legacy_pairing_algorithm::just_works;
        }

        template < typename T, T& Obj >
        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_yes_no< T, Obj >&, details::io_capabilities /* io_capability */ )
        {
            return details::legacy_pairing_algorithm::just_works;
        }

        template < typename T, T& Obj >
        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_keyboard< T, Obj >&, details::io_capabilities io_capability )
        {
            if ( io_capability == details::io_capabilities::no_input_no_output )
                return details::legacy_pairing_algorithm::just_works;

            return details::legacy_pairing_algorithm::passkey_entry_input;
        }

        static void sm_pairing_numeric_output( const std::array< std::uint8_t, 16 >& )
        {
        }

        struct meta_type :
            details::pairing_output_capabilty_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief the device has a mean to output the 6 digit pass key used during
     *        the pairing process.
     *
     * The parameter T have to be a class type with following none static member
     * function:
     *
     * void sm_pairing_numeric_output( int pass_key );
     *
     * @sa pairing_no_output
     */
    template < typename T, T& Obj >
    struct pairing_numeric_output
    {
        /** @cond HIDDEN_SYMBOLS */
        static details::io_capabilities get_io_capabilities( const pairing_no_input& )
        {
            return details::io_capabilities::display_only;
        }

        template < typename O, O& Other >
        static details::io_capabilities get_io_capabilities( const pairing_yes_no< O, Other >& )
        {
            return details::io_capabilities::display_yes_no;
        }

        template < typename O, O& Other >
        static details::io_capabilities get_io_capabilities( const pairing_keyboard< O, Other >& )
        {
            return details::io_capabilities::keyboard_display;
        }

        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_no_input&, details::io_capabilities io_capability )
        {
            if ( io_capability == details::io_capabilities::keyboard_only || io_capability == details::io_capabilities::keyboard_display )
                return details::legacy_pairing_algorithm::passkey_entry_display;

            return details::legacy_pairing_algorithm::just_works;
        }

        template < typename O, O& Other >
        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_yes_no< O, Other >&, details::io_capabilities io_capability )
        {
            if ( io_capability == details::io_capabilities::keyboard_only || io_capability == details::io_capabilities::keyboard_display )
                return details::legacy_pairing_algorithm::passkey_entry_display;

            return details::legacy_pairing_algorithm::just_works;
        }

        template < typename O, O& Other >
        static details::legacy_pairing_algorithm select_legacy_pairing_algorithm( const pairing_keyboard< O, Other >&, details::io_capabilities io_capability )
        {
            if ( io_capability == details::io_capabilities::no_input_no_output )
                return details::legacy_pairing_algorithm::just_works;

            if ( io_capability == details::io_capabilities::keyboard_only )
                return details::legacy_pairing_algorithm::passkey_entry_display;

            return details::legacy_pairing_algorithm::passkey_entry_input;
        }

        static void sm_pairing_numeric_output( const std::array< std::uint8_t, 16 >& key )
        {
            Obj.sm_pairing_numeric_output( static_cast< int >( details::read_32bit( key.data() ) ) );
        }

        struct meta_type :
            details::pairing_output_capabilty_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief mark that "just works" pairing is not acceptable
     *
     * This is the default, if the device has IO capabilties other than
     * pairing_no_input and pairing_no_output (or pairing_yes_no and pairing_no_output)
     * defined.
     *
     * @sa pairing_allow_just_works
     */
    struct pairing_no_just_works
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::pairing_just_works_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief mark that "just works" pairing is accepted, even when the device have
     *        IO capabilities to exchange a pass key
     *
     * @sa pairing_no_just_works
     */
    struct pairing_allow_just_works
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::pairing_just_works_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};
        /** @endcond */
    };

    namespace details
    {
        template < typename ... Options >
        class io_capabilities_matrix
        {
        public:
            using input_capabilities  = typename find_by_meta_type< pairing_input_capabilty_meta_type, Options..., pairing_no_input >::type;
            using output_capabilities = typename find_by_meta_type< pairing_output_capabilty_meta_type, Options..., pairing_no_output >::type;

            static io_capabilities get_io_capabilities()
            {
                return output_capabilities::get_io_capabilities(input_capabilities());
            }

            static legacy_pairing_algorithm select_legacy_pairing_algorithm( std::uint8_t io_capability )
            {
                return select_legacy_pairing_algorithm( static_cast< io_capabilities >( io_capability ) );
            }

            static legacy_pairing_algorithm select_legacy_pairing_algorithm( io_capabilities io_capability )
            {
                return output_capabilities::select_legacy_pairing_algorithm( input_capabilities(), io_capability );
            }

            static void sm_pairing_numeric_output( const std::array< std::uint8_t, 16 >& temp_key )
            {
                output_capabilities::sm_pairing_numeric_output( temp_key );
            }

            static std::array< std::uint8_t, 16 > sm_pairing_passkey()
            {
                return input_capabilities::sm_pairing_passkey();
            }
        };
    }
}

#endif // include guard
