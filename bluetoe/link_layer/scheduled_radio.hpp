#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP

#include <cstdint>
#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/address.hpp>
#include <bluetoe/link_layer/ll_data_pdu_buffer.hpp>

namespace bluetoe {
namespace link_layer {

    class delta_time;
    class write_buffer;
    class read_buffer;

    /*
     * @brief Type responsible for radio I/O and timing
     *
     * The API provides a set of scheduling functions, to schedule advertising or to schedule connection events. All scheduling functions take a point in time
     * to switch on the receiver / transmitter and to transmit and to receive. This points are defined as relative offsets to a previous point in time T0. The
     * first T0 is defined by the return of the constructor. After that, every scheduling function have to define what the next T0 is, that the next
     * functions relative point in time, is based on.
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    class scheduled_radio : public ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack > >
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
         * CallBack::adv_received() or CallBack::adv_timeout() is called. In both cases, every following call to a scheduling
         * function is based on the time, the transmission was scheduled. So the new T0 = T0 + when. In case of a CRC error,
         * CallBack::adv_timeout() will be called immediately.
         *
         * White list filtering is applied by calling CallBack::is_scan_request_in_filter().
         *
         * This function is intended to be used for sending advertising PDUs.
         *
         * @param channel channel to transmit and to receive on
         * @param advertising_data the advertising data to be send out.
         * @param response_data the response data used to reply to a scan request, in case the request was in the white list.
         * @param transmit data to be transmitted
         * @param when point in time, when the first bit of data should be started to be transmitted
         * @param receive buffer where the radio will copy the received data, before calling Callback::adv_receive().
         *        This buffer have to have at least room for two bytes.
         */
        void schedule_advertisment(
            unsigned                                    channel,
            const bluetoe::link_layer::write_buffer&    advertising_data,
            const bluetoe::link_layer::write_buffer&    response_data,
            bluetoe::link_layer::delta_time             when,
            const bluetoe::link_layer::read_buffer&     receive );

        /**
         * @brief schedules a connection event
         *
         * The function will return immediately and schedule the receiver to start at start_receive.
         * CallBack::timeout() is called when between start_receive and end_receive no valid pdu is received. The new T0 is then the old T0.
         * CallBack::end_event() is called when the connection event is over. The new T0 is the time point where the first PDU was
         * received from the Master.
         *
         * In any case is one (and only one) of the callbacks called (timeout(), end_event()). The context of the callback call is run().
         *
         * Data to be transmitted and received is passed by the inherited ll_data_pdu_buffer.
         *
         * @ret the distance from now to start_receive
         */
        bluetoe::link_layer::delta_time schedule_connection_event(
            unsigned                                    channel,
            bluetoe::link_layer::delta_time             start_receive,
            bluetoe::link_layer::delta_time             end_receive,
            bluetoe::link_layer::delta_time             connection_interval );

        /**
         * @brief set the access address initial CRC value for transmitted and received PDU
         *
         * The values should be changed, when there is no outstanding scheduled transmission or receiving.
         * The values will be applied with the next call to schedule_advertisment() or schedule_connection_event().
         */
        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

        /**
         * @brief function to return a device specific value that is persistent and unique for the device (CPU id or such)
         */
        std::uint32_t static_random_address_seed() const;

        /**
         * @brief allocates the CPU to the scheduled_radio
         *
         * All callbacks given by the CallBack parameter are called from within this CPU context.
         * The function will return from time to time, when an external event happed. It's up to concrete
         * implementations to identify and to define situations where the CPU should be released back to the
         * calling application.
         */
        void run();

        /**
         * @brief forces the run() function to return at least once
         *
         * The function is intended to be used from interrupt handler to synchronize with the main thread.
         */
        void wake_up();

        /**
         * @brief type to allow ll_data_pdu_buffer to synchronize the access to the buffer data structures.
         */
        class lock_guard;

        /**
         * @brief giving the maximum number of white list entries, the scheduled radio supports by hardware
         *
         * Only if this value is not zero, the radio have to implement the white list related functions.
         */
        static constexpr std::size_t radio_maximum_white_list_entries = 4;

        /**
         * @brief add the given address to the white list.
         *
         * Function will return true, if it was possible to add the address to the white list
         * or if the address was already in the white list.
         * If there was not enough room to add the address to the white list, the function
         * returns false.
         */
        bool radio_add_to_white_list( const device_address& addr );

        /**
         * @brief remove the given address from the white list
         *
         * The function returns true, if the address was in the list.
         * @post addr is not in the white list.
         */
        bool radio_remove_from_white_list( const device_address& addr );

        /**
         * @brief returns true, if the given address in within the white list
         */
        bool radio_is_in_white_list( const device_address& addr ) const;

        /**
         * @brief returns the number of addresses that could be added to the
         *        white list before add_to_white_list() would return false.
         */
        std::size_t radio_white_list_free_size() const;

        /**
         * @brief remove all entries from the white list
         */
        void radio_clear_white_list();

        /**
         * @brief Accept connection requests only from devices within the white list.
         *
         * If the property is set to true, only connection requests from from devices
         * that are in the white list, should be answered.
         * If the property is set to false, all connection requests should be answered.
         *
         * The default value of the is property is false.
         *
         * @post connection_request_filter() == b
         * @sa connection_request_filter()
         */
        void connection_request_filter( bool b );

        /**
         * @brief current value of the property.
         */
        bool connection_request_filter() const;

        /**
         * @brief Accept scan requests only from devices within the white list.
         *
         * If the property is set to true, only scan requests from from devices
         * that are in the white list, should be answered.
         * If the property is set to false, all scan requests should be answered.
         *
         * @post radio_scan_request_filter() == b
         * @sa radio_scan_request_filter()
         */
        void radio_scan_request_filter( bool b );

        /**
         * @brief current value of the property.
         */
        bool radio_scan_request_filter() const;

        /**
         * @brief returns true, if a connection request from the given address should be answered.
         */
        bool radio_is_connection_request_in_filter( const device_address& addr ) const;

        /**
         * @brief returns true, if a scan request from the given address should be answered.
         */
        bool radio_is_scan_request_in_filter( const device_address& addr ) const;
    };
}

}

#endif // include guard
