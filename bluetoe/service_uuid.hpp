#ifndef BLUETOE_SERVICE_UUID_HPP
#define BLUETOE_SERVICE_UUID_HPP

namespace bluetoe {
namespace details {
    struct service_uuid_meta_type {};
    struct service_uuid_16_meta_type : service_uuid_meta_type {};
    struct service_uuid_128_meta_type : service_uuid_meta_type {};
}
}

#endif // include guard
