#ifndef BLUETOE_LINK_LAYER_OPTIONS_HPP
#define BLUETOE_LINK_LAYER_OPTIONS_HPP

#include <bluetoe/link_layer/options.hpp>

namespace bluetoe
{
namespace link_layer
{
    namespace details {
        struct advertising_interval_meta_type {};

        template < unsigned long long AdvertisingIntervalMilliSeconds >
        struct check_advertising_interval_parameter {
            static_assert( AdvertisingIntervalMilliSeconds >= 20,    "the advertising interval must be greater than or equal to 20ms." );
            static_assert( AdvertisingIntervalMilliSeconds <= 10240, "the advertising interval must be greater than or equal to 20ms." );

            typedef void type;
        };

    }

    /**
     * @brief advertising interval in ms in the range 20ms to 10.24s
     */
    template < std::uint16_t AdvertisingIntervalMilliSeconds, typename = typename details::check_advertising_interval_parameter< AdvertisingIntervalMilliSeconds >::type >
    struct advertising_interval
    {
        typedef details::advertising_interval_meta_type meta_type;

        static constexpr delta_time interval() {
            // timeout in ms roundet to the next 0.625ms
            return delta_time( AdvertisingIntervalMilliSeconds * 1000 );
        }
    };
}
}

#endif
