#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <bluetoe/services/bas.hpp>
#include <bluetoe/services/dis.hpp>
#include <bluetoe/services/hid.hpp>

class keyboard_handler
{
public:
    int read_battery_level()
    {
        return 99;
    }

    std::uint8_t protocol_mode_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t protocol_mode_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t report_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }

    std::uint8_t report_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t report_map_read_handler( std::size_t offset, std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)offset;
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

    std::uint8_t hid_information_read_handler( std::size_t read_size, std::uint8_t* out_buffer, std::size_t& out_size )
    {
        (void)read_size;
        (void)out_buffer;
        (void)out_size;
        return bluetoe::error_codes::success;
    }

    std::uint8_t control_point_write_handler( std::size_t write_size, const std::uint8_t* value )
    {
        (void)write_size;
        (void)value;
        return bluetoe::error_codes::success;
    }
};

using battery_service = bluetoe::battery_level<
    bluetoe::bas::handler< keyboard_handler >
>;

static constexpr char manufacturer_name[] = "Torrox";
static constexpr char model_number[]      = "Model 1";

using device_info_service = bluetoe::device_information_service<
    bluetoe::dis::manufacturer_name< manufacturer_name >,
    bluetoe::dis::model_number< model_number >
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
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::report_write_handler >,
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::report_read_handler >,
        bluetoe::write_without_response,
        bluetoe::notify
    >,
    bluetoe::characteristic<
        bluetoe::hid::report_map_uuid,
        bluetoe::mixin_read_blob_handler< keyboard_handler, &keyboard_handler::report_map_read_handler >
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
        bluetoe::mixin_read_handler< keyboard_handler, &keyboard_handler::hid_information_read_handler >
    >,
    bluetoe::characteristic<
        bluetoe::hid::hid_control_point_uuid,
        bluetoe::mixin_write_handler< keyboard_handler, &keyboard_handler::control_point_write_handler >,
        bluetoe::only_write_without_response
    >
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
bluetoe::device< keyboard > server;

int main()
{
    for ( ;; )
    {
        server.run( gatt );
    }
}
