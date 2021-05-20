#ifndef BLUETOE_SERVICES_GATT_HPP
#define BLUETOE_SERVICES_GATT_HPP

#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/attribute_handle.hpp>

namespace bluetoe {

    namespace gatt {
        /**
         * @brief The assigned 16 bit UUID for the Generic Attribute Profile service
         */
        using service_uuid = service_uuid16< 0x1801 >;

        /**
         * @brief The assigned 16 bit UUID for Service Changed characteristic
         */
        using service_changed_uuid = characteristic_uuid16< 0x2A05 >;

        /**
         * @brief Service Changed characteristic
         */
        using service_changed_characteristic = characteristic<
            service_changed_uuid,
            indicate,
            no_read_access,
            fixed_uint32_value< 0xFFFF0001 >
        >;

        /**
         * @brief Generic Attribute Profile service with a single Service Changed characteristic
         *
         * As the handle of the Service Changed Value Attribute handle should be stable, equal
         * over all future versions of your server, this service should always be the first
         * service in the server. If that is not possible, use service_with_fixed_handles<> and
         * assign the attribute handles manually.
         */
        using service = ::bluetoe::service<
            service_uuid,
            service_changed_characteristic
        >;

        /**
         * @brief Generic Attribute Profile service with a single Service Changed characteristic
         *
         * This version allows manually allocating the attribute handles.
         */
        template <
            std::uint16_t ServiceHandle,
            std::uint16_t ServiceChangedCharDeclarationHandle = ServiceHandle + 1,
            std::uint16_t ServiceChangedCharValueHandle       = ServiceHandle + 2,
            std::uint16_t ServiceChangedCahrCCCDHandle        = ServiceHandle + 3 >
        using service_with_fixed_handles = ::bluetoe::service<
            attribute_handle< ServiceHandle >,
            service_uuid,
            characteristic<
                attribute_handles<
                    ServiceChangedCharDeclarationHandle,
                    ServiceChangedCharValueHandle,
                    ServiceChangedCahrCCCDHandle >,
                indicate,
                no_read_access,
                fixed_uint32_value< 0xFFFF0001 >
            >
        >;
    }
}

#endif

