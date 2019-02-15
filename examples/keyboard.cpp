#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <bluetoe/services/bas.hpp>
#include <bluetoe/services/dis.hpp>

struct keyboard_handler {
    int read_battery_level()
    {
        return 99;
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

using keyboard = bluetoe::server<
    battery_service,
    device_info_service,
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
