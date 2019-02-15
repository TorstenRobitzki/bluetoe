#ifndef BLUETOE_SERVICES_DIS_HPP
#define BLUETOE_SERVICES_DIS_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>

namespace bluetoe {

    namespace dis {

        /**
         * The assigned 16 bit UUID for the Device Information Service
         */
        using service_uuid = service_uuid16< 0x180A >;

        using manufacturer_name_uuid    = characteristic_uuid16< 0x2A29 >;
        using model_number_uuid         = characteristic_uuid16< 0x2A24 >;
        using serial_number_uuid        = characteristic_uuid16< 0x2A25 >;
        using hardware_revision_uuid    = characteristic_uuid16< 0x2A27 >;
        using firmware_revision_uuid    = characteristic_uuid16< 0x2A26 >;
        using software_revision_uuid    = characteristic_uuid16< 0x2A28 >;
        using system_uuid               = characteristic_uuid16< 0x2A23 >;
        using ieee_11073_20601_regulatory_certification_data_list_uuid = characteristic_uuid16< 0x2A2A >;
        using pnp_uuid                  = characteristic_uuid16< 0x2A50 >;

        /**
         * @brief includes a Manufacturer Name String characteristic into the Device Information Service.
         */
        template < const char* Manufacturer >
        using manufacturer_name =
            bluetoe::characteristic<
                bluetoe::cstring_value< Manufacturer >,
                manufacturer_name_uuid
            >
        ;

        /**
         * @brief includes a Model Number String characteristic into the Device Information Service.
         */
        template < const char* ModelNumberString >
        using model_number =
            bluetoe::characteristic<
                bluetoe::cstring_value< ModelNumberString >,
                model_number_uuid
            >
        ;

        /**
         * @brief includes a Serial Number String characteristic into the Device Information Service.
         */
        template < const char* SerialNumberString >
        using serial_number =
            bluetoe::characteristic<
                bluetoe::cstring_value< SerialNumberString >,
                serial_number_uuid
            >
        ;

        /**
         * @brief includes a Hardware Revision String characteristic into the Device Information Service.
         */
        template < const char* HardwareRevisionString >
        using hardware_revision =
            bluetoe::characteristic<
                bluetoe::cstring_value< HardwareRevisionString >,
                hardware_revision_uuid
            >
        ;

        /**
         * @brief includes a Firmware Revision String characteristic into the Device Information Service.
         */
        template < const char* FirmwareRevisionString >
        using firmware_revision =
            bluetoe::characteristic<
                bluetoe::cstring_value< FirmwareRevisionString >,
                firmware_revision_uuid
            >
        ;

        /**
         * @brief includes a Software Revision String characteristic into the Device Information Service.
         */
        template < const char* SoftwareRevisionString >
        using software_revision =
            bluetoe::characteristic<
                bluetoe::cstring_value< SoftwareRevisionString >,
                software_revision_uuid
            >
        ;

        /**
         * @TODO not implemented
         */
        template < std::uint64_t ManufacturerIdentifier, std::uint32_t OrganizationallyUniqueIdentifier >
        struct system_id
        {
        };

        /**
         * @TODO not implemeted
         */
        template < typename Value >
        struct ieee_11073_20601_regulatory_certification_data_list
        {
        };

        /**
         * @brief enumeration that defines, the valid domain of a used vendor ID
         */
        enum class vendor_id_source_t : uint8_t
        {
            /**
             * Bluetooth SIG assigned Company Identifier value from the Assigned Numbers document
             */
            bluetooth   = 1,

            /**
             * USB Implementer’s Forum assigned Vendor ID value
             */
            usb         = 2
        };

        /**
         * @TODO not implemeted
         */
        template < vendor_id_source_t VendorIDSource, std::uint16_t VendorID, std::uint16_t ProductID, std::uint16_t ProductVersion >
        struct pnp_id
        {
        };
    }

    /**
     * @brief implementation of a Device Information Service (DIS) 1.1
     *
     * @sa dis::manufacturer_name
     * @sa dis::model_number
     * @sa dis::serial_number
     * @sa dis::hardware_revision
     * @sa dis::firmware_revision
     * @sa dis::software_revision
     * @sa dis::system_id
     * @sa dis::ieee_11073_20601_regulatory_certification_data_list
     * @sa dis::pnp_id
     */
    template < typename ... Options >
    using device_information_service = bluetoe::service<
        dis::service_uuid,
        Options...
    >;
}

#endif
