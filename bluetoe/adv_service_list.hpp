#ifndef BLUETOE_ADV_SERVICE_LIST_HPP
#define BLUETOE_ADV_SERVICE_LIST_HPP

#include <codes.hpp>
#include <bits.hpp>
#include <meta_tools.hpp>
#include <service_uuid.hpp>
#include <meta_types.hpp>

namespace bluetoe {

    namespace details {
        struct list_of_16_bit_service_uuids_tag {};
        struct list_of_128_bit_service_uuids_tag {};
    }

    /**
     * @brief complete / incomplete list of 16 bit service UUIDs to be added to the advertising data
     *
     * If there is enough room to add all given UUIDs to the advertising data, a complete list AD type
     * is added. If there is not enough room, the list will be incomplete. UUIDs from the beginning
     * of the list are added as long as there is room. If there is no room for a single UUID, no
     * advertising data is added.
     */
    template < typename ... UUID16 >
    struct list_of_16_bit_service_uuids {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type
            : details::list_of_16_bit_service_uuids_tag
            , details::valid_server_option_meta_type
        {};

        static std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* end )
        {
            const std::size_t buffer_size = end - begin;

            if ( buffer_size < 4 )
                return begin;

            const std::size_t max_uuids = std::min( (buffer_size - 2) / 2, sizeof ...(UUID16));

            *begin = 1 + 2 * max_uuids;
            ++begin;

            *begin = max_uuids == sizeof ...(UUID16)
                ? bits( details::gap_types::complete_service_uuids_16 )
                : bits( details::gap_types::incomplete_service_uuids_16 );
            ++begin;

            for ( std::size_t uuid = 0; uuid != max_uuids; ++uuid )
            {
                begin = details::write_16bit( begin, values_[ uuid ] );
            }

            return begin;
        }

        static constexpr std::uint16_t values_[ sizeof ...(UUID16) ] = { UUID16::as_16bit()...};
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    template < typename ... UUID16 >
    constexpr std::uint16_t list_of_16_bit_service_uuids< UUID16... >::values_[ sizeof ...(UUID16) ];
    /** @endcond */

    /** @cond HIDDEN_SYMBOLS */
    template <>
    struct list_of_16_bit_service_uuids<> {
        struct meta_type
            : details::list_of_16_bit_service_uuids_tag
            , details::valid_server_option_meta_type
        {};

        static std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* )
        {
            return begin;
        }
    };

    template < typename ... UUID16 >
    struct list_of_16_bit_service_uuids< std::tuple< UUID16... > > : list_of_16_bit_service_uuids< UUID16... > {};
    /** @endcond */

    struct no_list_of_service_uuids {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type
            : details::list_of_16_bit_service_uuids_tag
            , details::list_of_128_bit_service_uuids_tag
            , details::valid_server_option_meta_type
        {};

        static constexpr std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* )
        {
            return begin;
        }
        /** @endcond */
    };

    namespace details {

        struct uuid_128_writer
        {
            constexpr uuid_128_writer( std::uint8_t*& b, std::uint8_t* e )
                : begin( b )
                , end( e )
            {
            }

            template< typename UUID >
            void each()
            {
                if ( begin + sizeof( UUID::bytes ) <= end )
                {
                    std::copy( std::begin( UUID::bytes ), std::end( UUID::bytes ), begin );
                    begin += sizeof( UUID::bytes );
                }
            }

            std::uint8_t*&      begin;
            std::uint8_t* const end;
        };
    }

    template < typename ... UUID128 >
    struct list_of_128_bit_service_uuids {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::list_of_128_bit_service_uuids_tag,
            details::valid_server_option_meta_type {};

        static std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* end )
        {
            static constexpr std::size_t uuid_size = 16;
            const std::size_t buffer_size = end - begin;

            if ( buffer_size < 2 + uuid_size )
                return begin;

            const std::size_t max_uuids = std::min( (buffer_size - 2) / uuid_size, sizeof ...(UUID128));

            *begin = 1 + uuid_size * max_uuids;
            ++begin;

            *begin = max_uuids == sizeof ...(UUID128)
                ? bits( details::gap_types::complete_service_uuids_128 )
                : bits( details::gap_types::incomplete_service_uuids_128 );
            ++begin;

            details::for_< UUID128... >::each( details::uuid_128_writer( begin, end ) );

            return begin;
        }
        /** @endcond */
    };

    /** @cond HIDDEN_SYMBOLS */
    template < typename ... UUID16 >
    struct list_of_128_bit_service_uuids< std::tuple< UUID16... > > : list_of_128_bit_service_uuids< UUID16... > {};

    template <>
    struct list_of_128_bit_service_uuids<> {
        struct meta_type :
            details::list_of_128_bit_service_uuids_tag,
            details::valid_server_option_meta_type {};

        static constexpr std::uint8_t* advertising_data( std::uint8_t* begin, std::uint8_t* ) {
            return begin;
        }
    };
    /** @endcond */

    namespace details {
        template < typename T >
        struct extract_uuid {
            using type = typename T::uuid;
        };

        template < typename Filter, typename ... Services >
        struct create_list_of_service_uuids
        {
            typedef typename bluetoe::details::transform_list<
                std::tuple< Services... >,
                extract_uuid >::type uuids;

            typedef typename find_all_by_meta_type< Filter, uuids >::type type;
        };

        template < typename ServiceList >
        struct default_list_of_16_bit_service_uuids;

        template < typename ... Services >
        struct default_list_of_16_bit_service_uuids< std::tuple< Services... > > : list_of_16_bit_service_uuids<
            typename create_list_of_service_uuids< bluetoe::details::service_uuid_16_meta_type, Services... >::type > {};

        template < typename ServiceList >
        struct default_list_of_128_bit_service_uuids;

        template < typename ... Services >
        struct default_list_of_128_bit_service_uuids< std::tuple< Services... > > : list_of_128_bit_service_uuids<
            typename create_list_of_service_uuids< bluetoe::details::service_uuid_128_meta_type, Services... >::type > {};
    }
}

#endif
