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

    }
}

#endif
