#include <bluetoe/radio_properties.hpp>

#include <ostream>

namespace bluetoe {
namespace link_layer {

    std::ostream& operator<<( std::ostream& out, const radio_properties& props )
    {
        out << "supports enryption: " << props.hardware_supports_encryption
            << "\nsupports LESC: " << props.hardware_supports_lesc_pairing
            << "\nsupports legacy pairing: " << props.hardware_supports_legacy_pairing
            << "\nsupports 2Mbit: " << props.hardware_supports_2mbit
            << "\nsyn. user timer: " << props.hardware_supports_synchronized_user_timer
            << "\nmax. supported payload length: " << props.radio_max_supported_payload_length
            << "\nsleep timer accuracy [ppm]: " << props.sleep_time_accuracy_ppm;

        return out;
    }

}
}