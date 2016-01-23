#ifndef BLUETOE_SERVICE_HPP
#define BLUETOE_SERVICE_HPP

#include <bluetoe/service_uuid.hpp>
#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/bits.hpp>
#include <cstddef>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace bluetoe {

    template < const char* const >
    class service_name {};

    namespace details {
        struct service_meta_type {};

        struct is_secondary_service_meta_type {};
        struct include_service_meta_type {};
        struct service_defintion_tag {};

        template < typename ... Options >
        struct generate_service_attributes;
    }

    /**
     * @brief a 128-Bit UUID used to identify a service.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::service_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
     *
     * @sa service
     */
    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E >
    class service_uuid
        /** @cond HIDDEN_SYMBOLS */
            : public details::uuid< A, B, C, D, E >
        /** @endcond */
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef details::service_uuid_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief a 16-Bit UUID used to identify a service
     *
     * @code{.cpp}
     * bluetoe::service_uuid16< 0x1816 >
     * @endcode
     *
     * @sa service
     */
    template < std::uint64_t UUID >
    class service_uuid16
        /** @cond HIDDEN_SYMBOLS */
            : public details::uuid16< UUID >
        /** @endcond */
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef details::service_uuid_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief a service with zero or more characteristics
     *
     * In GATT, a service groups a set of characteristics to something that is usefull as a group. For example, having a quadcopter, it would
     * make sense to group the x, y and z position of the quadcopter to a quadcopter position service. All characteristics that should be
     * part of the service, must be given as template parameter.
     *
     * Example:
     * @code
     * typedef bluetoe::service<
     *     bluetoe::characteristic<
     *         bluetoe::bind_characteristic_value< std::int64_t, &x_pos >,
     *         bluetoe::characteristic_name< x_pos_name >
     *     >,
     *     bluetoe::characteristic<
     *         bluetoe::bind_characteristic_value< std::int64_t, &y_pos >,
     *         bluetoe::characteristic_name< y_pos_name >
     *     >,
     *     bluetoe::characteristic<
     *         bluetoe::bind_characteristic_value< std::int64_t, &z_pos >,
     *         bluetoe::characteristic_name< z_pos_name >
     *     >,
     *     bluetoe::service_uuid< 0xA0C271BF, 0xBD17, 0x4627, 0xB5DF, 0x5E26AA194920 >
     * > quadcopter_position_service;
     * @endcode
     *
     * To identify a service, every service have to have a unique UUID. 128 bit UUIDs are used for custiom services,
     * while the shorter 16 bit UUIDs are used to identify services that have a special meaning the is documented
     * in <a href="https://developer.bluetooth.org/gatt/services/Pages/ServicesHome.aspx?_ga=1.140855218.1800461708.1407160742">specifications</a>
     * writen by the bluetooth special interrest group.
     * So every service must have either a service_uuid or a service_uuid16 as template parameter. The position of the UUID parameter is irelevant.
     * A GATT client can use this UUID to identify the service; in the example above, a GATT client would look for a service with the UUID
     * A0C271BF-BD17-4627-B5DF-5E26AA194920 to find the position service of the quadcopter.
     *
     * @sa server
     * @sa secondary_service
     * @sa is_secondary_service
     * @sa characteristic
     * @sa service_uuid
     * @sa service_uuid16
     * @sa include_service
     */
    template < typename ... Options >
    class service
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        typedef typename details::find_all_by_meta_type< details::characteristic_meta_type, Options... >::type  characteristics;

        typedef typename details::find_by_meta_type< details::service_uuid_meta_type, Options... >::type        uuid;

        static_assert( !std::is_same< uuid, details::no_such_type >::value, "Please provide a UUID to the service (service_uuid or service_uuid16 for example)." );

        typedef details::generate_service_attributes< Options... >                                              attribute_generator;

        static constexpr std::size_t number_of_service_attributes        = attribute_generator::number_of_attributes;
        static constexpr std::size_t number_of_characteristic_attributes = details::sum_by< characteristics, details::sum_by_attributes >::value;
        static constexpr std::size_t number_of_client_configs            = details::sum_by< characteristics, details::sum_by_client_configs >::value;

        /**
         * a service is a list of attributes
         */
        static constexpr std::size_t number_of_attributes =
              number_of_service_attributes
            + number_of_characteristic_attributes;

        typedef details::service_meta_type meta_type;

        /**
         * ClientCharacteristicIndex is the number of characteristics with a Client Characteristic Configuration attribute
         */
        template < std::size_t ClientCharacteristicIndex, typename ServiceList, typename Server = void >
        static details::attribute attribute_at( std::size_t index );

        /**
         * @brief assembles one data packet for a "Read by Group Type Response"
         */
        template < std::size_t ClientCharacteristicIndex, typename ServiceList, typename Server = void >
        static std::uint8_t* read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t starting_index, bool is_128bit_filter );

        /**
         * returns a correctly filled notification_data() object, if this characteristc was configured for notification or indication
         * and the given value identifies the characteristic value. If not found find_notification_data( value ).valid() is false.
         */
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static details::notification_data find_notification_data( const void* value );

        /**
         * returns a correctly filled notification_data() object, if this characteristc was configured for notification or indication
         * and the given value identifies the characteristic value. If not found find_notification_data( value ).valid() is false.
         */
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static details::notification_data find_notification_data_by_index( std::size_t index );
        /** @endcond */
    };

    /**
     * @brief modifier that defines a service to be a secondary service.
     *
     * @sa secondary_service
     * @sa service
     */
    struct is_secondary_service {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::is_secondary_service_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief definition of a secondary service
     *
     * Quote from the Core Spec: A secondary service is a service that is only intended to be
     * referenced from a primary service or another secondary service or other higher layer
     * specification. A secondary service is only relevant in the context of the entity that
     * references it.
     *
     * @sa service
     * @sa include_service
     */
    template < typename ... Options >
    struct secondary_service : service< Options..., is_secondary_service > {};

    /**
     * @brief includes an other service into the defined service
     *
     * The service to be included is defined by it's UUID (16-bit or 128-bit).
     * The included service must be defined within the very same server definition.
     * UUID is either a service_uuid<> or service_uuid16<>
     *
     * @sa service_uuid16
     * @sa service_uuid
     * @sa secondary_service
     */
    template < typename UUID >
    struct include_service;

    template <
        std::uint32_t A,
        std::uint16_t B,
        std::uint16_t C,
        std::uint16_t D,
        std::uint64_t E >
    struct include_service< service_uuid< A, B, C, D, E > >
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::include_service_meta_type meta_type;
        /** @endcond */
    };

    template < std::uint64_t UUID >
    struct include_service< service_uuid16< UUID > >
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::include_service_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @example include_example.cpp
     * This examples shows, how to define a secondary service and how to include it in an other service.
     */

    /** @cond HIDDEN_SYMBOLS */

    // service implementation
    template < typename ... Options >
    template < std::size_t ClientCharacteristicIndex, typename ServiceList, typename Server >
    details::attribute service< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        if ( index < number_of_service_attributes )
            return attribute_generator::template attribute_at< ServiceList >( index );

        return details::attribute_at_list< characteristics, ClientCharacteristicIndex, uuid, Server >::attribute_at( index - number_of_service_attributes );
    }

    template < typename ... Options >
    template < std::size_t ClientCharacteristicIndex, typename ServiceList, typename Server >
    std::uint8_t* service< Options... >::read_primary_service_response( std::uint8_t* output, std::uint8_t* end, std::uint16_t starting_index, bool is_128bit_filter )
    {
        const std::size_t attribute_data_size = is_128bit_filter ? 16 + 4 : 2 + 4;

        if ( is_128bit_filter == uuid::is_128bit && end - output >= attribute_data_size )
        {
            std::uint8_t* const old_output = output;

            output = details::write_handle( output, starting_index );
            output = details::write_handle( output, starting_index + number_of_attributes -1 );

            const details::attribute primary_service = attribute_at< ClientCharacteristicIndex, ServiceList, Server >( 0 );

            auto read = details::attribute_access_arguments::read( output, end, 0, details::client_characteristic_configuration() );

            if ( primary_service.access( read, 1 ) == details::attribute_access_result::success )
            {
                output += read.buffer_size;
            }
            else
            {
                output = old_output;
            }
        }

        return output;
    }

    template < typename ... Options >
    template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
    details::notification_data service< Options... >::find_notification_data( const void* value )
    {
        return details::find_notification_data_in_list< characteristics >::
            template find_notification_data< number_of_service_attributes + FirstAttributesHandle, ClientCharacteristicIndex >( value );
    }

    template < typename ... Options >
    template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
    details::notification_data service< Options... >::find_notification_data_by_index( std::size_t index )
    {
        return details::find_notification_data_in_list< characteristics >::
            template find_notification_data_by_index< number_of_service_attributes + FirstAttributesHandle, ClientCharacteristicIndex >( index );
    }

    namespace details {

        template < typename ServiceList, typename UUID >
        struct find_service_by_uuid
        {
            template < class T >
            struct equal_uuid : std::is_same< typename T::uuid, UUID > {};

            typedef typename find_if< ServiceList, equal_uuid >::type type;
        };

        template < typename ServiceList, typename Service, std::uint16_t Handle = 1 >
        struct service_handles;

        template < typename Service, typename ... Ss, std::uint16_t Handle >
        struct service_handles< std::tuple< Service, Ss... >, Service, Handle >
        {
            static constexpr std::uint16_t service_attribute_handle = Handle;
            static constexpr std::uint16_t end_service_handle       = Handle + Service::number_of_attributes - 1;
        };

        template < typename Service, typename S, typename ... Ss, std::uint16_t Handle >
        struct service_handles< std::tuple< S, Ss... >, Service, Handle >
        {
            typedef service_handles< std::tuple< Ss... >, Service, Handle + S::number_of_attributes > next;

            static constexpr std::uint16_t service_attribute_handle = next::service_attribute_handle;
            static constexpr std::uint16_t end_service_handle       = next::end_service_handle;
        };
        /*
         * service declaration
         */
        template < std::size_t ClientCharacteristicIndex, typename ... Options >
        struct generate_attribute< service_defintion_tag, ClientCharacteristicIndex, Options... >
        {
            static attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                typedef typename find_by_meta_type< service_uuid_meta_type, Options... >::type uuid;

                if ( args.type == attribute_access_type::read )
                {
                    if ( args.buffer_offset > sizeof( uuid::bytes ) )
                        return attribute_access_result::invalid_offset;

                    args.buffer_size = std::min< std::size_t >( sizeof( uuid::bytes ) - args.buffer_offset, args.buffer_size );

                    std::copy( std::begin( uuid::bytes ) + args.buffer_offset , std::begin( uuid::bytes ) + args.buffer_offset + args.buffer_size, args.buffer );

                    return attribute_access_result::success;
                }
                else if ( args.type == attribute_access_type::compare_value )
                {
                    if ( sizeof( uuid::bytes ) == args.buffer_size
                      && std::equal( std::begin( uuid::bytes ), std::end( uuid::bytes ), &args.buffer[ 0 ] ) )
                    {
                        return attribute_access_result::value_equal;
                    }
                }

                return attribute_access_result::write_not_permitted;
            }

            static const attribute attr;
        };

        template < std::size_t ClientCharacteristicIndex, typename ... Options >
        const attribute generate_attribute< service_defintion_tag, ClientCharacteristicIndex, Options... >::attr =
        {
            bits( has_option< is_secondary_service, Options... >::value
                ? gatt_uuids::secondary_service
                : gatt_uuids::primary_service ),
            &generate_attribute< service_defintion_tag, ClientCharacteristicIndex, Options... >::access
        };

        /*
         * include attribute for 16-bit includes
         */
        template <
            std::uint64_t UUID,
            std::size_t ClientCharacteristicIndex,
            typename ... Options >
        struct generate_attribute< include_service< service_uuid16< UUID > >, ClientCharacteristicIndex, Options... >
        {
            typedef typename last_from_pack< Options... >::type service_list;
            typedef typename find_service_by_uuid< service_list, service_uuid16< UUID > >::type included_service;

            static_assert( !std::is_same< included_service, no_such_type >::value, "The included service is was not found by UUID, please add the referenced service." );

            typedef service_handles< service_list, included_service > handles;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                static const std::uint8_t value[] = {
                    handles::service_attribute_handle & 0xff,
                    handles::service_attribute_handle >> 8,
                    handles::end_service_handle & 0xff,
                    handles::end_service_handle >> 8,
                    UUID & 0xff,
                    UUID >> 8
                };

                return attribute_value_read_only_access( args, &value[ 0 ], sizeof( value ) );
            }

            static const attribute attr;
        };

        template <
            std::uint64_t UUID,
            std::size_t ClientCharacteristicIndex,
            typename ... Options >
        const attribute generate_attribute< include_service< service_uuid16< UUID > >, ClientCharacteristicIndex, Options... >::attr =
        {
            bits( details::gatt_uuids::include ),
            &generate_attribute< include_service< service_uuid16< UUID > >, ClientCharacteristicIndex, Options... >::access
        };

        /*
         * include attribute for 128-bit includes
         */
        template <
            std::uint32_t A,
            std::uint16_t B,
            std::uint16_t C,
            std::uint16_t D,
            std::uint64_t E,
            std::size_t ClientCharacteristicIndex,
            typename ... Options >
        struct generate_attribute< include_service< service_uuid< A, B, C, D, E > >, ClientCharacteristicIndex, Options... >
        {
            typedef typename last_from_pack< Options... >::type service_list;
            typedef typename find_service_by_uuid< service_list, service_uuid< A, B, C, D, E > >::type included_service;

            static_assert( !std::is_same< included_service, no_such_type >::value, "The included service is was not found by UUID, please add the references service." );

            typedef service_handles< service_list, included_service > handles;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                static const std::uint8_t value[] = {
                    handles::service_attribute_handle & 0xff,
                    handles::service_attribute_handle >> 8,
                    handles::end_service_handle & 0xff,
                    handles::end_service_handle >> 8,
                };

                return attribute_value_read_only_access( args, &value[ 0 ], sizeof( value ) );
            }

            static const attribute attr;
        };

        template <
            std::uint32_t A,
            std::uint16_t B,
            std::uint16_t C,
            std::uint16_t D,
            std::uint64_t E,
            std::size_t ClientCharacteristicIndex,
            typename ... Options >
        const attribute generate_attribute< include_service< service_uuid< A, B, C, D, E > >, ClientCharacteristicIndex, Options... >::attr =
        {
            bits( details::gatt_uuids::include ),
            &generate_attribute< include_service< service_uuid< A, B, C, D, E > >, ClientCharacteristicIndex, Options... >::access
        };

        template < typename ... Options >
        struct generate_service_attributes
        {
            typedef typename
                add_type<
                    service_defintion_tag, // force generation of service generation attribute
                    typename find_all_by_meta_type<
                        include_service_meta_type,
                        Options...
                    >::type
                >::type attribute_generation_parameters;

            enum { number_of_attributes = std::tuple_size< attribute_generation_parameters >::value };

            template < typename ServiceList >
            static const attribute attribute_at( std::size_t index )
            {
                return generate_attribute_list< attribute_generation_parameters, 0, std::tuple< Options..., ServiceList > >::attribute_at( index );
            }
        };

    }
    /** @endcond */
}

#endif