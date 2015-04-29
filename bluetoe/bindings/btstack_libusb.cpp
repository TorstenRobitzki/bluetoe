#include "btstack_libusb.hpp"

#include "btstack-config.h"

#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>

#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include <bluetoe/codes.hpp>
#include <cstdint>
#include <algorithm>

namespace bluetoe {
namespace binding {

    static constexpr auto hci_header_size = 8;

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

    static std::uint8_t* write_u16( std::uint8_t* data, std::uint16_t value )
    {
        data[ 0 ] = value & 0xff;
        data[ 1 ] = value >> 8;

        return data + 2;
    }

    void btstack_libusb_device_base::btstack_packet_handler( std::uint8_t packet_type, std::uint8_t *packet, std::uint16_t size)
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
                    const std::uint16_t hci_length       = read_u16( packet + 2 );
                    const std::uint16_t l2cap_length     = read_u16( packet + 4 );
                    const std::uint16_t l2cap_channel_id = read_u16( packet + 6 );

                    if ( l2cap_channel_id == 0x0004 && l2cap_length == size - 8 && hci_length == size - 4 )
                    {
                        size    -= hci_header_size;
                        packet  += hci_header_size;
                        log_info( "*ATT-Command: %i", size );
                        hexdump( packet, size ), no_log = true;

                        hci_reserve_packet_buffer();
                        uint8_t     *acl_buffer      = hci_get_outgoing_packet_buffer();
                        std::size_t out_buffer_size  = mtu_size_;

                        l2cap_input_( packet, size, acl_buffer + hci_header_size, out_buffer_size );

                        send_acl_package( acl_buffer, out_buffer_size + hci_header_size );
                    }
                }
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

    void btstack_libusb_device_base::send_notification( std::uint8_t* buffer, std::size_t size )
    {
        log_info( "*ATT-Notification: %i", size );
        hexdump( buffer, size );

        hci_reserve_packet_buffer();
        uint8_t     *acl_buffer      = hci_get_outgoing_packet_buffer();
        std::size_t out_buffer_size  = mtu_size_;

        std::copy( buffer, buffer + size, acl_buffer );

        send_acl_package( acl_buffer, size + hci_header_size );
    }

    void btstack_libusb_device_base::send_acl_package( std::uint8_t* acl_buffer, std::size_t size )
    {
        const int pb = hci_non_flushable_packet_boundary_flag_supported() ? 0x00 : 0x02;

        // 0 - Connection handle : PB=pb : BC=00
        write_u16(acl_buffer + 0, connection_handle_ | (pb << 12) | (0 << 14));
        write_u16(acl_buffer + 2, size - 4);
        write_u16(acl_buffer + 4, size - 8);
        // 6 - L2CAP channel = 1
        write_u16(acl_buffer + 6, 0x0004);

        log_info("*send_acl_package: %i", size );
        hexdump( acl_buffer, size );

        hci_send_acl_packet_buffer( size );
    }

    enum class hic_event_code {
        le_meta_event = 0x3e,
        le_connection_complete  = 0x3e01
    };

    void btstack_libusb_device_base::handle_event( std::uint8_t ec, std::uint8_t param_size, const std::uint8_t* params )
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
                    const int  status = params[ 0 ];

                    if ( status == 0 )
                    {
                        connection_handle_ = read_u16( &params[ 1 ] );
                        log_info( "connection_handle_: %i", connection_handle_ );
                    }

                    log_info( "le_connection_complete: %i", status );
                }
                break;
            default:
                break;
        }
    }

    void btstack_libusb_device_base::init()
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
        hci_register_packet_handler( &btstack_libusb_device_base::btstack_packet_handler );
        hci_connectable_control(0); // no services yet

        // turn on!
        hci_power_control(HCI_POWER_ON);

        // go
        run_loop_execute();
    }

    std::function< std::size_t( std::uint8_t* buffer, std::size_t buffer_size ) >                                           btstack_libusb_device_base::advertising_data_;
    std::function< void( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size ) >    btstack_libusb_device_base::l2cap_input_;
    std::uint16_t                                                                                                           btstack_libusb_device_base::connection_handle_ = 0;
    std::uint16_t                                                                                                           btstack_libusb_device_base::mtu_size_ = HCI_PACKET_BUFFER_SIZE - 8;
}
}
