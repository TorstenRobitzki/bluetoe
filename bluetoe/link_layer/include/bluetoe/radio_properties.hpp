#ifndef BLUETOE_LINK_LAYER_RADIO_PROPERTIES_HPP
#define BLUETOE_LINK_LAYER_RADIO_PROPERTIES_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief Value type that allows the serialization of the compile time
     *        properties of scheduled_radio implementations
     */
    struct radio_properties
    {
        radio_properties() {}

        template < class Radio >
        explicit radio_properties( const Radio& );

        bool hardware_supports_encryption;
        bool hardware_supports_lesc_pairing;
        bool hardware_supports_legacy_pairing;
        bool hardware_supports_2mbit;
        bool hardware_supports_synchronized_user_timer;
        std::uint32_t radio_max_supported_payload_length;
        std::uint32_t sleep_time_accuracy_ppm;
    };

    // implementation
    template < class Radio >
    radio_properties::radio_properties( const Radio& )
        : hardware_supports_encryption( Radio::hardware_supports_encryption )
        , hardware_supports_lesc_pairing( Radio::hardware_supports_lesc_pairing )
        , hardware_supports_legacy_pairing( Radio::hardware_supports_legacy_pairing )
        , hardware_supports_2mbit( Radio::hardware_supports_2mbit )
        , hardware_supports_synchronized_user_timer( Radio::hardware_supports_synchronized_user_timer )
        , radio_max_supported_payload_length( Radio::radio_max_supported_payload_length )
        , sleep_time_accuracy_ppm( Radio::sleep_time_accuracy_ppm )
    {
    }

}
}
#endif
