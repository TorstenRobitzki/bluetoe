#ifndef BLUETOE_SERVICES_HID_HPP
#define BLUETOE_SERVICES_HID_HPP

namespace bluetoe {

    namespace hid {
        /**
         * The assigned 16 bit UUID for the Human Interface Device
         */
        using service_uuid = service_uuid16< 0x1812 >;

        using protocol_mode_uuid                = characteristic_uuid16< 0x2A4E >;
        using report_uuid                       = characteristic_uuid16< 0x2A4D >;
        using report_map_uuid                   = characteristic_uuid16< 0x2A4B >;
        using boot_keyboard_input_report_uuid   = characteristic_uuid16< 0x2A22 >;
        using boot_keyboard_output_report_uuid  = characteristic_uuid16< 0x2A32 >;
        using boot_mouse_input_report_uuid      = characteristic_uuid16< 0x2A33 >;
        using hid_information_uuid              = characteristic_uuid16< 0x2A4A >;
        using hid_control_point_uuid            = characteristic_uuid16< 0x2A4C >;
    }
}

#endif
