#ifndef BLUETOE_CHARACTERISTIC_HPP
#define BLUETOE_CHARACTERISTIC_HPP

#include <bluetoe/attribute.hpp>
#include <bluetoe/codes.hpp>
#include <bluetoe/uuid.hpp>
#include <bluetoe/options.hpp>
#include <bluetoe/bits.hpp>

#include <cstddef>
#include <cassert>
#include <algorithm>
#include <type_traits>

namespace bluetoe {

    namespace details {
        struct characteristic_meta_type {};
        struct characteristic_uuid_meta_type {};
        struct characteristic_value_meta_type {};
        struct characteristic_parameter_meta_type {};

        struct characteristic_user_description_parameter {};

        template < typename ... Options >
        struct generate_attributes;
    }

    /**
     * @brief a 128-Bit UUID used to identify a characteristic.
     *
     * The class takes 5 parameters to store the UUID in the usual form like this:
     * @code{.cpp}
     * bluetoe::characteristic_uuid< 0xF0426E52, 0x4450, 0x4F3B, 0xB058, 0x5BAB1191D92A >
     * @endcode
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
     * @brief a characteristic is a data point that is accessable by client.
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
        static constexpr std::size_t number_of_attributes = 2 + characteristic_descriptor_declarations::size;

        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        typedef details::characteristic_meta_type meta_type;
        /** @endcond */

        /**
         * @brief gives access to the all attributes of the characteristic
         */
        static details::attribute attribute_at( std::size_t index );

    private:
        typedef typename details::find_by_meta_type< details::characteristic_value_meta_type, Options... >::type base_value_type;
        typedef typename base_value_type::template value_impl< Options... >                                       value_type;

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

            static details::attribute_access_result characteristic_value_access( details::attribute_access_arguments&, std::uint16_t attribute_handle );
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
    details::attribute characteristic< Options... >::attribute_at( std::size_t index )
    {
        assert( index < number_of_attributes );

        static const details::attribute attributes[ number_of_attributes ] = {
            { bits( details::gatt_uuids::characteristic ), &char_declaration_access },
            {
                uuid::is_128bit
                    ? bits( details::gatt_uuids::internal_128bit_uuid )
                    : uuid::as_16bit(),
                &value_type::characteristic_value_access
            }
        };

        return index < 2
            ? attributes[ index ]
            : characteristic_descriptor_declarations::attribute_at( index - 2 );
    }

    template < typename ... Options >
    details::attribute_access_result characteristic< Options... >::char_declaration_access( details::attribute_access_arguments& args, std::uint16_t attribute_handle )
    {
        typedef typename details::find_by_meta_type< details::characteristic_uuid_meta_type, Options... >::type uuid;
        static const auto uuid_offset = 3;

        if ( args.type == details::attribute_access_type::read )
        {
            static constexpr auto uuid_size = sizeof( uuid::bytes );

            args.buffer_size          = std::min< std::size_t >( args.buffer_size, uuid_offset + uuid_size );
            const auto max_uuid_bytes = std::min< std::size_t >( std::max< int >( 0, args.buffer_size -uuid_offset ), uuid_size );

            if ( args.buffer_size > 2 )
            {
                args.buffer[ 0 ] =
                    ( value_type::has_read_access  ? bits( details::gatt_characteristic_properties::read ) : 0 ) |
                    ( value_type::has_write_access ? bits( details::gatt_characteristic_properties::write ) : 0 );

                // the Characteristic Value Declaration must follow directly behind this attribute and has, thus the next handle
                details::write_handle( args.buffer + 1, attribute_handle + 1 );
            }

            if ( max_uuid_bytes )
                std::copy( std::begin( uuid::bytes ), std::begin( uuid::bytes ) + max_uuid_bytes, args.buffer + uuid_offset );

            return args.buffer_size == uuid_offset + uuid_size
                ? details::attribute_access_result::success
                : details::attribute_access_result::read_truncated;
        }

        return details::attribute_access_result::write_not_permitted;
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
        args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );
        static const std::uint8_t* ptr = static_cast< const std::uint8_t* >( static_cast< const void* >( Ptr ) );
        std::copy( ptr, ptr + args.buffer_size, args.buffer );

        return args.buffer_size == sizeof( T )
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
        if ( args.buffer_size > sizeof( T ) )
            return details::attribute_access_result::write_overflow;

        args.buffer_size = std::min< std::size_t >( args.buffer_size, sizeof( T ) );

        static std::uint8_t* ptr = static_cast< std::uint8_t* >( static_cast< void* >( Ptr ) );
        std::copy( args.buffer, args.buffer + args.buffer_size, ptr );

        return details::attribute_access_result::success;
    }

    template < typename T, T* Ptr >
    template < typename ... Options >
    details::attribute_access_result bind_characteristic_value< T, Ptr >::value_impl< Options... >::characteristic_value_write_access( details::attribute_access_arguments&, const std::false_type& )
    {
        return details::attribute_access_result::write_not_permitted;
    }

    namespace details {


        template < typename >
        struct generate_attribute;

        /*
         * Characteristic User Description
         */
        template < const char* const Name >
        struct generate_attribute< std::tuple< characteristic_name< Name > > >
        {
            static const attribute attr;

            static details::attribute_access_result access( attribute_access_arguments& args, std::uint16_t attribute_handle )
            {
                details::attribute_access_result result = attribute_access_result::write_not_permitted;

                if ( args.type == attribute_access_type::read )
                {
                    const std::size_t str_len   = std::strlen( Name );
                    const std::size_t read_size = std::min( args.buffer_size, str_len );

                    std::copy( Name, Name + read_size, args.buffer );

                    result = str_len > args.buffer_size
                        ? attribute_access_result::read_truncated
                        : attribute_access_result::success;

                    args.buffer_size = read_size;
                }

                return result;
            }

        };

        template < const char* const Name >
        const attribute generate_attribute< std::tuple< characteristic_name< Name > > >::attr {
            bits( gatt_uuids::characteristic_user_description ),
            &generate_attribute< std::tuple< characteristic_name< Name > > >::access
        };

        template < typename >
        struct generate_attribute_list;

        template <>
        struct generate_attribute_list< std::tuple<> >
        {
            static const attribute attribute_at( std::size_t index )
            {
                assert( !"should not happen" );
            }
        };

        template < typename ... Options >
        struct generate_attribute_list< std::tuple< Options... > >
        {
            static const attribute attribute_at( std::size_t index )
            {
                return attributes[ index ];
            }

            static attribute attributes[ sizeof ...(Options) ];
        };

        template < typename ... Options >
        attribute generate_attribute_list< std::tuple< Options... > >::attributes[ sizeof ...(Options) ] = { generate_attribute< Options >::attr... };

        template < typename ... Options >
        struct generate_attributes
        {
            typedef typename group_by_meta_types_without_empty_groups<
                std::tuple< Options... >,
                characteristic_user_description_parameter
            >::type declaraction_parameters;

            enum { size = std::tuple_size< declaraction_parameters >::value };

            static const attribute attribute_at( std::size_t index )
            {
                return generate_attribute_list< declaraction_parameters >::attribute_at( index );
            }
        };

        /** @endcond */
    }
}

#endif
