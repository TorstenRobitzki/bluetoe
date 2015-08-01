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
    template <  std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    class scheduled_radio
    {
    public:
        /**
         * initializes the hardware and defines the first time point as anker for the next call to a scheduling function.
         */
        scheduled_radio();

        /**
         * @brief schedules for transmission of advertising data and starts to receive 150µs later
         *
         * The function will return immediately. Depending on whether a response is received or the receiving times out,
         * CallBack::received() or CallBack::timeout() is called. In both cases, every following call to a scheduling
         * function is based on the time, the tranmision was scheduled. So the new T0 = T0 + when. In case of a CRC error,
         * CallBack::timeout() will be called immediately .
         *
         * This function is intended to be used for sending advertising PDUs. If the given receive buffer is empty, the timeout callback
         * will be called when the PDU was sent.
         *
         * @param channel channel to transmit and to receive on
         * @param transmit data to be transmitted
         * @param when point in time, when the first bit of data should be started to be transmitted
         * @param receive buffer where the radio will copy the received data, before calling Callback::receive(). This parameter can be empty if no receiving is intended.
         */
        void schedule_advertisment_and_receive(
            unsigned                                    channel,
            const bluetoe::link_layer::write_buffer&    transmit,
            bluetoe::link_layer::delta_time             when,
            const bluetoe::link_layer::read_buffer&     receive );

        /**
         * @brief schedules a connection event
         *
         * The function will return immediately and schedule the receiver to start at start_receive.
         *
         */
        void schedule_connection_event(
            unsigned                                    channel,
            bluetoe::link_layer::delta_time             start_receive,
            bluetoe::link_layer::delta_time             end_receive,
            bluetoe::link_layer::delta_time             connection_interval );

        /**
         * @brief set the access address initial CRC value for transmitted and received PDU
         *
         * The values should be changed, when there is no outstanding scheduled transmission or receiving.
         * The values will be applied with the next call to schedule_advertisment_and_receive() or schedule_receive_and_transmit().
         */
        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

        /**
         * @brief function to return a device specific value that is persistant and unique for the device (CPU id or such)
         */
        std::uint32_t static_random_address_seed() const;
    };
}

}

#endif // include guard
