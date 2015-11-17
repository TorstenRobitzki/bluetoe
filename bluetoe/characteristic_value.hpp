#ifndef BLUETOE_CHARACTERISTIC_VALUE_HPP
#define BLUETOE_CHARACTERISTIC_VALUE_HPP

#include <bluetoe/options.hpp>
#include <bluetoe/attribute.hpp>

namespace bluetoe {

    namespace details {
        struct characteristic_value_meta_type {};
        struct characteristic_parameter_meta_type {};
        struct characteristic_value_declaration_parameter {};
        struct client_characteristic_configuration_parameter {};
    }

    /**
     * @brief if added as option to a characteristic, read access is removed from the characteristic
     *
     * Even if read access was the only remaining access type, the characterist will not be readable.
     *
     * Example:
     * @code
        std::uint32_t simple_value = 0xaabbccdd;

        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_read_access > >
     * @endcode
     * @sa characteristic
     */
    class no_read_access {};

    /**
     * @brief if added as option to a characteristic, write access is removed from the characteristic
     *
     * Even if write access was the only remaining access type, the characterist will not be writeable.
     *
     * Example:
     * @code
        std::uint32_t simple_value = 0xaabbccdd;

        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xD0B10674, 0x6DDD, 0x4B59, 0x89CA, 0xA009B78C956B >,
            bluetoe::bind_characteristic_value< std::uint32_t, &simple_value >,
            bluetoe::no_write_access > >
     * @endcode
     * @sa characteristic
     */
    class no_write_access {};

    /**
     * @brief adds the ability to notify this characteristic.
     *
     * When a characteristic gets notified, the current value of the characteristic will be send to all
     * connected clients that have subscribed for notifications.
     * @sa server::notify
     */
    struct notify {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type : details::client_characteristic_configuration_parameter, details::characteristic_parameter_meta_type {};
        /** @endcond */
    };

    /**
     * @brief a very simple device to bind a characteristic to a global variable to provide access to the characteristic value
     */
    template < typename T, T* Ptr >
    class bind_characteristic_value
    {
    public:
        /**
         * @cond HIDDEN_SYMBOLS
         * use a new type to mixin the options given to characteristic
         */
        template < typename ... Options >
        class value_impl
        {
        public:
            static constexpr bool has_read_access  = !details::has_option< no_read_access, Options... >::value;
            static constexpr bool has_write_access = !std::is_const< T >::value && !details::has_option< no_write_access, Options... >::value;
            static constexpr bool has_notifcation  = details::has_option< notify, Options... >::value;

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                if ( args.type == details::attribute_access_type::read )
                {
                    return characteristic_value_read_access( args, std::integral_constant< bool, has_read_access >() );
                }
                else if ( args.type == details::attribute_access_type::write )
                {
                    return characteristic_value_write_access( args, std::integral_constant< bool, has_write_access >() );
                }

                return details::attribute_access_result::write_not_permitted;
            }

            static bool is_this( const void* value )
            {
                return value == Ptr;
            }

        private:
            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments& args, const std::true_type& )
            {
                if ( args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_offset;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) - args.buffer_offset );
                const std::uint8_t* const ptr = static_cast< const std::uint8_t* >( static_cast< const void* >( Ptr ) ) + args.buffer_offset;

                std::copy( ptr, ptr + args.buffer_size, args.buffer );

                return args.buffer_size == sizeof( T ) - args.buffer_offset
                    ? details::attribute_access_result::success
                    : details::attribute_access_result::read_truncated;
            }

            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& )
            {
                return details::attribute_access_result::read_not_permitted;
            }

            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments& args, const std::true_type& )
            {
                if ( args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_offset;

                if ( args.buffer_size + args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::write_overflow;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) - args.buffer_offset );

                std::uint8_t* const ptr = static_cast< std::uint8_t* >( static_cast< void* >( Ptr ) );
                std::copy( args.buffer, args.buffer + args.buffer_size, ptr + args.buffer_offset );

                return details::attribute_access_result::success;
            }

            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& )
            {
                return details::attribute_access_result::write_not_permitted;
            }

        };

        struct meta_type : details::characteristic_value_meta_type, details::characteristic_value_declaration_parameter {};
        /** @endcond */
    };

    /**
     * @brief provides a characteristic with a fixed, read-only value
     *
     * Implements a numeric, readable litte endian encoded integer value.
     */
    template < class T, T Value >
    struct fixed_value {
        /**
         * @cond HIDDEN_SYMBOLS
         */
        template < typename ... Options >
        class value_impl
        {
        public:
            static constexpr bool has_read_access  = !details::has_option< no_read_access, Options... >::value;
            static constexpr bool has_write_access = false;
            static constexpr bool has_notifcation  = details::has_option< notify, Options... >::value;

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                if ( !has_read_access )
                    return details::attribute_access_result::read_not_permitted;

                if ( args.type != details::attribute_access_type::read )
                    return details::attribute_access_result::write_not_permitted;

                if ( args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_offset;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) - args.buffer_offset );

                // copy data
                std::uint8_t* output = args.buffer;
                for ( int i = args.buffer_offset; i != args.buffer_offset + args.buffer_size; ++i, ++output )
                    *output = ( Value >> ( 8 * i ) ) & 0xff;

                return args.buffer_size == sizeof( T ) - args.buffer_offset
                    ? details::attribute_access_result::success
                    : details::attribute_access_result::read_truncated;
            }

            static bool is_this( const void* value )
            {
                return false;
            }
        };

        struct meta_type : details::characteristic_value_meta_type, details::characteristic_value_declaration_parameter {};
        /** @endcond */
    };

    /**
     * @brief fixed size 8 bit unsigned int characteristic value
     * @sa fixed_value
     */
    template < std::uint8_t Value >
    using fixed_uint8_value = fixed_value< std::uint8_t, Value >;

    /**
     * @brief fixed size 16 bit unsigned int characteristic value
     * @sa fixed_value
     */
    template < std::uint16_t Value >
    using fixed_uint16_value = fixed_value< std::uint16_t, Value >;


    /**
     * @brief fixed size 32 bit unsigned int characteristic value
     * @sa fixed_value
     */
    template < std::uint32_t Value >
    using fixed_uint32_value = fixed_value< std::uint32_t, Value >;

    template < class Text >
    struct cstring_wrapper
    {
        /**
         * @cond HIDDEN_SYMBOLS
         */
        template < typename ... Options >
        class value_impl
        {
        public:
            static constexpr bool has_read_access  = true;
            static constexpr bool has_write_access = false;
            static constexpr bool has_notifcation  = false;

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                if ( args.type != details::attribute_access_type::read )
                    return details::attribute_access_result::write_not_permitted;

                const char*       value  = Text::value();
                const std::size_t length = strlen( value );

                if ( args.buffer_offset > length )
                    return details::attribute_access_result::invalid_offset;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, length - args.buffer_offset );

                // copy data
                std::copy( value + args.buffer_offset, value + args.buffer_offset + args.buffer_size, args.buffer );

                return args.buffer_size == length - args.buffer_offset
                    ? details::attribute_access_result::success
                    : details::attribute_access_result::read_truncated;
            }

            static bool is_this( const void* value )
            {
                return false;
            }

        };

        struct meta_type : details::characteristic_value_meta_type, details::characteristic_value_declaration_parameter {};
        /** @endcond */
   };
}

#endif
