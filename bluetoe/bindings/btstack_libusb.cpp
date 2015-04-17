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

    static std::uint16_t read_u16( std::uint8_t const * data )
    {
        return data[ 0 ] | ( data[ 1 ] << 8 );
    }

    void btstack_libusb_device::btstack_packet_handler( std::uint8_t packet_type, std::uint8_t *packet, std::uint16_t size)
    {
        bool no_log = false;

        switch (packet_type) {
            case HCI_EVENT_PACKET:
                log_info( "*HCI_EVENT_PACKET: %i; size: %lu", packet_type, size );

                if ( size >= 2 )
                {
                    const std::uint8_t event_code       = packet[ 0 ];
                    const std::uint8_t parameter_length = packet[ 1 ];
                    packet += 2;

                    if ( parameter_length  + 2 == size )
                    {
                        handle_event( event_code, parameter_length, packet );
                    }
                    else
                    {
                        log_error( "!!unresonable HCI_EVENT_parameter_length: %i; size: %lu plength: %lu", packet_type, size, parameter_length );
                    }
                }
                else
                {
                    log_error( "!!unresonable HCI_EVENT_SIZE: %i; size: %lu", packet_type, size);
                }

                break;
            case HCI_ACL_DATA_PACKET:
                log_info( "*HCI_ACL_DATA_PACKET: %i; size: %lu", packet_type, size );

                if ( size > 8 )
                {
                    hexdump( packet, size );
                    const std::uint16_t l2cap_length     = read_u16( packet + 4 );
                    const std::uint16_t l2cap_channel_id = read_u16( packet + 6 );

                    if ( l2cap_channel_id == 0x0004 && l2cap_length == size - 8 )
                    {
                        size    -= 8;
                        packet  += 8;
                        log_info( "ATT-Command: %i", size );
                        hexdump( packet, size ), no_log = true;

                        std::uint8_t out_buffer[ 23 ];
                        std::size_t  out_buffer_size = 23;

                        l2cap_input_( packet, size, out_buffer, out_buffer_size );
                    }
                }
                // 0x40, 0x20, 0x07, 0x00, 0x03, 0x00, 0x04, 0x00, 0x02, 0x9E, 0x00,
                // l2cap_acl_handler(packet, size);
                break;
            default:
                break;
        }

        if ( !no_log )
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

    enum class hic_event_code {
        le_meta_event = 0x3e,
        le_connection_complete  = 0x3e01
    };

    void btstack_libusb_device::handle_event( std::uint8_t ec, std::uint8_t param_size, const std::uint8_t* params )
    {
        std::uint16_t event_code = ec;

        if ( hic_event_code::le_meta_event == static_cast< hic_event_code >( event_code ) && param_size > 0 )
        {
            event_code = ( event_code << 8 ) | *params;
            --param_size;
            ++params;
        }

        switch ( static_cast< hic_event_code >( event_code ) )
        {
            case hic_event_code::le_connection_complete:
                if ( param_size == 18 )
                {
                    // const std::uint8_t  status = params[ 0 ];
                    // const std::uint16_t handle = params[ 1 ] | ( params[ 2 ] >> 8 );

                    log_info( "le_connection_complete" );
                }
                break;
            default:
                break;
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

    std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) >                                           btstack_libusb_device::advertising_data_;
    std::function< void( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size ) >    btstack_libusb_device::l2cap_input_;
}
