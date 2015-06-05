#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/characteristic_value.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/scattered_access.hpp>

#include <cstddef>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <type_traits>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type {};
        struct characteristic_uuid_meta_type {};

        struct characteristic_declaration_parameter {};
        struct characteristic_user_description_parameter {};

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
        struct meta_type : details::characteristic_uuid_meta_type, details::characteristic_value_declaration_parameter, details::characteristic_declaration_parameter {};
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
        struct meta_type : details::characteristic_uuid_meta_type, details::characteristic_value_declaration_parameter, details::characteristic_declaration_parameter {};
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
        static constexpr std::size_t number_of_attributes     = characteristic_descriptor_declarations::number_of_attributes;
        static constexpr std::size_t number_of_client_configs = characteristic_descriptor_declarations::number_of_client_configs;
        static constexpr std::size_t number_of_server_configs = characteristic_descriptor_declarations::number_of_server_configs;

        typedef details::characteristic_meta_type meta_type;

        /**
         * @brief gives access to the all attributes of the characteristic
         */
        template < std::size_t ClientCharacteristicIndex, typename ServiceUUID = void >
        static details::attribute attribute_at( std::size_t index );

        /**
         * returns a correctly filled notification_data() object, if this characteristc was configured for notification or indication
         * and the given value identifies the characteristic value. If not found find_notification_data( value ).valid() is false.
         */
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static details::notification_data find_notification_data( const void* value );

        typedef typename details::find_by_meta_type< details::characteristic_value_meta_type, Options... >::type    base_value_type;

        static_assert( !std::is_same< base_value_type, details::no_such_type >::value,
            "please make sure, that every characteristic defines some kind of value (bind_characteristic_value<> for example)" );

        typedef typename base_value_type::template value_impl< Options... >                                         value_type;

        /** @endcond */
    private:
        // the first two attributes are always the declaration, followed by the value
        static constexpr std::size_t characteristic_declaration_index = 0;
        static constexpr std::size_t characteristic_value_index       = 1;
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
    template < std::size_t ClientCharacteristicIndex, typename ServiceUUID >
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        return characteristic_descriptor_declarations::template attribute_at< ClientCharacteristicIndex, ServiceUUID >( index );
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
                attribute_at< ClientCharacteristicIndex >( characteristic_value_index ),
                ClientCharacteristicIndex
            );
    }

    namespace details {


        template < typename, std::size_t, typename ... Options >
        struct generate_attribute;

        /*
         * Characteristic declaration
         */
        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >
        {
            // the characterist value has two configurable aspects: the uuid and the value. The value is defined in the charcteristic
            typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, AttrOptions... >::type     uuid;
            typedef typename characteristic< Options... >::value_type                                                       value_type;

            /*
             * the characteristic decalarion consists of 3 parts a Properties byte, two bytes index and 2 - 16 bytes UUID
             */
            static details::attribute_access_result char_declaration_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                if ( args.type != details::attribute_access_type::read )
                    return details::attribute_access_result::write_not_permitted;

                const std::uint8_t properties[] = {
                    static_cast< std::uint8_t >(
                        ( value_type::has_read_access  ? bits( details::gatt_characteristic_properties::read ) : 0 ) |
                        ( value_type::has_write_access ? bits( details::gatt_characteristic_properties::write ) : 0 ) |
                        ( value_type::has_notifcation  ? bits( details::gatt_characteristic_properties::notify ) : 0 ) )
                };

                // the Characteristic Value Declaration must follow directly behind this attribute and has, thus the next handle
                const std::uint8_t value_handle[] = {
                    static_cast< std::uint8_t >( ( attribute_handle +1 ) & 0xff ),
                    static_cast< std::uint8_t >( ( attribute_handle +1 ) >> 8 )
                };

                static constexpr auto data_size = sizeof( properties ) + sizeof( value_handle ) + sizeof( uuid::bytes );

                if ( args.buffer_offset > data_size )
                    return details::attribute_access_result::invalid_offset;

                details::scattered_read_access( args.buffer_offset, properties, value_handle, uuid::bytes, args.buffer, args.buffer_size );

                args.buffer_size = std::min< std::size_t >( data_size - args.buffer_offset, args.buffer_size );

                return args.buffer_size == data_size - args.buffer_offset
                    ? details::attribute_access_result::success
                    : details::attribute_access_result::read_truncated;
            }

            static const attribute attr;
        };

        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >::attr {
            bits( details::gatt_uuids::characteristic ),
            &generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >::char_declaration_access
        };

        /*
         * Characteristic Value
         */
        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_value_declaration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >
        {
            // the characterist value has two configurable aspects: the uuid and the value. The value is defined in the charcteristic
            typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, AttrOptions... >::type     uuid;

            static const attribute attr;
        };

        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_value_declaration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >::attr {
            uuid::is_128bit
                ? bits( details::gatt_uuids::internal_128bit_uuid )
                : uuid::as_16bit(),
            &characteristic< Options... >::value_type::characteristic_value_access
        };

        /*
         * Characteristic User Description
         */
        template < const char* const Name, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex, Options... >
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

        template < const char* const Name, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex, Options... >::attr {
            bits( gatt_uuids::characteristic_user_description ),
            &generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, ClientCharacteristicIndex, Options... >::access
        };

        /*
         * Client Characteristic Configuration
         */
        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >
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

        template < typename ... AttrOptions, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >::attr {
            bits( gatt_uuids::client_characteristic_configuration ),
            &generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, ClientCharacteristicIndex, Options... >::access
        };

        /**
         * generate a const static array of attributes out of a list of tuples, containing the parameter to generate a attribute
         *
         *  Attributes: A std::tuple, containing a tuple for every attribute to generate.
         *
         *  ClientCharacteristicIndex: The index of the characteristic to be generate in the containing service
         *
         *  Options: All options that where given to the characteristic
         */
        template < typename Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute_list;

        template < std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute_list< std::tuple<>, ClientCharacteristicIndex, Options... >
        {
            static const attribute attribute_at( std::size_t index )
            {
                assert( !"should not happen" );
            }
        };

        template < typename ... Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute_list< std::tuple< Attributes... >, ClientCharacteristicIndex, Options... >
        {
            static const attribute attribute_at( std::size_t index )
            {
                return attributes[ index ];
            }

            static attribute attributes[ sizeof ...(Attributes) ];
        };

        template < typename ... Attributes, std::size_t ClientCharacteristicIndex, typename ... Options >
        attribute generate_attribute_list< std::tuple< Attributes... >, ClientCharacteristicIndex, Options... >::attributes[ sizeof ...(Attributes) ] =
        {
            generate_attribute< Attributes, ClientCharacteristicIndex, Options... >::attr...
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
                characteristic_declaration_parameter,
                characteristic_value_declaration_parameter,
                characteristic_user_description_parameter,
                client_characteristic_configuration_parameter
            >::type declaraction_parameters;

            enum { number_of_attributes     = std::tuple_size< declaraction_parameters >::value };
            enum { number_of_client_configs = count_if< declaraction_parameters, are_client_characteristic_configuration_parameter >::value };
            enum { number_of_server_configs = 0 };

            template < std::size_t ClientCharacteristicIndex, typename ServiceUUID >
            static const attribute attribute_at( std::size_t index )
            {
                return generate_attribute_list< declaraction_parameters, ClientCharacteristicIndex, Options..., ServiceUUID >::attribute_at( index );
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
