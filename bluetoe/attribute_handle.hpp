#ifndef BLUETOE_ATTRIBUTE_HANDLE_HPP
#define BLUETOE_ATTRIBUTE_HANDLE_HPP

#include <bluetoe/meta_types.hpp>
#include <bluetoe/meta_tools.hpp>

#include <cstdint>
#include <cstdlib>

namespace bluetoe {

    namespace details {
        struct attribute_handle_meta_type {};
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
        struct meta_type :
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

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename ... Options >
        struct characteristic_index_mapping
        {
            using characteristic_t = ::bluetoe::characteristic< Options... >;

            using declaration_handle_t = typename find_by_meta_type< attribute_handle_meta_type, Options..., attribute_handle< StartHandle > >::type;
            static constexpr std::uint16_t declaration_handle = declaration_handle_t::attribute_handle_value;

            static_assert( declaration_handle >= StartHandle, "attribute_handle<> can only be used to create increasing attribute handles." );

            static constexpr std::uint16_t end_handle   = declaration_handle + characteristic_t::number_of_attributes;
            static constexpr std::uint16_t end_index    = StartIndex + characteristic_t::number_of_attributes;

            std::uint16_t characteristic_attribute_handle_by_index( std::size_t index )
            {
                return declaration_handle + ( index - StartIndex );
            }
        };

        template < std::uint16_t StartHandle, std::uint16_t StartIndex, typename CharacteristicList >
        struct interate_characteristic_index_mappings;

        template < std::uint16_t StartHandle, std::uint16_t StartIndex >
        struct interate_characteristic_index_mappings< StartHandle, StartIndex, std::tuple<> >
        {
            std::uint16_t attribute_handle_by_index( std::size_t )
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

            std::uint16_t attribute_handle_by_index( std::size_t index )
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

            std::uint16_t characteristic_handle_by_index( std::size_t index )
            {
                if ( index == StartIndex )
                    return service_handle;

                return this->attribute_handle_by_index( index );
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
            std::uint16_t service_handle_by_index( std::size_t )
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
            std::uint16_t service_handle_by_index( std::size_t index )
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

            std::size_t index_by_handle( std::uint16_t attribute_handle ) {
                return attribute_handle - 1;
            }

            std::uint16_t handle_by_index( std::size_t index )
            {
                return this->service_handle_by_index( index );
            }
        };

    }
}

#endif

