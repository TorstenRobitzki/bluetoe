#ifndef BLUETOE_BINDINGS_BTSTACK_LIBUSB_HPP
#define BLUETOE_BINDINGS_BTSTACK_LIBUSB_HPP

#include <cstdint>
#include <functional>

namespace bluetoe {

    /**
     * @brief connects a server definition to a hardware to run on
     */
    struct btstack_libusb_device
    {
    public:
        template < typename Server >
        void run( Server& );

    private:
        static void init();
        static void btstack_packet_handler( std::uint8_t packet_type, std::uint8_t *packet, std::uint16_t size );
        static void handle_event( std::uint8_t event_code, std::uint8_t param_size, const std::uint8_t *parameters );

        static std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) > advertising_data_;
        static std::function< void( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size ) > l2cap_input_;
    };

    /*
     * implementation
     */
    template < typename Server >
    void btstack_libusb_device::run( Server& srv )
    {
        using namespace std::placeholders;

        advertising_data_ = std::bind( &Server::advertising_data, std::ref( srv ), _1, _2 );
        l2cap_input_      = std::bind( &Server::l2cap_input, std::ref( srv ), _1, _2, _3, _4 );

        init();
    }
};

#endif