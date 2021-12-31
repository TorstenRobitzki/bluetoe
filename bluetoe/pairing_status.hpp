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

    /**
     * @brief the basic security attributes of a connection
     *
     * @sa device_pairing_status
     */
    struct connection_security_attributes
    {
        /**
         * @brief true, if the connection is currently encrypted
         */
        bool                    is_encrypted;

        /**
         * @brief method that was used to exchange the long term key that is used in the connection
         */
        device_pairing_status   pairing_status;

        /**
         * @brief default: not encrypted, no key
         */
        constexpr connection_security_attributes()
            : is_encrypted( false )
            , pairing_status( device_pairing_status::no_key )
        {
        }

        /**
         * @brief c'tor to initialize both members
         */
        constexpr connection_security_attributes( bool encrypted, device_pairing_status status )
            : is_encrypted( encrypted )
            , pairing_status( status )
        {
        }
    };
}
#endif
