#ifndef BLUETOE_SERVER_NAME_HPP
#define BLUETOE_SERVER_NAME_HPP

#include <meta_types.hpp>

namespace bluetoe {

    namespace details {
        struct server_name_meta_type {};
    }

    /**
     * @brief adds a discoverable device name
     */
    template < const char* const Name >
    struct server_name {
        /** @cond HIDDEN_SYMBOLS */
        struct meta_type :
            details::server_name_meta_type,
            details::valid_server_option_meta_type {};

        static constexpr char const* name = Name;

        static constexpr const char* value()
        {
            return name;
        }
        /** @endcond */
    };

}

#endif
