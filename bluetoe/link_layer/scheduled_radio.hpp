#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {

    class delta_time;

    /*
     * @brief Type responsible for radio I/O and timeing
     *
     * The API provides a set of scheduling functions, which schedule transmit and/or receiving of the radion. All scheduling functions take a point in time
     * to switch on the receiver / transmitter and to transmit and to receive. This points are defined as relative offsets to a previous point in time. The
     * first point is defined by the return of the constructor. After that, every scheduling function have to define what the next point is, that the next
     * functions relative point in time, is based on.
     */
    template < typename CallBack >
    class scheduled_radio
    {
    public:
        /**
         * initializes the hardware and defines the first time point as anker for the next call to a scheduling function.
         */
        scheduled_radio();

        /**
         * @brief schedules for transmission and starts to receive at least 150µs later for the given timeout
         */
        void schedule_transmit_and_receive( unsigned channel, const std::uint8_t* data, std::size_t size, delta_time when, delta_time timeout );

        void schedule_transmit( unsigned channel, const std::uint8_t* data, std::size_t size, delta_time when_ms );

        void schedule_receive( unsigned channel, delta_time when_ms, delta_time timeout_ms );
    };
}

}

#endif // include guard
