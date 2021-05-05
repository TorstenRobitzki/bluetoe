#ifndef BLUETOE_ATTRIBUTE_HANDLE_HPP
#define BLUETOE_ATTRIBUTE_HANDLE_HPP

#include <bluetoe/meta_types.hpp>
#include <bluetoe/meta_tools.hpp>

#include <cstdint>
#include <cstdlib>
#include <cassert>

namespace bluetoe {

    namespace details {
        struct attribute_handle_meta_type {};
        struct attribute_handles_meta_type {};
    }

    /**
     * @brief define the first attribute handle used by a characteristic or service
     *
     * If this option is given to a service, the service attribute will be assigned
     * to the given handle value. For a characteristic, the Characteristic Declaration
     * attribute will be assigned to the given handle value.
     *
     * All following attrbutes are assigned handles with a larger value.
     *
     * @sa attribute_handles
     * @sa service
     * @sa characteristic
     */
    template < std::uint16_t AttributeHandleValue >
    struct attribute_handle
    {
        /** @cond HIDDEN_SYMBOLS */
        static constexpr std::uint16_t attribute_handle_value = AttributeHandleValue;

        struct meta_type :
            details::attribute_handle_meta_type,
            details::valid_service_option_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief define the attributes handles for the characteristic declaration, characteristic
     *        value and optional, for a Client Characteristic Configuration descriptor.
     *
     * If the characteristic has no Client Characteristic Configuration descriptor, the CCCD
     * parameter has to be 0. Value has to be larger than Declaration and CCCD has to larger
     * than Value (or 0).
     *
     * @sa attribute_handle
     * @sa characteristic
     */
    template < std::uint16_t Declaration, std::uint16_t Value, std::uint16_t CCCD = 0x0000 >
    struct attribute_handles
    {
        /** @cond HIDDEN_SYMBOLS */
        static constexpr std::uint16_t declaration_handle = Declaration;
        static constexpr std::uint16_t value_handle       = Value;
        static constexpr std::uint16_t cccd_handle        = CCCD;

        static_assert( value_handle > declaration_handle, "value handle has to be larger than declaration handle" );
        static_assert( cccd_handle > declaration_handle || cccd_handle == 0, "CCCD handle has to be larger than declaration handle" );
        static_assert( cccd_handle > value_handle || cccd_handle == 0, "CCCD handle has to be larger than value handle" );

        struct meta_type :
            details::attribute_handles_meta_type,
            details::valid_characteristic_option_meta_type {};
        /** @endcond */
    };

    template < typename ... Options >
    class server;

    template < typename ... Options >
    class service;

    template < typename ... Options >
    class characteristic;

    namespace details {

        static constexpr std::uint16_t invalid_attribute_handle = 0;
        static constexpr std::size_t   invalid_attribute_index  = ~0;

        /*
         * select one of attribute_handle< H > or attribute_handle< H, B, C >
         */
        template < std::uint16_t Default, class, class >
        struct select_attribute_handles
        {
            static constexpr std::uint16_t declaration_handle = Default;
            static constexpr std::uint16_t value_handle       = Default + 1;
            static constexpr std::uint16_t cccd_handle        = Default + 2;
        };

        template < std::uint16_t Default, std::uint16_t AttributeHandleValue, typename T >
        struct select_attribute_handles< Default, attribute_handle< AttributeHandleValue >, T >
        {
            static constexpr std::uint16_t declaration_handle = AttributeHandleValue;
            static constexpr std::uint16_t value_handle       = AttributeHandleValue + 1;
            static constexpr std::uint16_t cccd_handle        = AttributeHandleValue + 2;
        };

        template < std::uint16_t Default, typename T, std::uint16_t Declaration, std::uint16_t Value, std::uint16_t CCCD >
        struct select_attribute_handles< Default, T, attribute_handles< Declaration, Value, CCCD > >
        {
            static constexpr std::uint16_t declaration_handle = Declaration;
            static constexpr std::uint16_t value_handle       = Value;
            static constexpr std::uint16_t cccd_handle        = CCCD == 0 ? Value + 1 : CCCD;
        };

        template < std::uint16_t Default, std::uint16_t AttributeHandleValue, std::uint16_t Declaration, std::uint16_t Value, std::uint16_t CCCD >
        struct select_attribute_handles< Default, attribute_handle< AttributeHandleValue >, attribute_handles< Declaration, Value, CCCD > >
        {
            static_assert( Declaration == Value, "either attribute_handle<> or attribute_handles<> can be used as characteristic<> option, not both." );
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options >
        struct characteristic_index_mapping
        {
            using characteristic_t = ::bluetoe::characteristic< Options... >;

            using attribute_handles_t = select_attribute_handles< StartHandle,
                typename find_by_meta_type< attribute_handle_meta_type, Options... >::type,
                typename find_by_meta_type< attribute_handles_meta_type, Options... >::type
            >;

            static_assert( attribute_handles_t::declaration_handle >= StartHandle, "attribute_handle<> can only be used to create increasing attribute handles." );

            static constexpr std::uint16_t end_handle   = characteristic_t::number_of_attributes == 2
                ? attribute_handles_t::value_handle + 1
                : attribute_handles_t::cccd_handle + ( characteristic_t::number_of_attributes - 2 );
            static constexpr std::uint16_t end_index    = StartIndex + characteristic_t::number_of_attributes;

            static std::uint16_t characteristic_attribute_handle_by_index( std::size_t index )
            {
                static constexpr std::size_t declaration_position = 0;
                static constexpr std::size_t value_position       = 1;
                static constexpr std::size_t cccd_position        = 2;

                const std::size_t relative_index = index - StartIndex;

                assert( relative_index < characteristic_t::number_of_attributes );

                switch ( relative_index )
                {
                    case declaration_position:
                        return attribute_handles_t::declaration_handle;
                        break;

                    case value_position:
                        return attribute_handles_t::value_handle;
                        break;

                    case cccd_position:
                        return attribute_handles_t::cccd_handle;
                        break;
                }

                return relative_index - cccd_position + attribute_handles_t::cccd_handle;
            }
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename CharacteristicList >
        struct interate_characteristic_index_mappings;

        template < std::uint16_t StartHandle, std::uint16_t StartIndex >
        struct interate_characteristic_index_mappings< StartHandle, StartIndex, std::tuple<> >
        {
            static std::uint16_t attribute_handle_by_index( std::size_t )
            {
                return invalid_attribute_handle;
            }

            static constexpr std::uint16_t last_characteristic_end_handle = StartHandle;
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename Characteristics, typename ... Options >
        using next_characteristic_mapping = interate_characteristic_index_mappings<
            characteristic_index_mapping< StartHandle, StartIndex, Options... >::end_handle,
            characteristic_index_mapping< StartHandle, StartIndex, Options... >::end_index,
            Characteristics
        >;

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ...Options, typename ... Chars >
        struct interate_characteristic_index_mappings< StartHandle, StartIndex, std::tuple< ::bluetoe::characteristic< Options... >, Chars... > >
            : characteristic_index_mapping< StartHandle, StartIndex, Options... >
            , next_characteristic_mapping< StartHandle, StartIndex, std::tuple< Chars... >, Options... >
        {
            static constexpr std::uint16_t last_characteristic_end_handle =
                next_characteristic_mapping< StartHandle, StartIndex, std::tuple< Chars... >, Options... >::last_characteristic_end_handle;

            static std::uint16_t attribute_handle_by_index( std::size_t index )
            {
                if ( index < characteristic_index_mapping< StartHandle, StartIndex, Options... >::end_index )
                    return characteristic_index_mapping< StartHandle, StartIndex, Options... >::characteristic_attribute_handle_by_index( index );

                return next_characteristic_mapping< StartHandle, StartIndex, std::tuple< Chars... >, Options... >::attribute_handle_by_index( index );
            }
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options >
        struct service_start_handle
        {
            using start_handle_t = typename find_by_meta_type< attribute_handle_meta_type, Options..., attribute_handle< StartHandle > >::type;

            static constexpr std::uint16_t value = start_handle_t::attribute_handle_value;
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options >
        using next_char_mapping = interate_characteristic_index_mappings<
                service_start_handle< StartHandle, StartIndex, Options... >::value + 1, StartIndex + 1,
                typename find_all_by_meta_type< characteristic_meta_type, Options... >::type >;

        /*
         * Map index to handle within a single service
         */
        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options >
        struct service_index_mapping
            : next_char_mapping< StartHandle, StartIndex, Options... >
        {
            using service_t = ::bluetoe::service< Options... >;

            static constexpr std::uint16_t service_handle = service_start_handle< StartHandle, StartIndex, Options... >::value;

            static_assert( service_handle >= StartHandle, "attribute_handle<> can only be used to create increasing attribute handles." );

            static constexpr std::uint16_t end_handle   = next_char_mapping< StartHandle, StartIndex, Options... >::last_characteristic_end_handle;
            static constexpr std::uint16_t end_index    = StartIndex + service_t::number_of_attributes;

            static std::uint16_t characteristic_handle_by_index( std::size_t index )
            {
                if ( index == StartIndex )
                    return service_handle;

                return next_char_mapping< StartHandle, StartIndex, Options... >::attribute_handle_by_index( index );
            }
        };

        /*
         * Iterate over all services in a server
         */
        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename OptionTuple >
        struct interate_service_index_mappings;

        template < std::uint16_t StartHandle, std::uint16_t StartIndex >
        struct interate_service_index_mappings< StartHandle, StartIndex, std::tuple<> >
        {
            static std::uint16_t service_handle_by_index( std::size_t )
            {
                return invalid_attribute_handle;
            }
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename Services, typename ... Options >
        using next_service_mapping = interate_service_index_mappings<
            service_index_mapping< StartHandle, StartIndex, Options... >::end_handle,
            service_index_mapping< StartHandle, StartIndex, Options... >::end_index,
            Services
        >;

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options, typename ... Services >
        struct interate_service_index_mappings< StartHandle, StartIndex, std::tuple< ::bluetoe::service< Options... >, Services... > >
            : service_index_mapping< StartHandle, StartIndex, Options... >
            , next_service_mapping< StartHandle, StartIndex, std::tuple< Services... >, Options... >
        {
            static std::uint16_t service_handle_by_index( std::size_t index )
            {
                if ( index < service_index_mapping< StartHandle, StartIndex, Options... >::end_index )
                    return service_index_mapping< StartHandle, StartIndex, Options... >::characteristic_handle_by_index( index );

                return next_service_mapping< StartHandle, StartIndex, std::tuple< Services... >, Options... >::service_handle_by_index( index );
            }
        };

        /*
         * Interface, providing function to map from 0-based attribute index to ATT attribute handle and vice versa
         */
        template < typename Server >
        struct handle_index_mapping;

        template < typename ... Options >
        struct handle_index_mapping< ::bluetoe::server< Options... > >
            : private interate_service_index_mappings< 1u, 0u, typename ::bluetoe::server< Options... >::services >
        {
            static constexpr std::size_t   invalid_attribute_index  = ::bluetoe::details::invalid_attribute_index;
            static constexpr std::uint16_t invalid_attribute_handle = ::bluetoe::details::invalid_attribute_handle;

            static std::uint16_t handle_by_index( std::size_t index )
            {
                return interate_service_index_mappings< 1u, 0u, typename ::bluetoe::server< Options... >::services >::service_handle_by_index( index );
            }
        };

    }
}

#endif

