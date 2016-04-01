#ifndef BLUETOE_SERVER_NAME_HPP
#define BLUETOE_SERVER_NAME_HPP

namespace bluetoe {

    namespace details {
        struct server_name_meta_type;
    }

    /**
     * @brief adds a discoverable device name
     */
    template < const char* const Name >
    struct server_name {
        /** @cond HIDDEN_SYMBOLS */
        typedef details::server_name_meta_type meta_type;

        static constexpr char const* name = Name;

        static const char* value()
        {
            return name;
        }
        /** @endcond */
    };

}

#endif
