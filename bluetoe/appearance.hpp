#ifndef BLUETOE_APPEARANCE_HPP
#define BLUETOE_APPEARANCE_HPP

namespace bluetoe {

    namespace details {
        struct device_appearance_meta_type {};
    }

    /**
     * @brief type to keep a bluetoe::appearance value
     */
    template < std::uint16_t A >
    struct device_appearance
    {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::device_appearance_meta_type    meta_type;
        static constexpr std::uint16_t                  value = A;
        /** @endcond */
    };

    /**
         * @brief enumeration of appearances (org.bluetooth.characteristic.gap.appearance)
     *
     * The class contains a lot of alias-declarations for known, assigned device appearances.
     * Pass one of the types to the server definition, to define the appearance to the GATT client.
     *
     * This will define, the GAP Appearance Characteristic value of the GAP service for GATT server.
     *
     * @code
    unsigned temperature_value = 0;

    typedef bluetoe::server<
        bluetoe::service<
            bluetoe::appearance::thermometer
            ...
        >
    > small_temperature_service;
     * @endcode
     * @sa server
     */
    struct appearance
    {
        /// Unknown
        using unknown                                 = device_appearance< 0x0000 >;
        /// Generic Phone
        using phone                                   = device_appearance< 0x0040 >;
        /// Generic Computer
        using computer                                = device_appearance< 0x0080 >;
        /// Generic Watch
        using watch                                   = device_appearance< 0x00c0 >;
        /// Watch: Sports Watch
        using sports_watch                            = device_appearance< 0x00c1 >;
        /// Generic Clock
        using clock                                   = device_appearance< 0x0100 >;
        /// Generic Display
        using display                                 = device_appearance< 0x0140 >;
        /// Generic Remote Control
        using remote_control                          = device_appearance< 0x0180 >;
        /// Generic Eye-glasses
        using eye_glasses                             = device_appearance< 0x01c0 >;
        /// Generic Tag
        using tag                                     = device_appearance< 0x0200 >;
        /// Generic Keyring
        using keyring                                 = device_appearance< 0x0240 >;
        /// Generic Media Player
        using media_player                            = device_appearance< 0x0280 >;
        /// Generic Barcode Scanner
        using barcode_scanner                         = device_appearance< 0x02c0 >;
        /// Generic Thermometer
        using thermometer                             = device_appearance< 0x0300 >;
        /// Thermometer: Ear
        using ear_thermometer                         = device_appearance< 0x0301 >;
        /// Generic Heart rate Sensor
        using heart_rate_sensor                       = device_appearance< 0x0340 >;
        /// Heart Rate Sensor: Heart Rate Belt
        using heart_rate_belt                         = device_appearance< 0x0341 >;
        /// Generic Blood Pressure
        using blood_pressure                          = device_appearance< 0x0380 >;
        /// Blood Pressure: Arm
        using blood_pressure_arm                      = device_appearance< 0x0381 >;
        /// Blood Pressure: Wrist
        using blood_pressure_wrist                    = device_appearance< 0x0382 >;
        /// Human Interface Device (HID)
        using human_interface_device                  = device_appearance< 0x03c0 >;
        /// Keyboard (HID subtype)
        using keyboard                                = device_appearance< 0x03c1 >;
        /// Mouse (HID subtype)
        using mouse                                   = device_appearance< 0x03c2 >;
        /// Joystick (HID subtype)
        using joystick                                = device_appearance< 0x03c3 >;
        /// Gamepad (HID subtype)
        using gamepad                                 = device_appearance< 0x03c4 >;
        /// Digitizer Tablet (HID subtype)
        using digitizer_tablet                        = device_appearance< 0x03c5 >;
        /// Card Reader (HID subtype)
        using card_reader                             = device_appearance< 0x03c6 >;
        /// Digital Pen (HID subtype)
        using digital_pen                             = device_appearance< 0x03c7 >;
        /// Barcode Scanner (HID subtype)
        using hid_barcode_scanner                     = device_appearance< 0x03c8 >;
        /// Generic Glucose Meter
        using glucose_meter                           = device_appearance< 0x0400 >;
        /// Generic: Running Walking Sensor
        using running_walking_sensor                  = device_appearance< 0x0440 >;
        /// Running Walking Sensor: In-Shoe
        using in_shoe_running_walking_sensor          = device_appearance< 0x0441 >;
        /// Running Walking Sensor: On-Shoe
        using on_shoe_running_walking_sensor          = device_appearance< 0x0442 >;
        /// Running Walking Sensor: On-Hip
        using on_hip_running_walking_sensor           = device_appearance< 0x0443 >;
        /// Generic: Cycling
        using cycling                                 = device_appearance< 0x0480 >;
        /// Cycling: Cycling Computer
        using cycling_computer                        = device_appearance< 0x0481 >;
        /// Cycling: Speed Sensor
        using cycling_speed_sensor                    = device_appearance< 0x0482 >;
        /// Cycling: Cadence Sensor
        using cycling_cadence_sensor                  = device_appearance< 0x0483 >;
        /// Cycling: Power Sensor
        using cycling_power_sensor                    = device_appearance< 0x0484 >;
        /// Cycling: Speed and Cadence Sensor
        using cycling_speed_and_cadence_sensor        = device_appearance< 0x0485 >;
        /// Generic: Pulse Oximeter
        using pulse_oximeter                          = device_appearance< 0x0c40 >;
        /// Fingertip (Pulse Oximeter)
        using fingertip_pulse_oximeter                = device_appearance< 0x0c41 >;
        /// Wrist Worn (Pulse Oximeter)
        using wrist_worn_pulse_oximeter               = device_appearance< 0x0c42 >;
        /// Generic: Weight Scale
        using weight_scale                            = device_appearance< 0x0c80 >;
        /// Generic: Outdoor Sports Activity
        using outdoor_sports_activity                 = device_appearance< 0x1440 >;
        /// Location Display Device
        using location_display_device                 = device_appearance< 0x1441 >;
        /// Location and Navigation Display Device
        using location_and_navigation_display_device  = device_appearance< 0x1442 >;
        /// Location Pod
        using location_pod                            = device_appearance< 0x1443 >;
        /// Location and Navigation Pod
        using location_and_navigation_pod             = device_appearance< 0x1444 >;
    };


}

#endif // include guard
