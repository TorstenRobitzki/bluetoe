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

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments&, std::uint16_t attribute_handle );
            static bool is_this( const void* value );
        private:
            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::true_type& );
            static details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& );
            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::true_type& );
            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& );
        };

        struct meta_type : details::characteristic_value_meta_type, details::characteristic_value_declaration_parameter {};
        /** @endcond */
    };

    // implementation
    /** @cond HIDDEN_SYMBOLS */
    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
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

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_read_access( details::attribute_access_arguments& args, const std::true_type& )
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

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& )
    {
        return details::attribute_access_result::read_not_permitted;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_write_access( details::attribute_access_arguments& args, const std::true_type& )
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

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& )
    {
        return details::attribute_access_result::write_not_permitted;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    bool bind_characteristic_value< T, Ptr >::value_impl< Options... >::is_this( const void* value )
    {
        return value == Ptr;
    }

    /** @endcond */
}

#endif
