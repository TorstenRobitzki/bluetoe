#ifndef BLUETOE_CHARACTERISTIC_VALUE_HPP
#define BLUETOE_CHARACTERISTIC_VALUE_HPP

#include <bluetoe/meta_tools.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/meta_types.hpp>
#include <type_traits>
#include <climits>

namespace bluetoe {

    namespace details {
        // type defines the value of a characteristic
        struct characteristic_value_meta_type {};
        // type is a valid parameter to a characteristic
        struct characteristic_parameter_meta_type {};
        // type is a characteristic read handler
        struct characteristic_value_read_handler_meta_type {};
        // type is a characteristic write handler
        struct characteristic_value_write_handler_meta_type {};

        struct characteristic_value_declaration_parameter {};
        struct client_characteristic_configuration_parameter {};
        struct characteristic_subscription_call_back_meta_type {};
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
            bluetoe::no_read_access,
            bluetoe::notify
        >
     * @endcode
     * @sa characteristic
     */
    struct no_read_access {
        /** @cond HIDDEN_SYMBOLS */
        using meta_type = details::valid_characteristic_option_meta_type;
        /** @endcond */
    };

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
    struct no_write_access {
        /** @cond HIDDEN_SYMBOLS */
        using meta_type = details::valid_characteristic_option_meta_type;
        /** @endcond */
    };

    /**
     * @brief adds the ability to notify this characteristic.
     *
     * When a characteristic gets notified, the current value of the characteristic will be send to all
     * connected clients that have subscribed for notifications. To send a notification, server::notify() have
     * to be called. The server will store that there is a pending need to send out a notification and will do
     * so, as soon as there is a free link layer PDU that can be used to transport the notification. The server
     * will use what ever mean is implemented to read the characteristic, to get the characteristic value.
     *
     * @sa server::notify
     * @sa higher_outgoing_priority
     * @sa lower_outgoing_priority
     * @sa indicate
     */
    struct notify {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::client_characteristic_configuration_parameter,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief adds the ability to indicate this characteristic.
     *
     * When a characteristic gets notified, the current value of the characteristic will be send to all
     * connected clients that have subscribed for indications.
     * The difference to notify is, that indication needs to be confirmed by the GATT client.
     *
     * @sa server::indicate
     * @sa higher_outgoing_priority
     * @sa lower_outgoing_priority
     * @sa notify
     */
    struct indicate {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::client_characteristic_configuration_parameter,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief queues a notification of a characteristic as soon, as it was configured for notification.
     *
     * This requires, that the characteristic was configured for notifications.
     *
     * @sa characteristic
     * @sa notify
     */
    struct notify_on_subscription {
        /** @cond HIDDEN_SYMBOLS */
        template < typename UUID, typename Server >
        static void on_subscription( std::uint16_t flags, Server& srv )
        {
            if ( flags & details::client_characteristic_configuration_notification_enabled )
                srv.template notify< UUID >();
        }

        struct meta_type :
            details::characteristic_subscription_call_back_meta_type,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief queues a indication of a characteristic as soon, as it was configured for indication.
     *
     * This requires, that the characteristic was configured for indications.
     *
     * @sa characteristic
     * @sa indicate
     */
    struct indicate_on_subscription {
        /** @cond HIDDEN_SYMBOLS */
        template < typename UUID, typename Server >
        static void on_subscription( std::uint16_t flags, Server& srv )
        {
            if ( flags & details::client_characteristic_configuration_indication_enabled )
                srv.template indicate< UUID >();
        }

        struct meta_type :
            details::characteristic_subscription_call_back_meta_type,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    struct default_on_characteristic_subscription {
        template < typename UUID, typename Server >
        static void on_subscription( std::uint16_t, Server& ) {}

        struct meta_type :
            details::characteristic_subscription_call_back_meta_type,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
    };
    /** @endcond */

    /**
     * @brief sets the Write Without Response Characteristic Property bit.
     *
     * By adding this bit to the Characteristic Properties, a client knows that it can use the
     * Write Without Response sub-procedure. In this case, a client can write to a characteristic
     * and will get no response from the GATT server. Don't use this property if there is any
     * need to respond with an error to a write attempt.
     */
    struct write_without_response {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::client_characteristic_configuration_parameter,
            details::characteristic_parameter_meta_type,
            details::valid_characteristic_option_meta_type {};
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
            static constexpr bool has_write_without_response = details::has_option< write_without_response, Options... >::value;
            static constexpr bool has_notification = details::has_option< notify, Options... >::value;
            static constexpr bool has_indication   = details::has_option< indicate, Options... >::value;

            template < class Server, std::size_t ClientCharacteristicIndex, bool RequiresEncryption >
            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t )
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

            /*
             * Used to find this characteristic for notification
             */
            static constexpr bool is_this( const void* value )
            {
                return value == Ptr;
            }

        private:
            static constexpr details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments& args, const std::true_type& )
            {
                return details::attribute_value_read_access( args, static_cast< const std::uint8_t* >( static_cast< const void* >( Ptr ) ), sizeof( T ) );
            }

            static constexpr details::attribute_access_result characteristic_value_read_access( details::attribute_access_arguments&, const std::false_type& )
            {
                return details::attribute_access_result::read_not_permitted;
            }

            static details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments& args, const std::true_type& )
            {
                if ( args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_offset;

                if ( args.buffer_size + args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_attribute_value_length;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) - args.buffer_offset );

                std::uint8_t* const ptr = static_cast< std::uint8_t* >( static_cast< void* >( Ptr ) );
                std::copy( args.buffer, args.buffer + args.buffer_size, ptr + args.buffer_offset );

                return details::attribute_access_result::success;
            }

            static constexpr details::attribute_access_result characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& )
            {
                return details::attribute_access_result::write_not_permitted;
            }

        };

        struct meta_type :
            details::characteristic_value_meta_type,
            details::characteristic_value_declaration_parameter,
            details::valid_characteristic_option_meta_type {};
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
            static constexpr bool has_write_without_response = false;
            static constexpr bool has_notification = details::has_option< notify, Options... >::value;
            static constexpr bool has_indication   = details::has_option< indicate, Options... >::value;

            template < class Server, std::size_t ClientCharacteristicIndex, bool RequiresEncryption  >
            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t )
            {
                if ( RequiresEncryption && !args.connection_security.is_encrypted )
                    return details::attribute_access_result::insufficient_authentication;

                if ( !has_read_access )
                    return details::attribute_access_result::read_not_permitted;

                if ( args.type != details::attribute_access_type::read )
                    return details::attribute_access_result::write_not_permitted;

                if ( args.buffer_offset > sizeof( T ) )
                    return details::attribute_access_result::invalid_offset;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) - args.buffer_offset );

                // copy data
                std::uint8_t* output = args.buffer;
                for ( auto i = args.buffer_offset; i != args.buffer_offset + args.buffer_size; ++i, ++output )
                    *output = ( Value >> ( 8 * i ) ) & 0xff;

                return details::attribute_access_result::success;
            }

            static constexpr bool is_this( const void* )
            {
                return false;
            }
        };

        struct meta_type :
            details::characteristic_value_meta_type,
            details::characteristic_value_declaration_parameter,
            details::valid_characteristic_option_meta_type {};
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

    /**
     * @brief a constant string characteristic value
     *
     * The type Text have to have a member value() that returns a const char*.
     */
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
            static constexpr bool has_write_without_response = false;
            static constexpr bool has_notification = false;
            static constexpr bool has_indication   = false;

            template < class Server, std::size_t ClientCharacteristicIndex, bool RequiresEncryption  >
            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments& args, std::uint16_t )
            {
                if ( args.type != details::attribute_access_type::read )
                    return details::attribute_access_result::write_not_permitted;

                const char* value  = Text::value();
                std::size_t length = 0;
                for ( const char* v = value; *v; ++v, ++length )
                    ;

                if ( args.buffer_offset > length )
                    return details::attribute_access_result::invalid_offset;

                args.buffer_size = std::min< std::size_t >( args.buffer_size, length - args.buffer_offset );

                // copy data
                std::copy( value + args.buffer_offset, value + args.buffer_offset + args.buffer_size, args.buffer );

                return details::attribute_access_result::success;
            }

            static constexpr bool is_this( const void* )
            {
                return false;
            }

        };

        struct meta_type :
            details::characteristic_value_meta_type,
            details::characteristic_value_declaration_parameter,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief a constant string characteristic with the value Name
     */
    template < const char* const Name >
    struct cstring_value : cstring_wrapper< cstring_value< Name > >
    {
        /** @cond HIDDEN_SYMBOLS */
        static constexpr char const * name = Name;

        static constexpr char const * value()
        {
            return name;
        }
        /** @endcond */
    };

    namespace details {
        template < class T >
        struct invoke_read_handler {
            template < class Server >
            static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* server )
            {
                return T::template call_read_handler< Server >( offset, read_size, out_buffer, out_size, server );
            }
        };

        template <>
        struct invoke_read_handler< no_such_type > {
            template < class Server >
            static constexpr std::uint8_t call_read_handler( std::size_t, std::size_t, std::uint8_t*, std::size_t&, void* )
            {
                return error_codes::read_not_permitted;
            }
        };

        template < class T >
        struct invoke_write_handler {
            template < class Server, std::size_t ClientCharacteristicIndex >
            static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& config, void* server )
            {
                return T::template call_write_handler< Server, ClientCharacteristicIndex >( offset, write_size, value, config, server );
            }
        };

        template <>
        struct invoke_write_handler< no_such_type > {
            template < class Server, std::size_t ClientCharacteristicIndex >
            static constexpr std::uint8_t call_write_handler( std::size_t, std::size_t, const std::uint8_t*, const details::client_characteristic_configuration&, void* )
            {
                return error_codes::write_not_permitted;
            }
        };

        struct value_handler_base {

            template < typename ... Options >
            class value_impl
            {
            public:
                using read_handler_type = typename find_by_meta_type< characteristic_value_read_handler_meta_type, Options... >::type;
                using write_handler_type = typename find_by_meta_type< characteristic_value_write_handler_meta_type, Options... >::type;
                static constexpr bool no_read          = has_option< no_read_access, Options... >::value;
                static constexpr bool no_write         = has_option< no_write_access, Options... >::value;

                static constexpr bool has_read_handler = !std::is_same< read_handler_type, no_such_type >::value;
                static constexpr bool has_read_access  = has_read_handler && !no_read;
                static constexpr bool has_write_access = !std::is_same< write_handler_type, no_such_type >::value;
                static constexpr bool has_write_without_response = details::has_option< write_without_response, Options... >::value;
                static constexpr bool has_notification = has_option< notify, Options... >::value;
                static constexpr bool has_indication   = has_option< indicate, Options... >::value;

                static_assert( !( no_write && ( has_write_access || has_write_without_response ) ), "There is no point in providing a write_handler and disabling write by using no_write_access" );

                static_assert( !has_notification || ( has_notification && has_read_handler ), "When enabling notification, the characteristic needs to have a read_handler" );
                static_assert( !has_indication || ( has_indication && has_read_handler ), "When enabling indications, the characteristic needs to have a read_handler" );

                static_assert( has_read_access || has_write_access || has_notification || has_indication, "Ups!");

                template < class Server, std::size_t ClientCharacteristicIndex, bool RequiresEncryption >
                static attribute_access_result characteristic_value_access( attribute_access_arguments& args, std::uint16_t /* attribute_handle */ )
                {
                    if ( args.type == attribute_access_type::read )
                    {
                        return static_cast< attribute_access_result >(
                            invoke_read_handler< read_handler_type >::template call_read_handler< Server >( args.buffer_offset, args.buffer_size, args.buffer, args.buffer_size, args.server ) );
                    }
                    else if ( args.type == attribute_access_type::write )
                    {
                        return static_cast< attribute_access_result >(
                            invoke_write_handler< write_handler_type >::template call_write_handler< Server, ClientCharacteristicIndex >( args.buffer_offset, args.buffer_size, args.buffer, args.client_config, args.server ) );
                    }
                    else
                    {
                        return attribute_access_result::request_not_supported;
                    }
                }
            };

            struct meta_type :
                characteristic_value_meta_type,
                characteristic_value_declaration_parameter,
                details::valid_characteristic_option_meta_type {};
        };

        template < typename T >
        inline std::pair< std::uint8_t, T > deserialize( std::size_t write_size, const std::uint8_t* value )
        {
            static_assert( CHAR_BIT == 8, "needs porting!" );

            if ( write_size != sizeof( T ) )
                return std::pair< std::uint8_t, T >{ error_codes::invalid_attribute_value_length, 0 };

            T result = 0;

            for ( const std::uint8_t* v = value + sizeof( T ); v != value; --v )
            {
                result = ( result << 8 ) | *( v - 1 );
            }

            return std::pair< std::uint8_t, T >{ error_codes::success, result };
        }

        template <>
        inline std::pair< std::uint8_t, bool > deserialize( std::size_t write_size, const std::uint8_t* value )
        {
            if ( write_size != 1 )
                return std::make_pair( error_codes::invalid_attribute_value_length, false );

            if ( *value == 0 )
            {
                return std::pair< std::uint8_t, bool >{ error_codes::success, false };
            }
            else if ( *value == 1 )
            {
                return std::pair< std::uint8_t, bool >{ error_codes::success, true };
            }

            return std::pair< std::uint8_t, bool >{ error_codes::out_of_range, false };
        }
    }

    /**
     * @brief binds a free function as a read handler for the given characteristic
     *
     * The handler can be used to handle blobs. The handler can be used with a
     * bluetoe::no_read_access parameter in conjunction with bluetoe::indicate or bluetoe::notify to provide the
     * data to a notification or indication without the charactertic beeing able to be read.
     * If only a read handler is passed to the bluetoe::characteristic, the characteristic will be read only.
     * If the characteristic value will always be smaller than 21 octets, using a bluetoe::free_read_handler will save
     * you from coping with an offset.
     *
     * @tparam F pointer to function to handle a read request
     * @param offset offset in octets into the characteristic beeing read. If the offset is larger than the characteristic,
     *        the handler should return bluetoe::error_codes::invalid_offset
     * @param read_size the maximum size, a client is able to read from the characteristic. This is the maximum, that can be
     *                  copied to the out_buffer, without overflowing it. Reading just a part of the characteristic (because
     *                  of the limited out_buffer size) is fine and not an error.
     * @param out_buffer Output buffer to copy the data, that is read.
     * @param out_size The actual number of octets copied to out_buffer. out_size must be smaller or equal to read_size.
     *
     * @retval If the characteristic value could be read successfully (even when the out_buffer was to small, to read the whole value),
     *         the function should return luetoe::error_codes::success. In all other cases, Bluetoe will generate an error response to
     *         a read request and directly use the return value as error code. For usefull error codes, have a look at
     *         bluetoe::error_codes::error_codes.
     *
     * @sa characteristic
     * @sa free_read_handler
     * @sa free_write_blob_handler
     * @sa free_raw_write_handler
     * @sa bluetoe::error_codes::error_codes
     */
    template < std::uint8_t (*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct free_read_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return F( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    /**
     * @brief binds a free function as a read handler for the given characteristic
     *
     * This handler is functional similar to free_read_blob_handler, but can not be used for handling requests to
     * large characteristics. To be on the safe side, this type of handler should not be used for characteristic values
     * larger than 20 octets.
     * The handler can be used with a
     * bluetoe::no_read_access parameter in conjunction with bluetoe::indicate or bluetoe::notify to provide the
     * data to a notification or indication without the charactertic beeing able to be read.
     * If only a read handler is passed to the bluetoe::characteristic, the characteristic will be read only.
     *
     * @tparam F pointer to function to handle a read request
     * @param read_size the maximum size, a client is able to read from the characteristic. This is the maximum, that can be
     *                  copied to the out_buffer, without overflowing it. Reading just a part of the characteristic (because
     *                  of the limited out_buffer size) is fine and not an error.
     * @param out_buffer Output buffer to copy the data, that is read.
     * @param out_size The actual number of octets copied to out_buffer. out_size must be smaller or equal to read_size.
     *
     * @retval If the characteristic value could be read successfully (even when the out_buffer was to small, to read the whole value),
     *         the function should return luetoe::error_codes::success. In all other cases, Bluetoe will generate an error response to
     *         a read request and directly use the return value as error code. For usefull error codes, have a look at
     *         bluetoe::error_codes::error_codes.
     *
     * @sa characteristic
     * @sa free_read_blob_handler
     * @sa free_write_blob_handler
     * @sa free_raw_write_handler
     * @sa bluetoe::error_codes::error_codes
     */
    template < std::uint8_t (*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct free_read_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return offset == 0
                ? F( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    /**
     * @brief binds a free function as a write handler for the given characteristic
     *
     * The handler can be used to handle blobs.
     * If only a write handler is passed to the bluetoe::characteristic, the characteristic will be write only.
     * If the characteristic value will always be smaller than 21 octets, using a bluetoe::free_raw_write_handler will save
     * you from coping with an offset.
     *
     * @tparam F pointer to function to handle a write request
     *
     * @param offset offset in octets into the characteristic beeing written. If the offset is larger than the characteristic,
     *        the handler should return bluetoe::error_codes::invalid_offset
     * @param write_size The size of the data that should be written to the characteristic value. If the size given is to
     *            large, the handler should return bluetoe::error_codes::invalid_attribute_value_length.
     * @param value Input buffer containing the desired characteristic value.
     *
     * @retval If the characteristic value could be read successfully (even when the out_buffer was to small, to read the whole value),
     *         the function should return luetoe::error_codes::success. In all other cases, Bluetoe will generate an error response to
     *         a read request and directly use the return value as error code. For usefull error codes, have a look at
     *         bluetoe::error_codes::error_codes.
     *
     * @sa characteristic
     * @sa free_read_blob_handler
     * @sa free_read_handler
     * @sa free_raw_write_handler
     * @sa bluetoe::error_codes::error_codes
     */
    template < std::uint8_t (*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) >
    struct free_write_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return F( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    /**
     * @brief binds a free function as a write handler for the given characteristic
     *
     * If only a write handler is passed to the bluetoe::characteristic, the characteristic will be write only.
     * To handle characteristic value sizes larger than 20 octets, a free_write_blob_handler should be used.
     *
     * @tparam F pointer to function to handle a write request
     *
     * @param write_size The size of the data that should be written to the characteristic value. If the size given is to
     *            large, the handler should return bluetoe::error_codes::invalid_attribute_value_length.
     * @param value Input buffer containing the desired characteristic value.
     *
     * @retval If the characteristic value could be read successfully (even when the out_buffer was to small, to read the whole value),
     *         the function should return luetoe::error_codes::success. In all other cases, Bluetoe will generate an error response to
     *         a read request and directly use the return value as error code. For usefull error codes, have a look at
     *         bluetoe::error_codes::error_codes.
     *
     * @sa characteristic
     * @sa free_read_blob_handler
     * @sa free_read_handler
     * @sa free_write_blob_handler
     * @sa bluetoe::error_codes::error_codes
     */
    template < std::uint8_t (*F)( std::size_t write_size, const std::uint8_t* value ) >
    struct free_raw_write_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return offset == 0
                ? F( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct read_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return (O.*F)( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const >
    struct read_blob_handler_c : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return (O.*F)( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) volatile >
    struct read_blob_handler_v : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return (O.*F)( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const volatile >
    struct read_blob_handler_cv : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return (O.*F)( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, Obj& O, std::uint8_t (Obj::*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct read_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return offset == 0
                ? (O.*F)( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const Obj& O, std::uint8_t (Obj::*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const >
    struct read_handler_c : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return offset == 0
                ? (O.*F)( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) volatile >
    struct read_handler_v : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return offset == 0
                ? (O.*F)( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) const volatile >
    struct read_handler_cv : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static constexpr std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* )
        {
            return offset == 0
                ? (O.*F)( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) >
    struct write_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return (O.*F)( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) const >
    struct write_blob_handler_c : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return (O.*F)( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) volatile >
    struct write_blob_handler_v : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return (O.*F)( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) const volatile >
    struct write_blob_handler_cv : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return (O.*F)( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, Obj& O, std::uint8_t (Obj::*F)( std::size_t write_size, const std::uint8_t* value ) >
    struct write_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return offset == 0
                ? (O.*F)( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const Obj& O, std::uint8_t (Obj::*F)( std::size_t write_size, const std::uint8_t* value ) const >
    struct write_handler_c : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return offset == 0
                ? (O.*F)( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t write_size, const std::uint8_t* value ) volatile >
    struct write_handler_v : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return offset == 0
                ? (O.*F)( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Obj, const volatile Obj& O, std::uint8_t (Obj::*F)( std::size_t write_size, const std::uint8_t* value ) const volatile >
    struct write_handler_cv : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static constexpr std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            return offset == 0
                ? (O.*F)( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::uint8_t (Mixin::*F)( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct mixin_read_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* server )
        {
            assert( server );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_read_handler into the server." );

            Mixin& mixin = static_cast< Mixin& >( *static_cast< Server* >( server ) );

            return offset == 0
                ? (mixin.*F)( read_size, out_buffer, out_size )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );

        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::uint8_t (Mixin::*F)( std::size_t write_size, const std::uint8_t* value ) >
    struct mixin_write_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* server )
        {
            assert( server );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_write_handler into the server." );

            Mixin& mixin = static_cast< Mixin& >( *static_cast< Server* >( server ) );

            return offset == 0
                ? (mixin.*F)( write_size, value )
                : static_cast< std::uint8_t >( error_codes::attribute_not_long );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::uint8_t (Mixin::*F)( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size ) >
    struct mixin_read_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server >
        static std::uint8_t call_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size, void* server )
        {
            assert( server );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_read_blob_handler into the server." );

            Mixin& mixin = static_cast< Mixin& >( *static_cast< Server* >( server ) );
            return (mixin.*F)( offset, read_size, out_buffer, out_size );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_read_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::uint8_t (Mixin::*F)( std::size_t offset, std::size_t write_size, const std::uint8_t* value ) >
    struct mixin_write_blob_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* server )
        {
            assert( server );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_write_blob_handler into the server." );

            Mixin& mixin = static_cast< Mixin& >( *static_cast< Server* >( server ) );
            return (mixin.*F)( offset, write_size, value );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::pair< std::uint8_t, bool > (Mixin::*F)( std::size_t write_size, const std::uint8_t* value ), typename IndicationUUID >
    struct mixin_write_indication_control_point_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& config, void* server_ptr )
        {
            assert( server_ptr );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_write_handler into the server." );

            if ( offset != 0 )
                return static_cast< std::uint8_t >( error_codes::attribute_not_long );

            // we have a void pointer, the type of the server and the server is derived from the mixin
            Server& server = *static_cast< Server* >( server_ptr );
            Mixin&  mixin  = static_cast< Mixin& >( server );

            // as this is a indication control point, this thingy must be configured for indications
            if ( !server.template configured_for_indications< IndicationUUID >( config ) )
                return error_codes::cccd_improperly_configured;

            const std::pair< std::uint8_t, bool > result = (mixin.*F)( write_size, value );

            if ( result.second )
                server.template indicate< IndicationUUID >();

            return static_cast< std::uint8_t >( result.first );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    template < class Mixin, std::pair< std::uint8_t, bool > (Mixin::*F)( std::size_t write_size, const std::uint8_t* value ), typename NotificationUUID >
    struct mixin_write_notification_control_point_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t  >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& config, void* server_ptr )
        {
            assert( server_ptr );
            static_assert( std::is_convertible< Server*, Mixin* >::value, "Use bluetoe::mixin<> to mixin an instance of the mixin_write_handler into the server." );

            if ( offset != 0 )
                return static_cast< std::uint8_t >( error_codes::attribute_not_long );

            // we have a void pointer, the type of the server and the server is derived from the mixin
            Server& server = *static_cast< Server* >( server_ptr );
            Mixin&  mixin  = static_cast< Mixin& >( server );

            // as this is a indication control point, this thingy must be configured for indications
            if ( !server.template configured_for_notifications< NotificationUUID >( config ) )
                return error_codes::cccd_improperly_configured;

            const std::pair< std::uint8_t, bool > result = (mixin.*F)( write_size, value );

            if ( result.second )
                server.template notify< NotificationUUID >();

            return static_cast< std::uint8_t >( result.first );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    /**
     * @brief binds a free function as a write handler for the given characteristic
     *
     * This handler handle write to characteristics with a specific size. The size if derived from the first
     * template paramter. A std::uint32_t for example would result in a characteristic with a size of 4
     * octets.
     * If only a write handler is passed to the bluetoe::characteristic, the characteristic will be write only.
     *
     * @tparam T type of characteristic. Possible values are bool, std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t and there
     *          signed counterpart.
     *
     * @tparam F pointer to function to handle a write request.
     *
     * @retval If the characteristic value could be written successfully the function should return luetoe::error_codes::success.
     *         For usefull error codes, have a look at bluetoe::error_codes::error_codes.
     *
     * @sa characteristic
     * @sa free_read_blob_handler
     * @sa free_read_handler
     * @sa free_write_blob_handler
     * @sa bluetoe::error_codes::error_codes
     */
    template < typename T, std::uint8_t (*F)( T ) >
    struct free_write_handler : details::value_handler_base
    {
        /** @cond HIDDEN_SYMBOLS */
        template < class Server, std::size_t ClientCharacteristicIndex >
        static std::uint8_t call_write_handler( std::size_t offset, std::size_t write_size, const std::uint8_t* value, const details::client_characteristic_configuration& , void* )
        {
            if ( offset != 0 )
                return static_cast< std::uint8_t >( error_codes::attribute_not_long );

            const std::pair< std::uint8_t, T > deserialized_value = details::deserialize< T >( write_size, value );

            if ( deserialized_value.first != error_codes::success )
                return deserialized_value.first;

            return (*F)( deserialized_value.second );
        }

        struct meta_type : details::value_handler_base::meta_type, details::characteristic_value_write_handler_meta_type {};
        /** @endcond */
    };

    /**
     * @example read_write_handler_example.cpp
     * Example, showing all possible alternatives to bind functions as read or write handler to a bluetooth characteristic.
     */

}

#endif
