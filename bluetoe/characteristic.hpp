#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/bits.hpp>

#include <cstddef>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <type_traits>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type {};
        struct characteristic_uuid_meta_type {};
        struct characteristic_value_meta_type {};
        struct characteristic_parameter_meta_type {};

        struct characteristic_user_description_parameter {};
        struct client_characteristic_configuration_parameter {};

        template < typename ... Options >
        struct generate_attributes;

        template < typename Characteristic >
        struct sum_by_attributes;

        template < typename Characteristic >
        struct sum_by_client_configs;


    }

    /**
     * @brief a 128-Bit UUID used to identify a characteristic.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::characteristic_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     * @sa characteristic
     */
    template <
        std::uint64_t A,
        std::uint64_t B,
        std::uint64_t C,
        std::uint64_t D,
        std::uint64_t E >
    struct characteristic_uuid : details::uuid< A, B, C, D, E >
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::characteristic_uuid_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief a 16-Bit UUID used to identify a characteristic.
     * @sa characteristic
     */
    template <
        std::uint64_t UUID >
    struct characteristic_uuid16 : details::uuid16< UUID >
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::characteristic_uuid_meta_type meta_type;
        static constexpr bool is_128bit = false;
        /** @endcond */
    };

    /**
     * @brief A characteristic is an attribute that is accessable by a GATT client.
     *
     * A characteristics type (not it terms of C++ data type) is defined by a UUID. Characteristics with the
     * same UUID should represent attributes with the same type / characteristic. Usually a GATT client referes
     * an attribute by its UUID.
     *
     * To define the UUID of a characteristic add an instanciated characteristic_uuid<> or characteristic_uuid16<>
     * type as option to the characteristic.
     *
     * For example to define a user defined type to a characteristic:
     * @code
     * typedef bluetoe::characteristic<
     *    bluetue::characteristic_uuid16< 0xF0E6EBE6, 0x3749, 0x41A6, 0xB190, 0x591B262AC20A >
     * > speed_over_ground_characteristic;
     * @endcode
     *
     * If no UUID is given, bluetoe derives a 128bit UUID from the UUID of the service in which this characteric
     * is placed in. The first characteristic gets an UUID, where the last bytes are xored with 1, the second
     * characteristic will get an UUID, where the last bytes are xored with 2 and so on...
     *
     * The following examples show a service with the UUID 48B7F909-B039-4550-97AF-336228C45CED and two characteristics,
     * without an explicit UUID. In this case, the first characteristic becomes the UUID 48B7F909-B039-4550-97AF-336228C45CEC (0x336228C45CED ^ 1 )
     * and the second UUID becomes the UUID 48B7F909-B039-4550-97AF-336228C45CEF (0x336228C45CED ^ 2 )
     *
     * @code
     * typedef bluetoe::service<
     *     bluetoe::service_uuid< 0x48B7F909, 0xB039, 0x4550, 0x97AF, 0x336228C45CED >,
     *     bluetoe::characteristic<
     *         bluetoe::bind_characteristic_value< std::uint32_t, &presure >,
     *         bluetoe::no_write_access,
     *         bluetoe::notify
     *     >,
     *     bluetoe::characteristic<
     *         bluetoe::bind_characteristic_value< std::uint32_t, &alarm_threshold >
     *     >
     * > presure_service;
     * @endcode
     *
     * @sa characteristic_uuid
     * @sa characteristic_uuid16
     * @sa no_read_access
     * @sa no_write_access
     * @sa notify
     * @sa bind_characteristic_value
     * @sa characteristic_name
     */
    template < typename ... Options >
    class characteristic
    {
    public:
        /** @cond HIDDEN_SYMBOLS */

        typedef typename details::generate_attributes< Options... > characteristic_descriptor_declarations;

        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes     = 2 + characteristic_descriptor_declarations::number_of_attributes;
        static constexpr std::size_t number_of_client_configs = characteristic_descriptor_declarations::number_of_client_configs;
        static constexpr std::size_t number_of_server_configs = characteristic_descriptor_declarations::number_of_server_configs;

        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        typedef details::characteristic_meta_type meta_type;

        /**
         * @brief gives access to the all attributes of the characteristic
         */
        template < std::size_t ClientCharacteristicIndex >
        static details::attribute attribute_at( std::size_t index );

        /**
         * returns a correctly filled notification_data() object, if this characteristc was configured for notification or indication
         * and the given value identifies the characteristic value. If not found find_notification_data( value ).valid() is false.
         */
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static details::notification_data find_notification_data( const void* value );

        /** @endcond */
    private:
        typedef typename details::find_by_meta_type< details::characteristic_value_meta_type, Options... >::type    base_value_type;

        static_assert( !std::is_same< base_value_type, details::no_such_type >::value,
            "please make sure, that every characteristic defines some kind of value (bind_characteristic_value<> for example)" );

        typedef typename base_value_type::template value_impl< Options... >                                         value_type;

        static details::attribute_access_result char_declaration_access( details::attribute_access_arguments&, std::uint16_t attribute_handle );
    };

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

        typedef details::characteristic_value_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief adds a name to characteristic
     *
     * Adds a "Characteristic User Description" to the characteristic. So a GATT client can read the name of the characteristic.
     *
     * Example
     * @code
    char simple_value = 0;
    const char name[] = "Die ist der Name";

    typedef bluetoe::characteristic<
        bluetoe::characteristic_name< name >,
        bluetoe::characteristic_uuid16< 0x0815 >,
        bluetoe::bind_characteristic_value< char, &simple_value >
    > named_char;
     * @endcode
     */
    template < const char* const >
    struct characteristic_name
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::characteristic_parameter_meta_type,
            details::characteristic_user_description_parameter {};
        /** @endcond */
    };

    // implementation
    /** @cond HIDDEN_SYMBOLS */

    template < typename ... Options >
    template < std::size_t ClientCharacteristicIndex >
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        static const details::attribute attributes[] = {
            { bits( details::gatt_uuids::characteristic ), &char_declaration_access },
            {
                uuid::is_128bit
                    ? bits( details::gatt_uuids::internal_128bit_uuid )
                    : uuid::as_16bit(),
                &value_type::characteristic_value_access
            }
        };

        return index < sizeof( attributes ) / sizeof( attributes[ 0 ] )
            ? attributes[ index ]
            : characteristic_descriptor_declarations::template attribute_at< ClientCharacteristicIndex >( index - 2 );
    }

    template < typename ... Options >
    template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
    details::notification_data characteristic< Options... >::find_notification_data( const void* value )
    {
        static_assert( FirstAttributesHandle != 0, "FirstAttributesHandle is invalid" );

        if ( !value_type::is_this( value ) || !value_type::has_notifcation )
            return details::notification_data();

        return
            details::notification_data(
                FirstAttributesHandle + 1,
                details::attribute {
                    uuid::is_128bit
                        ? bits( details::gatt_uuids::internal_128bit_uuid )
                        : uuid::as_16bit(),
                    &value_type::characteristic_value_access
                },
                ClientCharacteristicIndex
            );
    }

    template < typename ... Options >
    details::attribute_access_result characteristic< Options... >::char_declaration_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
    {
        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;

        static constexpr auto uuid_offset = 3;
        static constexpr auto uuid_size = sizeof( uuid::bytes );
        static constexpr auto data_size = uuid_offset + uuid_size;

        if ( args.buffer_offset > data_size )
            return details::attribute_access_result::invalid_offset;

        if ( args.type != details::attribute_access_type::read )
            return details::attribute_access_result::write_not_permitted;

        // transform the output buffer into two pointers that point into the characteristic declaracation:
        const std::size_t begin = args.buffer_offset;
        const std::size_t end   = std::min< std::size_t >( args.buffer_offset + args.buffer_size, data_size );

        std::uint8_t* output = args.buffer;

        // header (properties and handle) if offset points into the header
        if ( begin < uuid_offset )
        {
            std::uint8_t header_buffer[ uuid_offset ];
            header_buffer[ 0 ] =
                ( value_type::has_read_access  ? bits( details::gatt_characteristic_properties::read ) : 0 ) |
                ( value_type::has_write_access ? bits( details::gatt_characteristic_properties::write ) : 0 ) |
                ( value_type::has_notifcation  ? bits( details::gatt_characteristic_properties::notify ) : 0 );

            // the Characteristic Value Declaration must follow directly behind this attribute and has, thus the next handle
            details::write_handle( header_buffer + 1, attribute_handle + 1 );

            const std::size_t end_header_bytes = std::min< std::size_t >( end, uuid_offset );
            std::copy( &header_buffer[ begin ], &header_buffer[ end_header_bytes ], output );

            output += end_header_bytes - begin;
        }

        if ( end > uuid_offset )
        {
            const std::size_t start_uuid_bytes = std::max< std::size_t >( begin, uuid_offset );
            std::copy( &uuid::bytes[ start_uuid_bytes - uuid_offset ], &uuid::bytes[ end - uuid_offset ], output );

            output += end - start_uuid_bytes;
        }

        args.buffer_size = output - args.buffer;

        return args.buffer_size == data_size - args.buffer_offset
            ? details::attribute_access_result::success
            : details::attribute_access_result::read_truncated;
    }

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

    namespace details {


        template < typename, std::size_t >
        struct generate_attribute;

        /*
         * Characteristic User Description
         */
        template < const char* const Name, std::size_t ClientCharacteristicIndex >
        struct generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex >
        {
            static const attribute attr;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                const std::size_t str_len   = std::strlen( Name );

                if ( args.buffer_offset > str_len )
                    return details::attribute_access_result::invalid_offset;

                details::attribute_access_result result = attribute_access_result::write_not_permitted;

                if ( args.type == attribute_access_type::read )
                {
                    const std::size_t read_size = std::min( args.buffer_size, str_len - args.buffer_offset );

                    std::copy( Name + args.buffer_offset, Name + args.buffer_offset + read_size, args.buffer );

                    result = str_len - args.buffer_offset > args.buffer_size
                        ? attribute_access_result::read_truncated
                        : attribute_access_result::success;

                    args.buffer_size = read_size;
                }

                return result;
            }

        };

        template < const char* const Name, std::size_t ClientCharacteristicIndex >
        const attribute generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex >::attr {
            bits( gatt_uuids::characteristic_user_description ),
            &generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex >::access
        };

        /*
         * Client Characteristic Configuration
         */
        template < typename ... Options, std::size_t ClientCharacteristicIndex >
        struct generate_attribute< std::tuple< client_characteristic_configuration_parameter, Options... >, ClientCharacteristicIndex >
        {
            static const attribute attr;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                static constexpr std::size_t flags_size = 2;
                std::uint8_t buffer[ flags_size ];

                if ( args.buffer_offset > flags_size )
                    return details::attribute_access_result::invalid_offset;

                details::attribute_access_result result = attribute_access_result::write_not_permitted;

                if ( args.type == attribute_access_type::read )
                {
                    write_16bit( &buffer[ 0 ], args.client_config.flags( ClientCharacteristicIndex ) );

                    const std::size_t read_size = std::min( args.buffer_size, flags_size - args.buffer_offset );

                    std::copy( &buffer[ args.buffer_offset ], &buffer[ args.buffer_offset + read_size ], args.buffer );

                    result = flags_size - args.buffer_offset > args.buffer_size
                        ? attribute_access_result::read_truncated
                        : attribute_access_result::success;

                    args.buffer_size = read_size;
                }
                else if ( args.type == attribute_access_type::write )
                {
                    if ( args.buffer_size + args.buffer_offset > flags_size )
                        return details::attribute_access_result::write_overflow;

                    if ( args.buffer_offset == 0 )
                        args.client_config.flags( ClientCharacteristicIndex, read_16bit( &args.buffer[ 0 ] ) );

                    result = attribute_access_result::success;
                }

                return result;
            }
        };

        template < typename ... Options, std::size_t ClientCharacteristicIndex >
        const attribute generate_attribute< std::tuple< client_characteristic_configuration_parameter, Options... >, ClientCharacteristicIndex >::attr {
            bits( gatt_uuids::client_characteristic_configuration ),
            &generate_attribute< std::tuple< client_characteristic_configuration_parameter, Options... >, ClientCharacteristicIndex >::access
        };

        template < typename, std::size_t >
        struct generate_attribute_list;

        template < std::size_t ClientCharacteristicIndex >
        struct generate_attribute_list< std::tuple<>, ClientCharacteristicIndex >
        {
            static const attribute attribute_at( std::size_t index )
            {
                assert( !"should not happen" );
            }
        };

        template < typename ... Options, std::size_t ClientCharacteristicIndex >
        struct generate_attribute_list< std::tuple< Options... >, ClientCharacteristicIndex >
        {
            static const attribute attribute_at( std::size_t index )
            {
                return attributes[ index ];
            }

            static attribute attributes[ sizeof ...(Options) ];
        };

        template < typename ... Options, std::size_t ClientCharacteristicIndex >
        attribute generate_attribute_list< std::tuple< Options... >, ClientCharacteristicIndex >::attributes[ sizeof ...(Options) ] =
        {
            generate_attribute< Options, ClientCharacteristicIndex >::attr...
        };

        template < typename Parmeters >
        struct are_client_characteristic_configuration_parameter : std::false_type {};

        template < typename ... Ts >
        struct are_client_characteristic_configuration_parameter< std::tuple< client_characteristic_configuration_parameter, Ts... > > : std::true_type {};

        template < typename ... Options >
        struct generate_attributes
        {
            // this constructs groups all Options by there meta type. Empty groups are removed from the result set.
            typedef typename group_by_meta_types_without_empty_groups<
                std::tuple< Options... >,
                // List of meta types. The order of this meta types defines the order in the attribute list.
                characteristic_user_description_parameter,
                client_characteristic_configuration_parameter
            >::type declaraction_parameters;

            enum { number_of_attributes     = std::tuple_size< declaraction_parameters >::value };
            enum { number_of_client_configs = count_if< declaraction_parameters, are_client_characteristic_configuration_parameter >::value };
            enum { number_of_server_configs = 0 };

            template < std::size_t ClientCharacteristicIndex >
            static const attribute attribute_at( std::size_t index )
            {
                return generate_attribute_list< declaraction_parameters, ClientCharacteristicIndex >::attribute_at( index );
            }
        };

        template < typename Characteristic >
        struct sum_by_attributes
        {
            enum { value = Characteristic::number_of_attributes };
        };

        template < typename Characteristic >
        struct sum_by_client_configs
        {
            enum { value = Characteristic::number_of_client_configs };
        };

        /** @endcond */
    }
}

#endif
