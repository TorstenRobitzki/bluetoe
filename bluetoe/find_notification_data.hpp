#ifndef BLUETOE_FIND_NOTIFICATION_DATA_HPP
#define BLUETOE_FIND_NOTIFICATION_DATA_HPP

#include <bluetoe/options.hpp>

namespace bluetoe {
namespace details {

    namespace impl {
        template < typename Characteristics, typename Pair >
        struct filter_characteristics_with_cccd
        {
            using type = typename select_type<
                Pair::characteristic_t::number_of_client_configs,
                typename add_type< Characteristics, Pair >::type,
                Characteristics
            >::type;
        };

        template < typename Characteristic, std::uint16_t Offset, int Prio >
        struct characteristic_with_service_attribute_offset
        {
            using characteristic_t = Characteristic;
            static constexpr std::uint16_t service_offset = Offset;
            static constexpr int           priority       = Prio;
        };

        struct preudo_first_char
        {
            static const std::size_t number_of_attributes = 0;
        };

        template < typename Characteristic, std::uint16_t FirstAttributesHandle, int Prio >
        struct characteristic_handle_pair
        {
            using characteristic_t = Characteristic;
            static constexpr std::uint16_t first_attribute_handle = FirstAttributesHandle;
            static constexpr int           priority               = Prio;
        };

        template < typename Characteristics, typename Characteristic >
        struct add_handle_to_characteristic
        {
            using last = typename last_type< Characteristics, impl::characteristic_handle_pair< impl::preudo_first_char, 1, 0 > >::type;

            static constexpr std::size_t attribute_handle = last::first_attribute_handle + last::characteristic_t::number_of_attributes + Characteristic::service_offset;

            using type = typename add_type<
                Characteristics,
                impl::characteristic_handle_pair< typename Characteristic::characteristic_t, attribute_handle, Characteristic::priority >
            >::type;
        };

        struct attribute_at
        {
            attribute_at( std::uint16_t& r, std::size_t i )
                : result( r )
                , index( i )
            {}

            template< typename O >
            void each()
            {
                if ( index == 0 )
                {
                    result = O::first_attribute_handle + 1;
                }

                --index;
            }

            std::uint16_t&  result;
            std::size_t     index;
        };

        template < typename A, typename B >
        struct order_by_prio
        {
            using type = std::integral_constant< bool, A::priority < B::priority >;
        };

        template < typename Characteristics, typename Characteristic >
        struct add_cccd_position
        {
            template < std::size_t CCCD, typename HandlePair >
            struct with_cccd_position : HandlePair
            {
                static constexpr std::size_t cccd_position = CCCD;
            };

            static constexpr std::size_t cccd_position = std::tuple_size< Characteristics >::value;

            using type = typename add_type<
                Characteristics,
                with_cccd_position< cccd_position, Characteristic >
            >::type;
        };

        template < typename Characteristics, typename Characteristic >
        struct add_cccd_handle
        {
            template < std::size_t CCCD, typename HandlePair >
            struct with_cccd_handle : HandlePair
            {
                static constexpr std::size_t cccd_handle = CCCD;
            };

            static constexpr std::size_t cccd_handle = std::tuple_size< Characteristics >::value;

            using type = typename add_type<
                Characteristics,
                with_cccd_handle< cccd_handle, Characteristic >
            >::type;
        };

        template < class Characteristic >
        struct select_cccd_position {
            using type = std::integral_constant< std::size_t, Characteristic::cccd_position >;
        };

    }

    template <
        typename Priorities,
        typename Services
    >
    struct find_notification_data_in_list
    {
        template < typename Characteristics, typename Service >
        struct characteristics_from_service
        {
            template < typename C >
            struct add_service_offset
            {
                using type = std::tuple<>;
            };

            // Add just to the first characteritic of a service the numer of attributes that are used to
            // model the service
            template < typename C, typename ...Cs >
            struct add_service_offset< std::tuple< C, Cs... > >
            {
                using type = std::tuple<
                    impl::characteristic_with_service_attribute_offset< C, Service::number_of_service_attributes, Priorities::template characteristic_priority< Services, Service, C >::value >,
                    impl::characteristic_with_service_attribute_offset< Cs, 0, Priorities::template characteristic_priority< Services, Service, Cs >::value >... >;
            };

            using type = typename add_type< Characteristics, typename add_service_offset< typename Service::characteristics >::type >::type;
        };

        using services                               = Services;
        using all_characteristics                    = typename fold_left< services, characteristics_from_service >::type;
        using characteristics_with_attribute_handles = typename fold_left< all_characteristics, impl::add_handle_to_characteristic >::type;
        using characteristics_only_with_cccd         = typename fold_left< characteristics_with_attribute_handles, impl::filter_characteristics_with_cccd >::type;
        using characteristics_with_cccd_position     = typename fold_left< characteristics_only_with_cccd, impl::add_cccd_position >::type;
        using characteristics_sorted_by_priority     = typename stable_sort< impl::order_by_prio, characteristics_with_cccd_position >::type;
        using characteristics_with_cccd_handle       = typename fold_left< characteristics_sorted_by_priority, impl::add_cccd_handle >::type;

        static notification_data find_notification_data_by_index( std::size_t index )
        {
            std::uint16_t attribute = 0;
            for_< characteristics_sorted_by_priority >::each( impl::attribute_at( attribute, index ) );

            return notification_data( attribute, index );
        }

        struct attribute_value
        {
            attribute_value( notification_data& r, const void* v )
                : result( r )
                , index( 0 )
                , value( v )
            {}

            template< typename O >
            void each()
            {
                if ( O::characteristic_t::value_type::is_this( value ) )
                    result = notification_data( O::first_attribute_handle + 1, index );

                ++index;
            }

            notification_data&  result;
            std::size_t         index;
            const void*         value;
        };

        static notification_data find_notification_data( const void* value )
        {
            notification_data result;
            for_< characteristics_only_with_cccd >::each( attribute_value( result, value ) );

            return result;
        }

        using cccd_indices = typename transform_list< characteristics_with_cccd_handle, impl::select_cccd_position >::type;
    };

    template <
        typename Priorities,
        typename Services,
        typename Characteristic
    >
    struct find_notification_by_uuid
    {
        using find = find_notification_data_in_list< Priorities, Services >;

        template < typename Other >
        struct equal_char
        {
            static constexpr bool value = std::is_same< typename Other::characteristic_t, Characteristic >::value;
        };

        using char_infos = typename find_if<
            typename find::characteristics_with_cccd_handle,
            equal_char >::type;

        static notification_data data()
        {
            return notification_data( char_infos::first_attribute_handle + 1, char_infos::cccd_handle );
        }
    };

}
}

#endif
