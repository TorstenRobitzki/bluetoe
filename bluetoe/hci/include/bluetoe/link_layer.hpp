#ifndef BLUETOE_HCI_LINK_LAYER_HPP
#define BLUETOE_HCI_LINK_LAYER_HPP

#include <bluetoe/address.hpp>

namespace bluetoe {
namespace hci {

    /**
     * @brief link layer implementation based on HCI
     */
    template <
        class Server,
        template < typename >
        class Transport,
        typename ... Options
    >
    class link_layer : Transport< link_layer< Server, Transport, Options... > >
    {
    public:
        /**
         * @brief this function passes the CPU to the link layer implementation
         *
         * This function should return on certain events to alow user code to do
         * usefull things. Details depend on the ScheduleRadio implemention.
         */
        void run( Server& );

        /**
         * @brief initiating the change of communication parameters of an established connection
         *
         * If it was not possible to initiate the connection parameter update, the function returns false.
         * @todo Add parameter that identifies the connection.
         */
        bool connection_parameter_update_request( std::uint16_t interval_min, std::uint16_t interval_max, std::uint16_t latency, std::uint16_t timeout );

        /**
         * @brief terminates the give connection
         *
         * @todo Add parameter that identifies the connection.
         */
        void disconnect();

        /**
         * @brief fills the given buffer with l2cap advertising payload
         */
        std::size_t fill_l2cap_advertising_data( std::uint8_t* buffer, std::size_t buffer_size ) const;

        /**
         * @brief returns the own local device address
         */
        const bluetoe::link_layer::device_address& local_address() const;
    private:
    };

    // implementation
    template < class Server, template < typename > class Transport, typename ... Options >
    void link_layer< Server, Transport, Options... >::run( Server& )
    {
    }
}
}

#endif
