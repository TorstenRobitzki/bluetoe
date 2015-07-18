#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP

#include <cstdint>

namespace bluetoe {
namespace link_layer {

    class delta_time;
    class write_buffer;
    class read_buffer;

    /*
     * @brief Type responsible for radio I/O and timeing
     *
     * The API provides a set of scheduling functions, which schedules transmit and/or receiving of the radion. All scheduling functions take a point in time
     * to switch on the receiver / transmitter and to transmit and to receive. This points are defined as relative offsets to a previous point in time T0. The
     * first T0 is defined by the return of the constructor. After that, every scheduling function have to define what the next T0 is, that the next
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
         * @brief schedules for transmission and starts to receive 150µs later
         *
         * The function will return imedately. Depending on whether a response is received or the receiving times out,
         * CallBack::received() or CallBack::timeout() is called. In both cases, every following call to a scheduling
         * function is based on the time, the tranmision was scheduled. So the new T0 = T0 + when.
         *
         * This function is intended to be used for sending advertising PDUs. If the given receive buffer is empty, the timeout callback
         * will be called when the PDU was sent.
         *
         * @param channel channel to transmit and to receive on
         * @param transmit data to be transmitted
         * @param when point in time, when the first bit of data should be started to be transmitted
         * @param receive buffer where the radio will copy the received data, before calling Callback::receive(). This parameter can be empty if no receiving is intended.
         */
        void schedule_transmit_and_receive(
                unsigned                                    channel,
                const bluetoe::link_layer::write_buffer&    transmit,
                bluetoe::link_layer::delta_time             when,
                const bluetoe::link_layer::read_buffer&     receive );

        void schedule_transmit( unsigned channel, const std::uint8_t* data, std::size_t size, delta_time when_ms );

        void schedule_receive( unsigned channel, delta_time when_ms, delta_time timeout_ms );

        /**
         * @brief function to return a device specific value that is persistant and unique for the device (CPU id or such)
         */
        std::uint32_t static_random_address_seed() const;
    };
}

}

#endif // include guard
