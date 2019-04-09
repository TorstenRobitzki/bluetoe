#ifndef BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP
#define BLUETOE_LINK_LAYER_SCHEDULED_RADIO_HPP

#include <cstdint>
#include <buffer.hpp>
#include <address.hpp>
#include <ll_data_pdu_buffer.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>

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
     *
     * TransmitSize and ReceiveSize is the size of buffer used for receiving and transmitting. This might not make sense for all implementations.
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

        /**
         * @brief a number of bytes that are additional required by the hardware to handle an over the air package/PDU.
         *
         * This number includes every byte that have to stored in a package to meet the requirments of the hardware,
         * that is not part of the received / transmitted PDU (CRC, preamble etc.).
         */
        static constexpr std::size_t radio_package_overhead = 0;


        /**
         * @brief indication no support for encryption
         */
        static constexpr bool hardware_supports_encryption = false;
    };

    /**
     * @brief extension of a scheduled_radio with functions to support encryption
     *
     * To allow the utilization of hardware support for certain cryptographical functions,
     * this interface abstracts at a quite high level.
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    class scheduled_radio_with_encryption : public scheduled_radio< TransmitSize, ReceiveSize, CallBack >
    {
    public:
        /**
         * @brief indication support for encryption
         */
        static constexpr bool hardware_supports_encryption = true;

        /**
         * @brief Function to create the Srand according to 2.3.5.5 Part H, Vol 3, Core Spec
         */
        bluetoe::details::uint128_t create_srand();

        /**
         * @brief Function to create a random long term key and random Rand and EDIV values to identify this newly created key.
         */
        bluetoe::details::longterm_key_t create_long_term_key();

        /**
         * @brief Confirm value generation function c1 for LE Legacy Pairing
         *
         * @param temp_key the temporary key from the LE legacy pairing algorithm
         * @param rand the value created by create_srand() or the
         * @param p1 p1 = pres || preq || rat’ || iat’ (see 2.3.3 Confirm value generation function c1 for LE Legacy Pairing)
         * @param p2 p2 = padding || ia || ra (see 2.3.3 Confirm value generation function c1 for LE Legacy Pairing)
         *
         * The function calculates the confirm value based on the slaves or masters random value (Srand or Mrand), the temporary
         * key and the data in the pairing request and response.
         */
        bluetoe::details::uint128_t c1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& rand,
            const bluetoe::details::uint128_t& p1,
            const bluetoe::details::uint128_t& p2 ) const;

        /**
         * @brief Key generation function s1 for LE Legacy Pairing
         *
         * The key generation function s1 is used to generate the STK during the LE
         * legacy pairing process.
         *
         * @param temp_key the temporary key from the LE legacy pairing algorithm
         * @param srand The slave random value (Srand).
         * @param mrand The master random value (Mrand).
         */
        bluetoe::details::uint128_t s1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& srand,
            const bluetoe::details::uint128_t& mrand );

        /**
         * @brief setup the hardware with all data required for encryption
         *
         * The encryption is prepaired but not started jet.
         * @param key long term or short term key to be used for encryption
         * @param skdm The master’s portion of the session key diversifier.
         * @param ivm The IVm field contains the master’s portion of the initialization vector.
         *
         * The function returns SKDs and IVs (the slaves portion of the session key diversifier and initialization vector),
         * to be send to the master.
         */
        std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm );

    };
        /**
         * @brief start the encryption after the next connection event.
         */
        void start_encryption();

        /**
         * @brief stop encryption after the next connection event.
         */
        void stop_encryption();

    };

    /**
     * @brief type that provides types and functions to access the differnt parts of a receiving PDU.
     *
     * There might be technical reasons, why an in memory PDU might differ in layout from an over the air PDU.
     * This indirection is supposed to resolve such cases. (Currently, it's nrf51/52 that requires this).
     *
     * By default, bluetoe::link_layer::default_pdu_layout is used. To override this for a radio R, specialize
     * bluetoe::link_layer::pdu_layout_by_radio for R to define an alias to the layout type to be used:
     *
     * @code
     * template <>
     * struct pdu_layout_by_radio< R > {
     *     using pdu_layout = special_pdu_layout_required_by_R;
     * };
     *
     */
    struct pdu_layout {
        /**
         * @brief returns the header for advertising channel and for data channel PDUs.
         */
        static std::uint16_t header( const read_buffer& pdu );
        static std::uint16_t header( const write_buffer& pdu );
        static std::uint16_t header( const std::uint8_t* pdu );

        /**
         * @brief writes to the header of the given PDU
         */
        static void header( const read_buffer& pdu, std::uint16_t header_value );
        static void header( std::uint8_t* pdu, std::uint16_t header_value );

        /**
         * @brief returns the writable body for advertising channel or for data channel PDUs.
         */
        static std::pair< std::uint8_t*, std::uint8_t* > body( const read_buffer& pdu );

        /**
         * @brief returns the readonly body for advertising channel or for data channel PDUs.
         */
        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const write_buffer& pdu );

        /**
         * @brief returns the size of the memory buffer that is required to keep a data channel PDU
         *        in memory, with the given payload size (payload according to Vol.6; Part B; 2.4).
         *
         * This function have to be constexpr / evaluatable at compile time to allow static buffer
         * size calculation.
         */
        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size );
    };

}

}

#endif // include guard
