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
        static void btstack_packet_handler( std::uint8_t packet_type, std::uint16_t channel, std::uint8_t *packet, std::uint16_t size);

        static std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) > advertising_data_;
    };

    /*
     * implementation
     */
    template < typename Server >
    void btstack_libusb_device::run( Server& srv )
    {
        using std::placeholders::_1;
        using std::placeholders::_2;
        advertising_data_ = std::bind( &Server::advertising_data, std::ref( srv ), _1, _2 );

        init();
    }
};

#endif