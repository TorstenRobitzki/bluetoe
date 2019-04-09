#ifndef BLUETOE_SM_PAIRING_STATUS_HPP
#define BLUETOE_SM_PAIRING_STATUS_HPP

namespace bluetoe {

    /**
     * @brief pairing status of a given connection
     *
     * As enumerated as "Local Device Pairing Status" in table 10.2, Vol. 3; Part C; 10.3.1
     */
    enum class device_pairing_status {
        no_key,
        unauthenticated_key,
        authenticated_key,
        authenticated_key_with_secure_connection
    };
}
#endif
