#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/characteristic_value.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/bits.hpp>
#include <bluetoe/scattered_access.hpp>
#include <bluetoe/service_uuid.hpp>
#include <bluetoe/attribute_generator.hpp>
#include <bluetoe/server_meta_type.hpp>

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

        template < typename CCCDIndices, typename ... Options >
        struct generate_characteristic_attributes;

        template < typename ... Options >
        struct count_characteristic_attributes;

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
     * @brief A characteristic is a typed value that is accessable by a GATT client hosted by a GATT server.
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
     *    bluetoe::characteristic_uuid< 0xF0E6EBE6, 0x3749, 0x41A6, 0xB190, 0x591B262AC20A >
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
     * @sa indicate
     * @sa higher_outgoing_priority
     * @sa lower_outgoing_priority
     * @sa write_without_response
     * @sa bind_characteristic_value
     * @sa characteristic_name
     * @sa free_read_blob_handler
     * @sa free_read_handler
     * @sa free_write_blob_handler
     * @sa free_write_handler
     * @sa mixin_read_handler
     * @sa mixin_write_handler
     * @sa mixin_read_blob_handler
     * @sa mixin_write_blob_handler
     * @sa mixin_write_indication_control_point_handler
     * @sa mixin_write_notification_control_point_handler
     * @sa requires_encryption
     * @sa no_encryption_required
     * @sa may_require_encryption
     */
    template < typename ... Options >
    class characteristic
    {
    public:
        /** @cond HIDDEN_SYMBOLS */

        using attribute_numbers = details::count_characteristic_attributes< Options... >;

        /**
         * a characteristic is a list of attributes
         */
        static constexpr std::size_t number_of_attributes     = attribute_numbers::number_of_attributes;
        static constexpr std::size_t number_of_client_configs = attribute_numbers::number_of_client_configs;

        typedef details::characteristic_meta_type meta_type;

        // this is just the configured UUID, if auto uuids are used, this will be no_such_type
        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type configured_uuid;

        /**
         * @brief gives access to all attributes of the characteristic
         * @todo remove
         */
        template < typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ServiceUUID = void, typename Server = void >
        static details::attribute attribute_at( std::size_t index );

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
    constexpr char name[] = "This is the name of the characteristic";

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
    template < typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ServiceUUID, typename Server >
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        using characteristic_descriptor_declarations = typename details::generate_characteristic_attributes< CCCDIndices, Options... >;
        return characteristic_descriptor_declarations::template attribute_at< ClientCharacteristicIndex, ServiceUUID, Server >( index );
    }

    namespace details {
        template < typename ... Options >
        struct characteristic_or_service_uuid
        {
            typedef typename find_by_meta_type< service_uuid_meta_type, Options... >::type            service_uuid;

            typedef typename find_by_meta_type< characteristic_uuid_meta_type, Options... >::type     char_uuid;

            typedef typename or_type< no_such_type, char_uuid, service_uuid >::type                   uuid;

            static_assert( !std::is_same< uuid, no_such_type >::value, "If instanciating a characteristic<> for testing, please provide a UUID." );

            static constexpr bool auto_generated_uuid = std::is_same< char_uuid, no_such_type >::value;
        };

        /*
         * Characteristic declaration
         */
        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >
        {
            typedef typename characteristic_or_service_uuid< Options... >::uuid                             uuid;
            typedef typename characteristic< Options... >::value_type                                       value_type;

            static void fixup_auto_uuid( details::attribute_access_arguments& args )
            {
                // needs a 16 bit characteristic index!!!
                std::uint16_t char_index = 1;

                static constexpr std::size_t uuid_offset = 3;

                int index_low  = static_cast< int >( args.buffer_offset ) - static_cast< int >( uuid_offset );
                int index_high = static_cast< int >( args.buffer_offset ) - static_cast< int >( uuid_offset + 1 );

                if ( index_low >= 0 && index_low < static_cast< int >( args.buffer_size ) )
                    args.buffer[ index_low ] ^= ( char_index & 0xff );

                if ( index_high >= 0 && index_high < static_cast< int >( args.buffer_size ) )
                    args.buffer[ index_high ] ^= ( char_index >> 8 );
            }

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
                        ( value_type::has_write_without_response ? bits( details::gatt_characteristic_properties::write_without_response ) : 0 ) |
                        ( value_type::has_notification ? bits( details::gatt_characteristic_properties::notify ) : 0 ) |
                        ( value_type::has_indication   ? bits( details::gatt_characteristic_properties::indicate ) : 0 ) )
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

                if ( characteristic_or_service_uuid< Options... >::auto_generated_uuid )
                    fixup_auto_uuid( args );

                args.buffer_size = std::min< std::size_t >( data_size - args.buffer_offset, args.buffer_size );

                return details::attribute_access_result::success;
            }

            static const attribute attr;
        };

        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >::attr {
            bits( details::gatt_uuids::characteristic ),
            &generate_attribute< std::tuple< characteristic_declaration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >::char_declaration_access
        };

        /*
         * Characteristic Value
         */
        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_value_declaration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >
        {
            // the characterist value has two configurable aspects: the uuid and the value. The value is defined in the charcteristic
            typedef typename characteristic_or_service_uuid< Options... >::uuid      uuid;
            typedef typename find_by_meta_type< server_meta_type, Options... >::type server;

            static const attribute attr;
        };

        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_value_declaration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >::attr {
            uuid::is_128bit
                ? bits( details::gatt_uuids::internal_128bit_uuid )
                : uuid::as_16bit(),
            &characteristic< Options... >::value_type::template characteristic_value_access< server, ClientCharacteristicIndex >
        };

        /*
         * Characteristic User Description
         */
        template < const char* const Name, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, CCCDIndices, ClientCharacteristicIndex, Options... >
        {
            static const attribute attr;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t )
            {
                const std::size_t str_len   = std::strlen( Name );

                if ( args.buffer_offset > str_len )
                    return details::attribute_access_result::invalid_offset;

                details::attribute_access_result result = attribute_access_result::write_not_permitted;

                if ( args.type == attribute_access_type::read )
                {
                    const std::size_t read_size = std::min( args.buffer_size, str_len - args.buffer_offset );

                    std::copy( Name + args.buffer_offset, Name + args.buffer_offset + read_size, args.buffer );

                    result = attribute_access_result::success;

                    args.buffer_size = read_size;
                }

                return result;
            }

        };

        template < const char* const Name, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, CCCDIndices, ClientCharacteristicIndex, Options... >::attr {
            bits( gatt_uuids::characteristic_user_description ),
            &generate_attribute< std::tuple< characteristic_user_description_parameter, characteristic_name< Name > >, CCCDIndices, ClientCharacteristicIndex, Options... >::access
        };

        /*
         * Client Characteristic Configuration Descriptor (CCCD)
         */
        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >
        {
            static const attribute attr;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t )
            {
                static constexpr std::size_t flags_size = 2;
                std::uint8_t buffer[ flags_size ];

                if ( args.buffer_offset > flags_size )
                    return details::attribute_access_result::invalid_offset;

                details::attribute_access_result result = attribute_access_result::write_not_permitted;

                // currently, a lot of test code supplies an empty CCCDIndices list. In this case, use the ClientCharacteristicIndex
                const std::size_t cccd_position_index = index_of< std::integral_constant< std::size_t, ClientCharacteristicIndex >, CCCDIndices >::value;
                const std::size_t cccd_position = std::tuple_size< CCCDIndices >::value == 0
                                                    ? ClientCharacteristicIndex
                                                    : cccd_position_index;

                if ( args.type == attribute_access_type::read )
                {
                    write_16bit( &buffer[ 0 ], args.client_config.flags( cccd_position ) );

                    const std::size_t read_size = std::min( args.buffer_size, flags_size - args.buffer_offset );

                    std::copy( &buffer[ args.buffer_offset ], &buffer[ args.buffer_offset + read_size ], args.buffer );

                    result = attribute_access_result::success;

                    args.buffer_size = read_size;
                }
                else if ( args.type == attribute_access_type::write )
                {
                    if ( args.buffer_size + args.buffer_offset > flags_size )
                        return details::attribute_access_result::invalid_attribute_value_length;

                    if ( args.buffer_offset == 0 )
                        args.client_config.flags( cccd_position, read_16bit( &args.buffer[ 0 ] ) );

                    result = attribute_access_result::success;
                }

                return result;
            }
        };

        template < typename ... AttrOptions, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >::attr {
            bits( gatt_uuids::client_characteristic_configuration ),
            &generate_attribute< std::tuple< client_characteristic_configuration_parameter, AttrOptions... >, CCCDIndices, ClientCharacteristicIndex, Options... >::access
        };

        template < typename Parmeters >
        struct are_client_characteristic_configuration_parameter : std::false_type {};

        template < typename ... Ts >
        struct are_client_characteristic_configuration_parameter< std::tuple< client_characteristic_configuration_parameter, Ts... > > : std::true_type {};

        template < typename CCCDIndices, typename ... Options >
        struct generate_characteristic_attributes : generate_attributes<
                std::tuple< Options... >,
                std::tuple<
                    characteristic_declaration_parameter,
                    characteristic_value_declaration_parameter,
                    characteristic_user_description_parameter,
                    client_characteristic_configuration_parameter
                >,
                CCCDIndices,
                // force the existens of an characteristic declaration, even without Options with this meta_type
                std::tuple< empty_meta_type< characteristic_declaration_parameter > >
            > {};

        template < typename ... Options >
        struct count_characteristic_attributes
        {
            enum { number_of_client_configs =
                count_by_meta_type< client_characteristic_configuration_parameter, Options... >::count ? 1 : 0 };

            enum { number_of_user_descriptions =
                count_by_meta_type< characteristic_user_description_parameter, Options... >::count ? 1 : 0 };

            enum { number_of_attributes = 2 + number_of_client_configs + number_of_user_descriptions };
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
