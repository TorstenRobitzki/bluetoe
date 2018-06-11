#ifndef BLUETOE_SENSOR_LOCATION_HPP
#define BLUETOE_SENSOR_LOCATION_HPP

namespace bluetoe {

    namespace details {
        struct sensor_location_meta_type {};
    }

    /**
     * @brief type to keep a bluetoe::location value
     */
    template < std::uint8_t L >
    struct sensor_location_tag
    {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::sensor_location_meta_type,
            details::valid_service_option_meta_type {};

        static constexpr std::uint8_t                 value = L;
        /** @endcond */
    };

    /**
     * @brief enumeration of sensor_location (org.bluetooth.characteristic.sensor_location)
     *
     * The class contains a lot of alias-declarations for known, assigned sensor locations.
     */
    struct sensor_location
    {
        /// Other
        using other                 = sensor_location_tag< 0 >;
        /// Top of shoe
        using top_of_shoe           = sensor_location_tag< 1 >;
        /// In shoe
        using in_shoe               = sensor_location_tag< 2 >;
        /// Hip
        using hip                   = sensor_location_tag< 3 >;
        /// Front Wheel
        using front_wheel           = sensor_location_tag< 4 >;
        /// Left Crank
        using left_crank            = sensor_location_tag< 5 >;
        /// Right Crank
        using right_crank           = sensor_location_tag< 6 >;
        /// Left Pedal
        using left_pedal            = sensor_location_tag< 7 >;
        /// Right Pedal
        using right_pedal           = sensor_location_tag< 8 >;
        /// Front Hub
        using front_hub             = sensor_location_tag< 9 >;
        /// Rear Dropout
        using rear_dropout          = sensor_location_tag< 10 >;
        /// Chainstay
        using chainstay             = sensor_location_tag< 11 >;
        /// Rear Wheel
        using rear_wheel            = sensor_location_tag< 12 >;
        /// Rear Hub
        using rear_hub              = sensor_location_tag< 13 >;
        /// Chest
        using chest                 = sensor_location_tag< 14 >;
    };
}

#endif
