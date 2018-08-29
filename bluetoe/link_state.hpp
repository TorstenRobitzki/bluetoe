#ifndef BLUETOE_LINK_STATE_HPP
#define BLUETOE_LINK_STATE_HPP

#include <bluetoe/sm/pairing_status.hpp>

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
    template < class ATTState >
    class link_state : public ATTState
    {
    public:
        explicit link_state( std::uint16_t server_mtu );

        /**
         * @brief returns the negotiated MTU
         */
        std::uint16_t negotiated_mtu() const;

        /**
         * @brief sets the MTU size of the connected client.
         *
         * The default is 23. Usually this function will be called by the server implementation as reaction
         * of an "Exchange MTU Request".
         * @post client_mtu() == mtu
         */
        void client_mtu( std::uint16_t mtu );

        /**
         * @brief returns the client MTU
         *
         * By default this returns 23 unless the client MTU was changed by call to client_mtu( std::size_t )
         */
        std::uint16_t client_mtu() const;

        /**
         * @brief returns the MTU of this server as provided in the c'tor
         * @pre connection_data(X).server_mtu() == X
         */
        std::uint16_t server_mtu() const;

        /**
         * @brief returns true, if the connection is currently encrypted
         */
        bool is_entrypted() const;

        /**
         * @brief returns the pairing state of the local device with the remote device for this link
         */
        device_pairing_status pairing_status() const;

        /**
         * @brief sets the pairing status of the current link / connection
         */
        void pairing_status( device_pairing_status status );
    private:
        std::uint16_t               server_mtu_;
        std::uint16_t               client_mtu_;
        bool                        encrypted_;
        device_pairing_status       pairing_status_;
    };

    /** @cond HIDDEN_SYMBOLS */
    template < class ATTState >
    link_state< ATTState >::link_state( std::uint16_t server_mtu )
        : ATTState()
        , server_mtu_( server_mtu )
        , client_mtu_( details::default_att_mtu_size )
        , encrypted_( false )
        , pairing_status_( device_pairing_status::no_key )
    {
        assert( server_mtu >= details::default_att_mtu_size );
    }

    template < class ATTState >
    std::uint16_t link_state< ATTState >::negotiated_mtu() const
    {
        return std::min( server_mtu_, client_mtu_ );
    }

    template < class ATTState >
    void link_state< ATTState >::client_mtu( std::uint16_t mtu )
    {
        assert( mtu >= details::default_att_mtu_size );
        client_mtu_ = mtu;
    }

    template < class ATTState >
    std::uint16_t link_state< ATTState >::client_mtu() const
    {
        return client_mtu_;
    }

    template < class ATTState >
    std::uint16_t link_state< ATTState >::server_mtu() const
    {
        return server_mtu_;
    }

    template < class ATTState >
    bool link_state< ATTState >::is_entrypted() const
    {
        return encrypted_;
    }

    template < class ATTState >
    device_pairing_status link_state< ATTState >::pairing_status() const
    {
        return pairing_status_;
    }

    template < class ATTState >
    void link_state< ATTState >::pairing_status( device_pairing_status status )
    {
        pairing_status_ = status;
    }

    /** @endcond */
}
}

#endif // include guard
