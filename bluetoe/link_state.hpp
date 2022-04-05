#ifndef BLUETOE_LINK_STATE_HPP
#define BLUETOE_LINK_STATE_HPP

#include <bluetoe/pairing_status.hpp>
#include <bluetoe/codes.hpp>

namespace bluetoe {
namespace details {

    /**
     * @brief Attributes of a link
     *
     * Data that is required / provided by the link_layer or by any
     * l2cap layer service (ATT/SM).
     *
     * Currently, this state data is required by the link_layer,
     * the ATT layer (server.hpp) and by the security manager.
     */
    class link_state
    {
    public:
        link_state()
            : encrypted_( false )
            , pairing_status_( device_pairing_status::no_key )
        {
        }

        /**
         * @brief returns true, if the connection is currently encrypted
         */
        bool is_encrypted() const
        {
            return encrypted_;
        }

        /**
         * @brief set the current encryption status
         *
         * Returns true, if the value changed.
         */
        bool is_encrypted( bool encrypted )
        {
            const bool result = encrypted_ != encrypted;
            encrypted_ = encrypted;

            return result;
        }

        /**
         * @brief returns the pairing state of the local device with the remote device for this link
         */
        device_pairing_status pairing_status() const
        {
            return pairing_status_;
        }

        /**
         * @brief sets the pairing status of the current link / connection
         */
        void pairing_status( device_pairing_status status )
        {
            pairing_status_ = status;
        }

        /**
         * @brief returns the result of is_encrypted() and pairing_status() as tuple
         */
        connection_security_attributes security_attributes() const
        {
            return connection_security_attributes{ encrypted_, pairing_status_ };
        }

    private:
        bool                        encrypted_;
        device_pairing_status       pairing_status_;
    };

    /**
     * @brief in case, no security is implemented on the link layer
     */
    class link_state_no_security
    {
    public:
        bool is_encrypted() const
        {
            return false;
        }

        /**
         * @brief returns the pairing state of the local device with the remote device for this link
         */
        device_pairing_status pairing_status() const
        {
            return device_pairing_status::no_key;
        }

        /**
         * @brief returns the result of is_encrypted() and pairing_status() as tuple
         */
        connection_security_attributes security_attributes() const
        {
            return connection_security_attributes{ false, device_pairing_status::no_key };
        }
    };

}
}

#endif // include guard
