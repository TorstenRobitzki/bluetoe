#ifndef BLUETOE_ATTRIBUTE_HPP
#define BLUETOE_ATTRIBUTE_HPP

#include <bluetoe/options.hpp>
#include <bluetoe/client_characteristic_configuration.hpp>
#include <bluetoe/sm/pairing_status.hpp>
#include <cstdint>
#include <cstddef>
#include <cassert>

namespace bluetoe {
namespace details {

    /*
     * Attribute and accessing an attribute
     */

    enum class attribute_access_result : std::int_fast16_t {
        // Accessing the attribute was successfully
        success                         = 0x00,

        // here goes the ATT return codes
        invalid_offset                  = 0x07,
        write_not_permitted             = 0x03,
        read_not_permitted              = 0x02,
        invalid_attribute_value_length  = 0x0d,
        attribute_not_long              = 0x0b,
        request_not_supported           = 0x06,
        insufficient_encryption         = 0x0f,
        insufficient_authentication     = 0x05,

        // returned when access type is compare_128bit_uuid and the attribute contains a 128bit uuid and
        // the buffer in attribute_access_arguments is equal to the contained uuid.
        uuid_equal                      = 0x100,
        value_equal
    };

    enum class attribute_access_type {
        read,
        write,
        compare_128bit_uuid,
        compare_value
    };

    struct attribute_access_arguments
    {
        attribute_access_type               type;
        std::uint8_t*                       buffer;
        std::size_t                         buffer_size;
        std::size_t                         buffer_offset;
        client_characteristic_configuration client_config;
        connection_security_attributes      connection_security;
        void*                               server;

        template < std::size_t N >
        static constexpr attribute_access_arguments read(
            std::uint8_t(&buffer)[N],
            std::size_t offset,
            const client_characteristic_configuration& cc = client_characteristic_configuration(),
            const connection_security_attributes& cs = connection_security_attributes() )
        {
            return attribute_access_arguments{
                attribute_access_type::read,
                &buffer[ 0 ],
                N,
                offset,
                cc,
                cs,
                nullptr
            };
        }

        static attribute_access_arguments read(
            std::uint8_t* begin, std::uint8_t* end, std::size_t offset,
            const client_characteristic_configuration& cc,
            const connection_security_attributes& cs,
            void* server )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::read,
                begin,
                static_cast< std::size_t >( end - begin ),
                offset,
                cc,
                cs,
                server
            };
        }

        template < std::size_t N >
        static constexpr attribute_access_arguments write( const std::uint8_t(&buffer)[N], std::size_t offset = 0,
            const client_characteristic_configuration& cc = client_characteristic_configuration(),
            const connection_security_attributes& cs = connection_security_attributes() )
        {
            return attribute_access_arguments{
                attribute_access_type::write,
                const_cast< std::uint8_t* >( &buffer[ 0 ] ),
                N,
                offset,
                cc,
                cs,
                nullptr
            };
        }

        static attribute_access_arguments write( const std::uint8_t* begin, const std::uint8_t* end, std::size_t offset,
            const client_characteristic_configuration& cc,
            const connection_security_attributes& cs,
            void* server )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::write,
                const_cast< std::uint8_t* >( begin ),
                static_cast< std::size_t >( end - begin ),
                offset,
                cc,
                cs,
                server
            };
        }

        static constexpr attribute_access_arguments check_write( void* server )
        {
            return attribute_access_arguments{
                attribute_access_type::write,
                0,
                0,
                0,
                client_characteristic_configuration(),
                connection_security_attributes(),
                server
            };
        }

        static constexpr attribute_access_arguments compare_128bit_uuid( const std::uint8_t* uuid )
        {
            return attribute_access_arguments{
                attribute_access_type::compare_128bit_uuid,
                const_cast< std::uint8_t* >( uuid ),
                16u,
                0,
                client_characteristic_configuration(),
                connection_security_attributes(),
                nullptr
            };
        }

        static attribute_access_arguments compare_value( const std::uint8_t* begin, const std::uint8_t* end, void* )
        {
            assert( end >= begin );

            return attribute_access_arguments{
                attribute_access_type::compare_value,
                const_cast< std::uint8_t* >( begin ),
                static_cast< std::size_t >( end - begin ),
                0,
                client_characteristic_configuration(),
                connection_security_attributes(),
                nullptr
            };
        }
    };

    typedef attribute_access_result ( *attribute_access )( attribute_access_arguments&, std::uint16_t attribute_handle );

    /*
     * An attribute is an uuid combined with a mean of how to access the attributes
     *
     * design decisions to _not_ use pointer to staticaly allocated virtual objects:
     * - makeing access a pointer to a virtual base class would result in storing a pointer that points to a pointer (vtable) to list with a bunch of functions.
     * - it's most likely that most attributes will not have any mutable data.
     * attribute contains only one function that takes a attribute_access_type to save memory and in the expectation that there are only a few different combination of
     * access functions.
     */
    struct attribute {
        // all uuids used by GATT are 16 bit UUIDs (except for Characteristic Value Declaration for which the value internal_128bit_uuid is used, if the UUID is 128bit long)
        std::uint16_t       uuid;
        attribute_access    access;
    };

    /*
     * Given that T is a tuple with elements that implement attribute_at< std::size_t, Service >() and number_of_attributes, the type implements
     * attribute_at() for a list of attribute lists.
     */
    template < typename T, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename Service, typename Server >
    struct attribute_at_list;

    template < typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename Service, typename Server >
    struct attribute_at_list< std::tuple<>, CCCDIndices, ClientCharacteristicIndex, Service, Server >
    {
        static details::attribute attribute_at( std::size_t )
        {
            assert( !"index out of bound" );
            return details::attribute();
        }
    };

    template <
        typename T,
        typename ...Ts,
        typename CCCDIndices,
        std::size_t ClientCharacteristicIndex,
        typename Service,
        typename Server >
    struct attribute_at_list< std::tuple< T, Ts... >, CCCDIndices, ClientCharacteristicIndex, Service, Server >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            if ( index < T::number_of_attributes )
                return T::template attribute_at< CCCDIndices, ClientCharacteristicIndex, Service, Server >( index );

            typedef details::attribute_at_list< std::tuple< Ts... >, CCCDIndices, ClientCharacteristicIndex + T::number_of_client_configs, Service, Server > remaining_characteristics;

            return remaining_characteristics::attribute_at( index - T::number_of_attributes );
        }
    };

    /*
     * Iterating the list of services is the same, but needs less parameters
     */
    template < typename Services, typename Server, typename CCCDIndices, std::size_t ClientCharacteristicIndex = 0, typename AllServices = Services >
    struct attribute_from_service_list;

    template < typename Server, typename CCCDIndices, std::size_t ClientCharacteristicIndex, typename AllServices >
    struct attribute_from_service_list< std::tuple<>, Server, CCCDIndices, ClientCharacteristicIndex, AllServices >
    {
        static details::attribute attribute_at( std::size_t )
        {
            assert( !"index out of bound" );
            return details::attribute();
        }
    };

    template <
        typename T,
        typename ...Ts,
        typename Server,
        typename CCCDIndices,
        std::size_t ClientCharacteristicIndex,
        typename AllServices >
    struct attribute_from_service_list< std::tuple< T, Ts... >, Server, CCCDIndices, ClientCharacteristicIndex, AllServices >
    {
        static details::attribute attribute_at( std::size_t index )
        {
            if ( index < T::number_of_attributes )
                return T::template attribute_at< CCCDIndices, ClientCharacteristicIndex, AllServices, Server >( index );

            typedef details::attribute_from_service_list<
                std::tuple< Ts... >,
                Server,
                CCCDIndices,
                ClientCharacteristicIndex + T::number_of_client_configs,
                AllServices > remaining_characteristics;

            return remaining_characteristics::attribute_at( index - T::number_of_attributes );
        }
    };

    /**
     * @brief data needed to send an indication or notification to the l2cap layer
     */
    class notification_data
    {
    public:
        notification_data()
            : att_handle_( 0 )
            , client_characteristic_configuration_index_( 0 )
        {
            assert( !valid() );
        }

        notification_data( std::uint16_t value_attribute_handle, std::size_t client_characteristic_configuration_index )
            : att_handle_( value_attribute_handle )
            , client_characteristic_configuration_index_( client_characteristic_configuration_index )
        {
            assert( valid() );
        }

        bool valid() const
        {
            return att_handle_ != 0;
        }

        std::uint16_t handle() const
        {
            return att_handle_;
        }

        std::size_t client_characteristic_configuration_index() const
        {
            return client_characteristic_configuration_index_;
        }

        /*
         * For testing
         */
        void clear()
        {
            att_handle_ = 0;
            client_characteristic_configuration_index_ = 0;
        }
    private:
        std::uint16_t   att_handle_;
        std::size_t     client_characteristic_configuration_index_;
    };

    /*
     * Given a list of characteristics, find the data required for notification
     */
    template < typename CharacteristicList, typename UUID >
    struct find_characteristic_data_by_uuid_in_characteristic_list;

    template < typename UUID >
    struct find_characteristic_data_by_uuid_in_characteristic_list< std::tuple<>, UUID >
    {
        typedef details::no_such_type type;
    };

    template <
        typename Characteristic,
        typename ... Characteristics,
        typename UUID >
    struct find_characteristic_data_by_uuid_in_characteristic_list< std::tuple< Characteristic, Characteristics...>, UUID >
    {
        using found = std::is_same< typename Characteristic::configured_uuid, UUID >;

        using next = typename find_characteristic_data_by_uuid_in_characteristic_list<
            std::tuple< Characteristics... >,
            UUID >::type;

        struct result
        {
            static constexpr bool has_indication   = Characteristic::value_type::has_indication;
            static constexpr bool has_notification = Characteristic::value_type::has_notification;
            using characteristic_t = Characteristic;
        };

        typedef typename details::select_type<
            found::value,
            result,
            next
        >::type type;
    };

    /*
     * Given a list of services and a UUID, find the data required for notification
     */
    template < typename ServiceList, typename UUID >
    struct find_characteristic_data_by_uuid_in_service_list;

    template < typename UUID >
    struct find_characteristic_data_by_uuid_in_service_list< std::tuple<>, UUID >
    {
        typedef details::no_such_type type;
    };

    struct wrap_search_in_service_list {
        template <
            typename UUID,
            typename ... Services >
        using f = typename find_characteristic_data_by_uuid_in_service_list<
            std::tuple< Services... >, UUID >::type;
    };

    template < typename Result >
    struct wrap_result_found_in_service {
        template <
            typename UUID,
            typename ... Services >
        using f = Result;
    };

    template <
        typename Service,
        typename ... Services,
        typename UUID >
    struct find_characteristic_data_by_uuid_in_service_list< std::tuple< Service, Services...>, UUID >
    {
        // The searched UUID is either in first Service or in the remaining service. There is no point in searching through
        // the remaining service, if the first service contains the characteristic already
        using c_type = typename find_characteristic_data_by_uuid_in_characteristic_list<
            typename Service::characteristics, UUID >::type;

        using next_path =
            typename select_type<
                std::is_same< c_type, no_such_type  >::value,
                wrap_search_in_service_list,
                wrap_result_found_in_service< c_type >
            >::type;

        using type = typename next_path::template f<
            UUID,
            Services... >;
    };


    inline attribute_access_result attribute_value_read_access( attribute_access_arguments& args, const std::uint8_t* memory, std::size_t size )
    {
        if ( args.type == attribute_access_type::compare_value )
        {
            return size == args.buffer_size && std::equal( memory, memory + size, args.buffer )
                ? attribute_access_result::value_equal
                : attribute_access_result::read_not_permitted;
        }

        if ( args.buffer_offset > size )
            return details::attribute_access_result::invalid_offset;

        args.buffer_size = std::min< std::size_t >( args.buffer_size, size - args.buffer_offset );
        const std::uint8_t* const ptr = memory + args.buffer_offset;

        std::copy( ptr, ptr + args.buffer_size, args.buffer );

        return details::attribute_access_result::success;
    }

    inline attribute_access_result attribute_value_read_only_access( attribute_access_arguments& args, const std::uint8_t* memory, std::size_t size )
    {
        if ( args.type != attribute_access_type::read && args.type != attribute_access_type::compare_value )
            return attribute_access_result::write_not_permitted;

        return attribute_value_read_access( args, memory, size );
    }

}
}

#endif
