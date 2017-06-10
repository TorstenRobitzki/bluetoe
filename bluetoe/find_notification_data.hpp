#ifndef BLUETOE_FIND_NOTIFICATION_DATA_HPP
#define BLUETOE_FIND_NOTIFICATION_DATA_HPP

namespace bluetoe {
namespace details {


    template <
        typename Priorities,
        typename Services
    >
    struct find_notification_data_in_list
    {
        template < typename Characteristics, typename Pair >
        struct filter_characteristics_with_cccd
        {
            using type = typename select_type<
                Pair::characteristic_t::number_of_client_configs,
                typename add_type< Characteristics, Pair >::type,
                Characteristics
            >::type;
        };

        template < typename Characteristic, std::uint16_t Offfset >
        struct characteristic_with_service_attribute_offset
        {
            using characteristic_t = Characteristic;
            static constexpr std::uint16_t service_offset = Offfset;
        };

        template < typename Characteristics, typename Service >
        struct characteristics_from_service
        {
            template < typename I >
            struct add_service_offset;

            // Add just to the first characteritic of a service the numer of attributes that are used to
            // model the service
            template < typename I, typename ...Is >
            struct add_service_offset< std::tuple< I, Is... > >
            {
                using type = std::tuple<
                    characteristic_with_service_attribute_offset< I, Service::number_of_service_attributes >,
                    characteristic_with_service_attribute_offset< Is, 0 >... >;
            };

            using type = typename add_type< Characteristics, typename add_service_offset< typename Service::characteristics >::type >::type;
        };

        struct preudo_first_char
        {
            static const std::size_t number_of_attributes = 0;
        };

        template < typename Characteristic, std::uint16_t FirstAttributesHandle >
        struct characteristic_handle_pair
        {
            using characteristic_t = Characteristic;
            static constexpr std::uint16_t first_attribute_handle = FirstAttributesHandle;
        };

        template < typename Characteristics, typename Characteristic >
        struct add_handle_to_characteristic
        {
            using last = typename last_type< Characteristics, characteristic_handle_pair< preudo_first_char, 1 > >::type;

            static const std::size_t attribute_handle = last::first_attribute_handle + last::characteristic_t::number_of_attributes + Characteristic::service_offset;

            using type = typename add_type< Characteristics, characteristic_handle_pair< typename Characteristic::characteristic_t, attribute_handle > >::type;
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

        using services                               = Services;
        using all_characteristics                    = typename fold_left< services, characteristics_from_service >::type;
        using characteristics_with_attribute_handles = typename fold_left< all_characteristics, add_handle_to_characteristic >::type;
        using characteristics_only_with_cccd         = typename fold_left< characteristics_with_attribute_handles, filter_characteristics_with_cccd >::type;

        static notification_data find_notification_data_by_index( std::size_t index )
        {
            std::uint16_t attribute = 0;
            for_< characteristics_only_with_cccd >::each( attribute_at( attribute, index ) );

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

    };

}
}

#endif
