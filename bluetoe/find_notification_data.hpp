#ifndef BLUETOE_FIND_NOTIFICATION_DATA_HPP
#define BLUETOE_FIND_NOTIFICATION_DATA_HPP

namespace bluetoe {
namespace details {

    /*
     * similar to the algorithm above, but this time the number of attributes is summed up.
     */
    template < typename T >
    struct find_notification_data_in_list;

    template <>
    struct find_notification_data_in_list< std::tuple<> >
    {
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data( const void* )
        {
            return notification_data();
        }

        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data_by_index( std::size_t )
        {
            return notification_data();
        }
    };

    template <
        typename T,
        typename ...Ts
    >
    struct find_notification_data_in_list< std::tuple< T, Ts... > >
    {
        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data( const void* value )
        {
            notification_data result = T::template find_notification_data< FirstAttributesHandle, ClientCharacteristicIndex >( value );

            if ( !result.valid() )
            {
                typedef find_notification_data_in_list< std::tuple< Ts... > > next;
                result = next::template find_notification_data<
                    FirstAttributesHandle + T::number_of_attributes,
                    ClientCharacteristicIndex + T::number_of_client_configs >( value );
            }

            return result;
        }

        template < std::size_t FirstAttributesHandle, std::size_t ClientCharacteristicIndex >
        static notification_data find_notification_data_by_index( std::size_t index )
        {
            notification_data result = T::template find_notification_data_by_index< FirstAttributesHandle, ClientCharacteristicIndex >( index );

            if ( !result.valid() )
            {
                typedef find_notification_data_in_list< std::tuple< Ts... > > next;
                result = next::template find_notification_data_by_index<
                    FirstAttributesHandle + T::number_of_attributes,
                    ClientCharacteristicIndex + T::number_of_client_configs >( index );
            }

            return result;
        }
    };

}
}

#endif
