#ifndef BLUETOE_META_TYPES_HPP
#define BLUETOE_META_TYPES_HPP

namespace bluetoe {

    namespace details {

        /*
         * A meta_type, that every option to a characteristic must have
         */
        struct valid_characteristic_option_meta_type {};

        /*
         * A meta_type that tags every valid option to a service
         */
        struct valid_service_option_meta_type {};

        /*
         * A meta_type that tags every valid option to a server
         */
        struct valid_server_option_meta_type {};

        /*
         * A service declaration
         */
        struct service_meta_type {};

        /*
         * A characteristic declaration
         */
        struct characteristic_meta_type {};

    }
}

#endif

