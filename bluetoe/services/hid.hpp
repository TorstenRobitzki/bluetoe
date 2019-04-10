#ifndef BLUETOE_SERVICES_HID_HPP
#define BLUETOE_SERVICES_HID_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

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

        static constexpr std::uint16_t report_reference_descriptor_uuid = 0x2908;

        enum class report_type : std::uint8_t {
            input = 1,
            output = 2,
            feature = 3
        };

        namespace details {
            template < std::uint8_t ReportID, report_type Type >
            struct report_reference_impl {
                static const std::uint8_t data[ 2 ];

                using descriptor = bluetoe::descriptor< report_reference_descriptor_uuid, data, sizeof( data ) >;
            };

            template < std::uint8_t ReportID, report_type Type >
            const std::uint8_t report_reference_impl< ReportID, Type >::data[ 2 ] = { ReportID, static_cast< std::uint8_t >( Type ) };

        }

        /**
         * @brief an Input Report Reference Descriptor
         */
        template < std::uint8_t ReportID >
        using input_report_reference = typename details::report_reference_impl< ReportID, report_type::input >::descriptor;

        /**
         * @brief an Output Report Reference Descriptor
         */
        template < std::uint8_t ReportID >
        using output_report_reference = typename details::report_reference_impl< ReportID, report_type::output >::descriptor;

        /**
         * @brief a Feature Report Reference Descriptor
         */
        template < std::uint8_t ReportID >
        using feature_report_reference = typename details::report_reference_impl< ReportID, report_type::feature >::descriptor;
    }
}

#endif
