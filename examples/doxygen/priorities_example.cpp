
#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/outgoing_priority.hpp>
#include <cstdint>

// the enumeration of the possible axis
enum class axis {
    X, Y, Z
};

// a handler that will use a sensor depending on the axis X
template < axis X >
struct acceleration_measurement_handler
{
    /*
     * This handler function will respond to requests to the acceleration sensors value or,
     * when the server was requested to send a notification of this characteristic.
     */
    std::uint8_t read_sensor_value( std::size_t, std::uint8_t* out_buffer, std::size_t& out_size );

    /*
     * This handler function will be called to fill the content of a temperatur alarm notification
     * and is thus more important to reach a GATT client than the characteristic above.
     */
    std::uint8_t temperature_alarm( std::size_t, std::uint8_t* out_buffer, std::size_t& out_size );
};

// the definition of a service that provides access to the value of a given access
template < class UUID, axis X >
using acceleration_measurement_service = bluetoe::service<
    UUID,
    bluetoe::mixin< acceleration_measurement_handler< X > >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xCBD99443, 0x85C2, 0x415B, 0x901B, 0x23C0CAE8B0B8 >,
        bluetoe::mixin_read_handler<
            acceleration_measurement_handler< X >,
            &acceleration_measurement_handler< X >::read_sensor_value >,
        bluetoe::notify
    >,
    bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0xCBD99443, 0x85C2, 0x415B, 0x901B, 0x23C0CAE8B0B8 >,
        bluetoe::mixin_read_handler<
            acceleration_measurement_handler< X >,
            &acceleration_measurement_handler< X >::temperature_alarm >,
        bluetoe::notify
    >,
    // make sure, that temperature alarms are send out with higher priority
    bluetoe::higher_outgoing_priority<
        bluetoe::characteristic_uuid< 0xCBD99443, 0x85C2, 0x415B, 0x901B, 0x23C0CAE8B0B8 >
    >
>;

/*
 * A server with 3 similar acceleration services.
 */
using helicopter = bluetoe::server<
    acceleration_measurement_service<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        axis::X
    >,
    // this service messures the altitude and is thus more important than the other
    // axis.
    bluetoe::higher_outgoing_priority<
        bluetoe::service_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >
    >,
    acceleration_measurement_service<
        bluetoe::service_uuid< 0x8C8B4095, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        axis::Y
    >,
    acceleration_measurement_service<
        bluetoe::service_uuid< 0x8C8B4096, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        axis::Z
    >
>;
