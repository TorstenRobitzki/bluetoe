#ifndef BLUETOE_LINK_LAYER_OPTIONS_HPP
#define BLUETOE_LINK_LAYER_OPTIONS_HPP

#include <bluetoe/link_layer/options.hpp>
#include <bluetoe/link_layer/address.hpp>


namespace bluetoe
{
namespace link_layer
{
    namespace details {
        struct advertising_interval_meta_type {};
        struct device_address_meta_type {};

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

    /**
     * @brief defines that the device will use a static random address.
     *
     * A static random address is an address that is random, but stays the same every time the device starts.
     * A static random address is the default.
     */
    struct random_static_address
    {
        static constexpr bool is_random()
        {
            return true;
        }

        template < class Radio >
        static address address( const Radio& r )
        {
            return address::generate_static_random_address( r.static_random_address_seed() );
        }

        typedef details::device_address_meta_type meta_type;
    };

    namespace details {
        struct sleep_clock_accuracy_meta_type {};

        template < unsigned long long SleepClockAccuracyPPM >
        struct check_sleep_clock_accuracy_ppm {
            static_assert( SleepClockAccuracyPPM <= 500, "The highest, possible sleep clock accuracy is 500ppm." );

            typedef void type;
        };

    }

    /**
     * @brief defines the sleep clock accuracy of the device hardware.
     *
     * The stack uses the accuracy information to keep the time window where the slave listens for radio messages
     * from the master, as small as possible. It's important to determine the real sleep clock accuracy.
     * Giving to large accuracy will leed to not optimal power consumption.
     * To small accuracy will leed to instable connections.
     */
    template < unsigned long long SleepClockAccuracyPPM, typename = typename details::check_sleep_clock_accuracy_ppm< SleepClockAccuracyPPM >::type >
    struct sleep_clock_accuracy_ppm
    {
        static constexpr unsigned accuracy_ppm = static_cast< unsigned >( SleepClockAccuracyPPM );
        typedef details::sleep_clock_accuracy_meta_type meta_type;
    };
}
}

#endif
