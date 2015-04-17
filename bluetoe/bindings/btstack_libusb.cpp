#include "btstack_libusb.hpp"
#include "btstack-config.h"

#include <btstack/run_loop.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"
#include <btstack/hci_cmds.h>
#include <cstdint>

namespace bluetoe {

    extern "C" void sigint_handler(int param)
    {
        log_info(" <= SIGINT received, shutting down..\n");
        hci_power_control(HCI_POWER_OFF);
        hci_close();
        log_info("Good bye, see you.\n");
        exit(0);
    }

    void btstack_libusb_device::btstack_packet_handler( std::uint8_t packet_type, std::uint8_t *packet, std::uint16_t size)
    {
        log_info( "btstack_packet_handler: %i size: %lu\n", packet_type, size );
        hexdump( packet, size );

        static int init_phase = 0;

        if ( hci_can_send_command_packet_now() && init_phase < 100 )
        {
            if ( init_phase == 0 )
            {
                std::uint8_t      adv_data[ 31 ] = { 0 };
                const std::size_t size           = advertising_data_( adv_data, sizeof( adv_data ) );

                hci_send_cmd( &hci_le_set_advertising_data, size, adv_data );
            }
            else if ( init_phase == 1 )
            {
                hci_send_cmd( &hci_le_set_advertise_enable, 1 );
            }

            ++init_phase;
        }
    }

    void btstack_libusb_device::init()
    {
        /// GET STARTED with BTstack ///
        btstack_memory_init();
        run_loop_init(RUN_LOOP_POSIX);

        hci_dump_open( nullptr, HCI_DUMP_STDOUT );

        // init HCI
        hci_init( hci_transport_usb_instance(), nullptr, nullptr, nullptr );

        // handle CTRL-c
        signal(SIGINT, sigint_handler);

        // setup app
        hci_register_packet_handler( &btstack_libusb_device::btstack_packet_handler );
        hci_connectable_control(0); // no services yet

        // turn on!
        hci_power_control(HCI_POWER_ON);

        // go
        run_loop_execute();
    }

    std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) > btstack_libusb_device::advertising_data_;

}
