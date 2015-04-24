#ifndef BLUETOE_BINDINGS_BTSTACK_LIBUSB_HPP
#define BLUETOE_BINDINGS_BTSTACK_LIBUSB_HPP

#include <cstdint>
#include <functional>

namespace bluetoe {

    class btstack_libusb_device_base
    {
    public:
        static void init();
        static void btstack_packet_handler( std::uint8_t packet_type, std::uint8_t *packet, std::uint16_t size );
        static void handle_event( std::uint8_t event_code, std::uint8_t param_size, const std::uint8_t *parameters );
        static void send_acl_package( std::uint8_t* buffer, std::size_t size );
        static void exchange_mtu_request( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        static std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) > advertising_data_;
        static std::function< void( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size ) > l2cap_input_;
        static std::uint16_t connection_handle_;
        static std::uint16_t mtu_size_;
    };

    /**
     * @brief connects a server definition to a hardware to run on
     *
     * @attention this is currently a hack; basically for testing.
     */
    template < typename Server >
    class btstack_libusb_device : btstack_libusb_device_base
    {
    public:
        btstack_libusb_device();

        void run( Server& );

    private:
        typename Server::connection_data connection_;
    };

    /*
     * implementation
     */
    template < typename Server >
    btstack_libusb_device< Server >::btstack_libusb_device()
        : connection_( btstack_libusb_device_base::mtu_size_ )
    {
    }

    template < typename Server >
    void btstack_libusb_device< Server >::run( Server& srv )
    {
        using namespace std::placeholders;

        advertising_data_ = std::bind( &Server::advertising_data, std::ref( srv ), _1, _2 );
        l2cap_input_      = std::bind( &Server::l2cap_input, std::ref( srv ), _1, _2, _3, _4, std::ref( connection_ ) );

        init();
    }
};

#endif