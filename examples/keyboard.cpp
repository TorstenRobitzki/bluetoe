#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <bluetoe/services/bas.hpp>
#include <bluetoe/services/dis.hpp>
#include <bluetoe/services/hid.hpp>
#include <bluetoe/descriptor.hpp>

#include <nrf.h>

static constexpr std::uint32_t suspend_mode_led_pin = 17;

void set_led( std::uint32_t pin, bool on )
{
    NRF_GPIO->OUT = on
        ? NRF_GPIO->OUT & ~( 1 << pin )
        : NRF_GPIO->OUT | ( 1 << pin );
}

void init_hardware()
{
    NRF_GPIO->PIN_CNF[ suspend_mode_led_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

}

 // shamless stolen from the Nordic examples
static const std::uint8_t report_map_data[] =
{
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x06,       // Usage (Keyboard)
    0xA1, 0x01,       // Collection (Application)
    0x05, 0x07,       // Usage Page (Key Codes)
    0x19, 0xe0,       // Usage Minimum (224)
    0x29, 0xe7,       // Usage Maximum (231)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x01,       // Logical Maximum (1)
    0x75, 0x01,       // Report Size (1)
    0x95, 0x08,       // Report Count (8)
    0x81, 0x02,       // Input (Data, Variable, Absolute)

    0x95, 0x01,       // Report Count (1)
    0x75, 0x08,       // Report Size (8)
    0x81, 0x01,       // Input (Constant) reserved byte(1)

    0x95, 0x05,       // Report Count (5)
    0x75, 0x01,       // Report Size (1)
    0x05, 0x08,       // Usage Page (Page# for LEDs)
    0x19, 0x01,       // Usage Minimum (1)
    0x29, 0x05,       // Usage Maximum (5)
    0x91, 0x02,       // Output (Data, Variable, Absolute), Led report
    0x95, 0x01,       // Report Count (1)
    0x75, 0x03,       // Report Size (3)
    0x91, 0x01,       // Output (Data, Variable, Absolute), Led report padding

    0x95, 0x06,       // Report Count (6)
    0x75, 0x08,       // Report Size (8)
    0x15, 0x00,       // Logical Minimum (0)
    0x25, 0x65,       // Logical Maximum (101)
    0x05, 0x07,       // Usage Page (Key codes)
    0x19, 0x00,       // Usage Minimum (0)
    0x29, 0x65,       // Usage Maximum (101)
    0x81, 0x00,       // Input (Data, Array) Key array(6 bytes)

    0x09, 0x05,       // Usage (Vendor Defined)
    0x15, 0x00,       // Logical Minimum (0)
    0x26, 0xFF, 0x00, // Logical Maximum (255)
    0x75, 0x08,       // Report Count (2)
    0x95, 0x02,       // Report Size (8 bit)
    0xB1, 0x02,       // Feature (Data, Variable, Absolute)

    0xC0              // End Collection (Application)
};

static const std::uint8_t hid_information[] =
{
    0x11, 0x01,         // USB HID Version 1.11
    0x00,               // Hardware target country.
    0x02,               // RemoteWake = false; NormallyConnectable = true
};

class keyboard_handler
{
public:
    keyboard_handler()
        : mode_( protocol_mode::report )
    {
        set_led( suspend_mode_led_pin, false );
    }

    int read_battery_level()
    {
        return 99;
    }

    std::uint8_t protocol_mode_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        if ( write_size == 1 && ( *value == protocol_mode::boot || *value == protocol_mode::report ) )
        {
            mode_ = static_cast< decltype( mode_ ) >( *value );
        }

        return bluetoe::error_codes::success;
    }

    std::uint8_t protocol_mode_read_handler( std::size_t, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        *out_buffer = mode_;
        out_size = 1;

        return bluetoe::error_codes::success;
    }

    std::uint8_t input_report_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t input_report_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t output_report_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t output_report_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t feature_report_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t feature_report_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t boot_input_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t boot_input_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t boot_output_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t boot_output_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t control_point_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        if ( write_size == 1 && ( *value == opcodes::suspend || *value == opcodes::exit_suspend ) )
        {
            set_led( suspend_mode_led_pin, *value == opcodes::suspend );
        }

        return bluetoe::error_codes::success;
    }

private:
    struct protocol_mode
    {
        enum protocol_mode_t : std::uint8_t
        {
            boot   = 0x00,
            report = 0x01
        };
    };

    struct opcodes
    {
        enum : std::uint8_t
        {
            suspend      = 0x00,
            exit_suspend = 0x01
        };
    };

    protocol_mode::protocol_mode_t       mode_;
};

using battery_service = bluetoe::battery_level<
    bluetoe::bas::handler< keyboard_handler >,
    bluetoe::requires_encryption
>;

static constexpr char manufacturer_name[] = "Torrox";
static constexpr char model_number[]      = "Model 1";

using device_info_service = bluetoe::device_information_service<
    bluetoe::dis::manufacturer_name< manufacturer_name >,
    bluetoe::dis::model_number< model_number >,
    bluetoe::dis::pnp_id<
        bluetoe::dis::vendor_id_source_t::bluetooth, 0x0269,
        0x4711,
        0x0000 >,
    bluetoe::requires_encryption
>;

using hid_service = bluetoe::service<
    bluetoe::hid::service_uuid,
    bluetoe::characteristic<
        bluetoe::hid::protocol_mode_uuid,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::protocol_mode_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::protocol_mode_read_handler >,
        bluetoe::only_write_without_response
    >,
    bluetoe::characteristic<
        bluetoe::hid::report_uuid,
        bluetoe::hid::input_report_reference< 0 >,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::input_report_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::input_report_read_handler >,
        bluetoe::notify
    >,
    bluetoe::characteristic<
        bluetoe::hid::report_uuid,
        bluetoe::hid::output_report_reference< 0 >,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::output_report_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::output_report_read_handler >,
        bluetoe::write_without_response
    >,
    bluetoe::characteristic<
        bluetoe::hid::report_uuid,
        bluetoe::hid::feature_report_reference< 0 >,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::feature_report_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::feature_report_read_handler >
    >,
    bluetoe::characteristic<
        bluetoe::hid::report_map_uuid,
        bluetoe::fixed_blob_value< report_map_data, sizeof( report_map_data ) >
    >,
    bluetoe::characteristic<
        bluetoe::hid::boot_keyboard_input_report_uuid,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::boot_input_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::boot_input_read_handler >,
        bluetoe::notify
    >,
    bluetoe::characteristic<
        bluetoe::hid::boot_keyboard_output_report_uuid,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::boot_output_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::boot_output_read_handler >,
        bluetoe::write_without_response
    >,
    bluetoe::characteristic<
        bluetoe::hid::hid_information_uuid,
        bluetoe::fixed_blob_value< hid_information, sizeof( hid_information ) >
    >,
    bluetoe::characteristic<
        bluetoe::hid::hid_control_point_uuid,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::control_point_write_handler >,
        bluetoe::only_write_without_response
    >,
    bluetoe::requires_encryption
>;

static const char name[] = "Bluetoe-Keyboard";

using keyboard = bluetoe::server<
    bluetoe::server_name< name >,
    bluetoe::appearance::keyboard,
    // by default, bluetoe would advertise all services
    bluetoe::list_of_16_bit_service_uuids<
        bluetoe::hid::service_uuid
    >,
    battery_service,
    device_info_service,
    hid_service,
    bluetoe::mixin< keyboard_handler >
>;

keyboard gatt;
bluetoe::device<
    keyboard,
    bluetoe::link_layer::advertising_interval< 30u > > server;

int main()
{
    init_hardware();

    for ( ;; )
    {
        server.run( gatt );
    }
}
