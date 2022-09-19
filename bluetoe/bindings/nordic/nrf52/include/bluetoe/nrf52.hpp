#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <bluetoe/security_tool_box.hpp>
#include <bluetoe/connection_events.hpp>

/**
 * @file nrf52.hpp
 *
 * Design desisions:
 *
 * - The radio interrupt has highes priority
 * - The radio ISR is triggered by the radio DISABLED event.
 * - The radio is either disabled by receiving and transmitting, or by the receive timeout
 *   timer, that triggers the DISABLE task of the radio, if the reception timed out.
 * - The ancor of every timing, is the reception of the first PDU in a connection event.
 * - During advertising, the accuracy of the ancor is not important, as there is already some
 *   random jitter to be applied to the interval according to the specs.
 * - CC[ 0 ] of timer0 is used to start the radio
 * - CC[ 1 ] is used to implement the timeout timer and DISABLES the radio
 * - CC[ 2 ] is used to capture the anchor when receiving a connect request or the first PDU
 *           in a connection event.
 * - RTC0 is always running and used as time base. TIMER0 is just started if the RTC0 comes close
 *   to a connection / advertising event.
 *
 * Resources used:
 *
 * - The radio peripheral is exclusively used by Bluetoe.
 * - Timer0 is exclusively used by Bluetoe.
 * - Timer1 is used by Bluetoe, if the synchronized_connection_event_callback<> feature is used.
 * - Bluetoe will switch the HFXO on and off.
 * - RTC0 is exclusively used by Bluetoe.
 * - Bluetoe will switch on the configured low frequency clock source.
 * - CCM, AES, and RNG peripheral are exclusively used by Bluetoe, if encryption
 *   is enabled.
 * - The highest priority interrupt level is exclusively used by Bluetoe.
 * - Bluetoe used PPI channel 19 exclusively
 * - Bluetoe used PPI channel 18 exclusively, if the synchronized_connection_event_callback<> feature is used.
 */
namespace bluetoe
{
    namespace nrf52_details
    {
        static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;
        // position of the connecting address (AdvA)
        static constexpr unsigned           connect_addr_offset          = 2 + 6;

        // after T_IFS (150µs +- 2) at maximum, a connection request will be received (34 Bytes + 1 Byte preable, 4 Bytes Access Address and 3 Bytes CRC)
        // plus some additional 20µs
        static constexpr std::uint32_t      adv_reponse_timeout_us       = 152 + 42 * 8 + 20;
        static constexpr unsigned           us_radio_rx_startup_time     = 140;
        static constexpr unsigned           us_radio_tx_startup_time     = 130;

        // Time reserved to setup a connection event in µs
        // time measured to setup a connection event, using GCC 8.3.1 with -O0 is 12µs
        // the addition 31µs is to take the granularity of the sleep clock into account (32kHz)
        // a second 31µs is add to take the same granularity of the sleep clock into account when
        // it is used to calculate the current distance to the last anchor
        static constexpr std::uint32_t      setup_connection_event_limit_us =
            50 + std::max( us_radio_rx_startup_time, us_radio_tx_startup_time ) + 31 + 31;

        static constexpr std::uint8_t       more_data_flag = 0x10;
        static constexpr std::size_t        encryption_mic_size = 4;

        /**
         * @brief Counter used for CCM
         */
        struct counter {
            std::uint32_t   low;
            std::uint8_t    high;

            // set to zero
            counter();

            void increment();
            void copy_to( std::uint8_t* target ) const;
        };

        /**
         * @brief abstraction of the hardware that can be replaced during tests
         *
         * This abstracts the hardware operations (Radio, Timer)
         */
        class radio_hardware_without_crypto_support
        {
        public:
            static int pdu_gap_required_by_encryption()
            {
                return 0;
            }

            static void init( void (*isr)( void* ), void* that );

            /**
             * @brief configure the radio to use the given channel
             */
            static void configure_radio_channel(
                unsigned                                    channel );

            /**
             * @brief configure radio to transmit data and then start to reveive.
             */
            static void configure_transmit_train(
                const bluetoe::link_layer::write_buffer&    transmit_data );

            /**
             * @brief configure the radio to transmit and then stop the radio.
             */
            static void configure_final_transmit(
                const bluetoe::link_layer::write_buffer&    transmit_data );

            /**
             * @brief configure radio to receive data and then switch to
             *        transmitting.
             */
            static void configure_receive_train(
                const bluetoe::link_layer::read_buffer&     receive_buffer );

            static void stop_radio();

            /**
             * @brief store the captured time anchor
             */
            static void store_timer_anchor( int offset_us );

            /**
             * @brief returns details to a received PDU
             *
             * Returns { true, X } if a PDU was received
             * Returns { true, true } if that PDU is valid
             * Returns { true, false } if that PDU has a CRC or MIC error
             */
            static std::pair< bool, bool > received_pdu();

            /**
             * @brief elapsed time in µs since the last anchor
             *
             * @sa store_timer_anchor
             */
            static std::uint32_t now();

            /**
             * @brief try to stop the connection event and return the time from the
             *        anchor.
             */
            static std::pair< bool, link_layer::delta_time > can_stop_connection_event_timer( std::uint32_t safety_margin_us );

            static void setup_identity_resolving(
                const std::uint8_t* )
            {
            }

            static bool resolving_address_invalid()
            {
                return false;
            }

            static void set_phy(
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t transmiting_c_encoding );

            /**
             * @brief triggers the radio.start task at when, disables the radio timeout_us later
             *
             * @return true, if the high frequency clock isn't used anymore
             */
            static bool schedule_advertisment_event_timer(
                bluetoe::link_layer::delta_time when,
                std::uint32_t                   timeout_us,
                std::uint32_t                   start_hfxo_offset );

            static void schedule_connection_event_timer(
                std::uint32_t                   begin_us,
                std::uint32_t                   end_us,
                std::uint32_t                   start_hfxo_offset );

            static bool schedule_user_timer(
                void (*isr)( void* ),
                std::uint32_t                   time_us,
                std::uint32_t                   max_cb_runtimer_ms );

            static bool stop_user_timer();
            /**
             * @brief stop the timer from disabling the RADIO
             */
            static void stop_timeout_timer();

            static std::uint32_t static_random_address_seed();

            static void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            static void debug_toggle();

            class lock_guard
            {
            public:
                // see https://devzone.nordicsemi.com/question/47493/disable-interrupts-and-enable-interrupts-if-they-where-enabled/
                lock_guard()
                    : context_( __get_PRIMASK() )
                {
                    __disable_irq();
                }

                ~lock_guard()
                {
                    __set_PRIMASK( context_ );
                }

                lock_guard( const lock_guard& ) = delete;
                lock_guard& operator=( const lock_guard& ) = delete;
            private:
                const std::uint32_t context_;
            };


        protected:
            static bool          receive_2mbit_;
            static bool          transmit_2mbit_;

        private:

            // as the end of the PDU is captured and the start of the connection event
            // then calculated based on the PDU size, HF frequency domain anchor can be negativ
            static int           hf_connection_event_anchor_;
            static std::uint32_t lf_connection_event_anchor_;

            volatile static int           hf_user_timer_anchor_;
            volatile static std::uint32_t lf_user_timer_anchor_;
            volatile static int           user_timer_anchor_version_;
            volatile static bool          user_timer_start_;
        };

        /**
         * @brief abstraction of the hardware that can be replaced during tests
         *
         * This abstract extends the radio_hardware_without_crypto_support abstraction,
         * by adding function, necessary for encrypting the link.
         */
        class radio_hardware_with_crypto_support : public radio_hardware_without_crypto_support
        {
        public:
            static int pdu_gap_required_by_encryption()
            {
                return 1;
            }

            using radio_hardware_without_crypto_support::init;
            static void init( std::uint8_t* encrypted_area, void (*isr)( void* ), void* that );

            static void configure_transmit_train(
                const bluetoe::link_layer::write_buffer&    transmit_data );

            static void configure_final_transmit(
                const bluetoe::link_layer::write_buffer&    transmit_data );

            static void configure_receive_train(
                const bluetoe::link_layer::read_buffer&     receive_buffer );

            static void store_timer_anchor( int offset_us );

            static std::pair< bool, bool > received_pdu();

            static void configure_encryption( bool receive, bool transmit );

            static std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm );

            static void increment_receive_packet_counter()
            {
                receive_counter_.increment();
            }

            static void increment_transmit_packet_counter()
            {
                transmit_counter_.increment();
            }

            /*
             * @brief set the advertising address
             */
            static void setup_identity_resolving_address(
                const std::uint8_t* address );

            /*
             * @brief enable identity resolving and the set the corresponding key
             */
            static void set_identity_resolving_key(
                const details::identity_resolving_key_t& irk );

            /*
             * @brief returns true, if a request steams from an invalid address
             */
            static bool resolving_address_invalid();

        private:
            static volatile bool    receive_encrypted_;
            static volatile bool    transmit_encrypted_;
            static std::uint8_t*    encrypted_area_;
            static counter          receive_counter_;
            static counter          transmit_counter_;
            static bool             identity_resolving_enabled_;
        };

        template < typename ... Options >
        struct radio_options
        {
            using result = typename bluetoe::details::find_all_by_meta_type<
                bluetoe::nrf::nrf_details::radio_option_meta_type,
                Options...
            >::type;
        };

        template < typename ... Options >
        struct link_layer_options
        {
            using result = typename bluetoe::details::find_all_by_not_meta_type<
                bluetoe::details::binding_option_meta_type,
                Options...
            >::type;
        };

        template < class CallBacks, class Hardware, class Buffer, typename ... RadioOptions >
        class nrf52_radio_base : public Buffer
        {
        public:
            nrf52_radio_base()
            {
               low_frequency_clock_t::start_clocks();
                Hardware::init( []( void* that ){
                    static_cast< nrf52_radio_base* >( that )->radio_interrupt_handler();
                }, this);
            }

            nrf52_radio_base( std::uint8_t* receive_buffer )
            {
               low_frequency_clock_t::start_clocks();
                Hardware::init( receive_buffer, []( void* that ){
                    static_cast< nrf52_radio_base* >( that )->radio_interrupt_handler();
                }, this);
            }

            void schedule_advertisment(
                unsigned                                    channel,
                const bluetoe::link_layer::write_buffer&    advertising_data,
                const bluetoe::link_layer::write_buffer&    response_data,
                bluetoe::link_layer::delta_time             when,
                const bluetoe::link_layer::read_buffer&     receive )
            {
                assert( !adv_received_ );
                assert( !adv_timeout_ );
                assert( state_ == state::idle );
                assert( receive.buffer && receive.size >= 2u );

                bluetoe::link_layer::write_buffer advertising = advertising_data;
                advertising.size = std::min< std::uint8_t >( advertising.size, maximum_advertising_pdu_size );

                response_data_       = response_data;
                receive_buffer_      = receive;
                receive_buffer_.size = std::min< std::size_t >( receive.size, maximum_advertising_pdu_size );

                Hardware::configure_radio_channel( channel );
                Hardware::configure_transmit_train( advertising );

                Hardware::setup_identity_resolving( receive_buffer_.buffer + connect_addr_offset );

                // Advertising size + LL header + preable + access address + CRC
                const std::uint32_t read_timeout = ( advertising.size + 2 + 1 + 4 + 3 ) * 8 + adv_reponse_timeout_us;

                state_ = state::adv_transmitting;

                if ( Hardware::schedule_advertisment_event_timer( when, read_timeout, hfxo_startup_value ) )
                    low_frequency_clock_t::stop_high_frequency_crystal_oscilator();
            }

            link_layer::delta_time schedule_connection_event(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             start_receive,
                bluetoe::link_layer::delta_time             end_receive,
                bluetoe::link_layer::delta_time             connection_interval )
            {
                assert( state_ == state::idle );
                assert( start_receive < end_receive );

                // Stop all interrupts so that the calculation, that enough CPU time is available to setup everything, will not
                // be disturbed by any interrupt.
                lock_guard lock;

                const std::uint32_t start_event = start_receive.usec();
                // TODO: 500: must depend on receive size. This probably would depend on the maximum
                // LL PDU size that would be expected.
                const std::uint32_t end_event   = end_receive.usec() + 500;

                const auto now = Hardware::now();
                if ( now + start_event_safety_margin_us > start_event )
                {
                    evt_timeout_ = true;
                    return connection_interval;
                }

                receive_buffer_ = receive_buffer();
                state_          = state::evt_wait_connect;

                Hardware::configure_radio_channel( channel );
                Hardware::configure_receive_train( receive_buffer_ );

                Hardware::schedule_connection_event_timer( start_event, end_event, hfxo_startup_value );
                low_frequency_clock_t::stop_high_frequency_crystal_oscilator();

                return link_layer::delta_time( connection_interval.usec() - now );
            }

            std::pair< bool, link_layer::delta_time > disarm_connection_event()
            {
                lock_guard lock;

                const auto result = Hardware::can_stop_connection_event_timer( start_event_safety_margin_us );

                if ( result.first )
                {
                    Hardware::stop_radio();
                    state_ = state::idle;
                }

                return result;
            }

            bool schedule_synchronized_user_timer(
                    bluetoe::link_layer::delta_time timer,
                    bluetoe::link_layer::delta_time max_cb_runtime )
            {
                return Hardware::schedule_user_timer( []( void* that ){
                    const auto this_ = static_cast< nrf52_radio_base* >( that );
                    const auto callbacks = static_cast< CallBacks* >( this_ );

                    callbacks->user_timer();

                }, timer.usec(), max_cb_runtime.usec() );
            }

            bool cancel_synchronized_user_timer()
            {
                return Hardware::stop_user_timer();
            }

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
            {
                Hardware::set_access_address_and_crc_init( access_address, crc_init );
            }

            void run()
            {
                while ( !adv_received_ && !adv_timeout_ && !evt_timeout_ && !end_evt_ && wake_up_ == 0 )
                    __WFI();

                if ( adv_received_ )
                {
                    assert( reinterpret_cast< std::uint8_t* >( NRF_RADIO->PACKETPTR ) == receive_buffer_.buffer );

                    receive_buffer_.size = std::min< std::size_t >(
                        receive_buffer_.size,
                        ( receive_buffer_.buffer[ 1 ] & 0x3f ) + 2 + Hardware::pdu_gap_required_by_encryption()
                    );
                    adv_received_ = false;

                    static_cast< CallBacks* >( this )->adv_received( receive_buffer_ );
                }

                if ( adv_timeout_ )
                {
                    adv_timeout_ = false;
                    static_cast< CallBacks* >( this )->adv_timeout();
                }

                if ( evt_timeout_ )
                {
                    evt_timeout_ = false;
                    static_cast< CallBacks* >( this )->timeout();
                }

                if ( end_evt_ )
                {
                    end_evt_ = false;
                    static_cast< CallBacks* >( this )->end_event( events_ );

                    events_ = link_layer::connection_event_events();
                }

                if ( wake_up_ )
                {
                    --wake_up_;
                }
            }

            void wake_up()
            {
                ++wake_up_;
            }

            std::uint32_t static_random_address_seed() const
            {
                return Hardware::static_random_address_seed();
            }

            void nrf_flash_memory_access_begin()
            {
                lock_guard lock;
                Hardware::stop_radio();
                low_frequency_clock_t::stop_high_frequency_crystal_oscilator();
                state_ = state::idle;
            }

            void nrf_flash_memory_access_end()
            {
                // this kicks the CPU out of the loop in run() and requests the link layer to setup the next connection event
                evt_timeout_ = true;
            }

            void radio_set_phy(
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
                bluetoe::link_layer::details::phy_ll_encoding::phy_ll_encoding_t transmiting_c_encoding )
            {
                Hardware::set_phy( receiving_encoding, transmiting_c_encoding );
            }

            using lock_guard = typename Hardware::lock_guard;

            /**
             * @brief indicates support for 2Mbit
             */
            static constexpr bool hardware_supports_2mbit = true;

            /**
             * @brief indicates support for schedule_synchronized_user_timer()
             */
            static constexpr bool hardware_supports_synchronized_user_timer = true;

            static constexpr unsigned connection_event_setup_time_us = nrf52_radio_base::start_event_safety_margin_us;

        private:
            using low_frequency_clock_t = typename bluetoe::details::find_by_meta_type<
                nrf::nrf_details::sleep_clock_source_meta_type,
                RadioOptions...,
                bluetoe::nrf::calibrated_rc_sleep_clock >::type;

            using hfxo_startup_time_t = typename bluetoe::details::find_by_meta_type<
                nrf::nrf_details::hfxo_startup_time_meta_type,
                RadioOptions...,
                bluetoe::nrf::high_frequency_crystal_oscillator_startup_time_default >::type;

            /*
             * The startup time of the HFXO in LF clock periods
             */
            static constexpr std::uint32_t hfxo_startup_value =
                ( hfxo_startup_time_t::value + 1000000 / nrf::lfxo_clk_freq ) * nrf::lfxo_clk_freq / 1000000;

            /*
             * The startup time rounded up to the next full period of the sleep clock
             */
            static constexpr std::uint32_t hfxo_startup_time = hfxo_startup_value * 1000000 / nrf::lfxo_clk_freq;

            /*
             * Margin required to safetely setup the connection event in µs.
             */
            static constexpr std::uint32_t start_event_safety_margin_us = setup_connection_event_limit_us + hfxo_startup_time;


            link_layer::read_buffer receive_buffer()
            {
                link_layer::read_buffer result = this->allocate_receive_buffer();
                if ( result.empty() )
                    result = link_layer::read_buffer{ &empty_receive_[ 0 ], sizeof( empty_receive_ ) };

                return result;
            }

            void radio_interrupt_handler()
            {
                if ( state_ == state::adv_transmitting )
                {
                    // The timeout timer was already set with the start of the advertising
                    // Configure Radio to receive and then switch to transmitting
                    Hardware::configure_receive_train( receive_buffer_ );
                    state_ = state::adv_receiving;
                }
                else if ( state_ == state::adv_receiving )
                {
                    bool valid_anchor, valid_pdu;
                    std::tie( valid_anchor, valid_pdu ) = Hardware::received_pdu();

                    if ( valid_anchor && valid_pdu )
                    {
                        Hardware::stop_timeout_timer();

                        if ( is_valid_scan_request() )
                        {
                            Hardware::configure_final_transmit( response_data_ );
                            state_ = state::adv_transmitting_response;

                            return;
                        }

                        adv_received_ = true;

                        // In case of an connection request, the anchor is exactly the end
                        // of the request
                        Hardware::store_timer_anchor( 0 );
                    }
                    else
                    {
                        adv_timeout_ = true;
                    }

                    Hardware::stop_radio();

                    state_       = state::idle;
                }
                else if ( state_ == state::adv_transmitting_response )
                {
                    Hardware::stop_radio();

                    state_       = state::idle;
                    adv_timeout_ = true;
                }
                else if ( state_ == state::evt_wait_connect )
                {
                    bool valid_anchor, valid_pdu;
                    std::tie( valid_anchor, valid_pdu ) = Hardware::received_pdu();

                    if ( valid_anchor )
                    {
                        // switch to transmission
                        const auto trans = ( receive_buffer_.buffer == &empty_receive_[ 0 ] || !valid_pdu )
                            ? this->next_transmit()
                            : this->received( receive_buffer_ );

                        // TODO: Hack to disable the more data flag, because this radio implementation is currently
                        // not able to do this (but it should be possible with the given hardware).
                        // Issue: #75 More Data not working
                        const_cast< std::uint8_t* >( trans.buffer )[ 0 ] = trans.buffer[ 0 ] & ~more_data_flag;

                        if ( trans.buffer[ 1 ] != 0 )
                            events_.last_transmitted_not_empty = true;

                        Hardware::configure_final_transmit( trans );
                        state_   = state::evt_transmiting_closing;

                        // TODO as we currently take the anchor from the end of the PDU, we should be
                        // sure that the length of the PDU is correct!
                        // Issue: #76 Taking Anchor from End of PDU
                        if ( valid_pdu )
                        {
                            // TODO: Couldn't we just capture the time at the start of the PDU?
                            // the timer was captured with the end event; the anchor is the start of the receiving.
                            // Additional to the ll PDU length there are 1 byte preamble, 4 byte access address, 2 byte LL header and 3 byte crc.
                            // Issue: #76 Taking Anchor from End of PDU
                            static constexpr std::size_t ll_pdu_overhead = 1 + 4 + 2 + 3;
                            const int total_pdu_length = ( receive_buffer_.buffer[ 1 ] + ll_pdu_overhead ) * 8;

                            // TODO: Anchor must also be taken, if PDU has a CRC error
                            // Issue: #76 Taking Anchor from End of PDU
                            Hardware::store_timer_anchor( -total_pdu_length );

                            if ( receive_buffer_.buffer[ 1 ] != 0 )
                                events_.last_received_not_empty = true;

                            if ( receive_buffer_.buffer[ 0 ] & more_data_flag )
                                events_.last_received_had_more_data = true;
                        }
                        else
                        {
                            events_.error_occured = true;
                        }
                    }
                    else
                    {
                        Hardware::stop_radio();
                        events_.error_occured = true;
                        evt_timeout_  = true;
                        state_        = state::idle;
                    }
                }
                else if ( state_ == state::evt_transmiting_closing )
                {
                    Hardware::stop_radio();
                    state_   = state::idle;
                    end_evt_ = true;
                }
                else
                {
                    assert(!"Invalid state");
                }
            }

            bool is_valid_scan_request() const
            {
                static constexpr std::uint8_t scan_request_size     = 12;
                static constexpr std::uint8_t scan_request_pdu_type = 0x03;
                static constexpr std::uint8_t pdu_type_mask         = 0x0F;
                static constexpr int          pdu_header_size       = 2;
                static constexpr int          addr_size             = 6;
                static constexpr std::uint8_t tx_add_mask           = 0x40;
                static constexpr std::uint8_t rx_add_mask           = 0x80;

                // the advertising type does not expect a scan request
                if ( !response_data_.buffer )
                    return false;

                if ( Hardware::resolving_address_invalid() )
                    return true;

                if ( receive_buffer_.buffer[ 1 ] != scan_request_size )
                    return false;

                if ( ( receive_buffer_.buffer[ 0 ] & pdu_type_mask ) != scan_request_pdu_type )
                    return false;

                const int pdu_gap = Hardware::pdu_gap_required_by_encryption();

                if ( !std::equal( &receive_buffer_.buffer[ pdu_header_size + addr_size + pdu_gap ], &receive_buffer_.buffer[ pdu_header_size + 2 * addr_size + pdu_gap ],
                    &response_data_.buffer[ pdu_header_size + pdu_gap ] ) )
                    return false;

                // in the scan request, the randomness is stored in RxAdd, in the scan response, it's stored in
                // TxAdd.
                const bool scanner_addres_is_random = response_data_.buffer[ 0 ] & tx_add_mask;
                if ( !static_cast< bool >( receive_buffer_.buffer[ 0 ] & rx_add_mask ) == scanner_addres_is_random )
                    return false;

                const link_layer::device_address scanner( &receive_buffer_.buffer[ pdu_header_size + pdu_gap ], scanner_addres_is_random );

                return static_cast< const CallBacks* >( this )->is_scan_request_in_filter( scanner );
            }

            volatile bool adv_timeout_;
            volatile bool adv_received_;
            volatile bool evt_timeout_;
            volatile bool end_evt_;
            volatile int  wake_up_;

            enum class state {
                idle,
                // timeout while receiving, stopping the radio, waiting for the radio to become disabled
                adv_transmitting,
                adv_receiving,
                adv_transmitting_response,
                adv_shutting_down_radio,
                // connection event
                evt_wait_connect,
                evt_transmiting_closing,
            };

            volatile state                      state_;

            link_layer::read_buffer             receive_buffer_;
            link_layer::write_buffer            response_data_;
            std::uint8_t                        empty_receive_[ 3 ];
            link_layer::connection_event_events events_;
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            bool EnabledEncryption,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio;

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > :
            public nrf52_radio_base< CallBacks, Hardware, bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > >,
                                    RadioOptions... >
        {
        public:
            static constexpr bool hardware_supports_encryption = false;

            void increment_receive_packet_counter() {}
            void increment_transmit_packet_counter() {}
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > :
            public nrf52_radio_base<
                CallBacks,
                Hardware,
                bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize,
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >,
                    RadioOptions... >,
            public security_tool_box
        {
        public:
            static constexpr bool hardware_supports_lesc_pairing    = true;
            static constexpr bool hardware_supports_legacy_pairing  = true;
            static constexpr bool hardware_supports_encryption      = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

            using radio_base_t = nrf52_radio_base<
                CallBacks,
                Hardware,
                bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize,
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >,
                    RadioOptions...  >;

            nrf52_radio() : radio_base_t( encrypted_message_.data )
            {
            }

            std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm )
            {
                return Hardware::setup_encryption( key, skdm, ivm );
            }

            /**
             * @brief start the encryption of received PDUs with the next connection event.
             */
            void start_receive_encrypted()
            {
                Hardware::configure_encryption( true, false );
            }

            /**
             * @brief start to encrypt transmitted PDUs with the next connection event.
             */
            void start_transmit_encrypted()
            {
                Hardware::configure_encryption( true, true );
            }

            /**
             * @brief stop receiving encrypted with the next connection event.
             */
            void stop_receive_encrypted()
            {
                Hardware::configure_encryption( false, true );
            }

            /**
             * @brief stop transmitting encrypted with the next connection event.
             */
            void stop_transmit_encrypted()
            {
                Hardware::configure_encryption( false, false );
            }

            void increment_receive_packet_counter()
            {
                Hardware::increment_receive_packet_counter();
            }

            void increment_transmit_packet_counter()
            {
                Hardware::increment_transmit_packet_counter();
            }

            /**
             * @brief sets an IRK filter for incomming scan requests and connection requests
             *
             * Has to be called when the radio is not in a connection or after disconnected.
             * Experimental!
             */
            void set_identity_resolving_key( const details::identity_resolving_key_t& irk )
            {
                Hardware::set_identity_resolving_key( irk );
            }

        private:
            // TODO should be calculated with more accuracy base on the configuration of:
            // - l2cap MAX MTU
            // - implementation of Data Length Update procedure
            struct alignas( 4 ) encrypted_message_t {
                std::uint8_t data[ 260 ];
            } encrypted_message_;
        };

        template < typename Server, bool EnabledEncryption, typename RadioOptions, typename LinkLayerOptions >
        struct link_layer_factory;

        template < typename Server, bool EnabledEncryption, typename ... RadioOptions, typename ... LinkLayerOptions >
        struct link_layer_factory< Server, EnabledEncryption, std::tuple< RadioOptions... >, std::tuple< LinkLayerOptions... > >
        {
            using radio_hardware_t = typename details::select_type<
                EnabledEncryption,
                radio_hardware_with_crypto_support,
                radio_hardware_without_crypto_support >::type;

            template <
                std::size_t TransmitSize,
                std::size_t ReceiveSize,
                class CallBacks
            >
            using radio_t = nrf52_radio< TransmitSize, ReceiveSize, EnabledEncryption, CallBacks, radio_hardware_t, RadioOptions... >;

            using link_layer = bluetoe::link_layer::link_layer< Server, radio_t, LinkLayerOptions... >;
        };
    }

    namespace link_layer {

        /** @cond HIDDEN_SYMBOLS */
        /*
         * specialize pdu_layout_by_radio<> for the radio that supports encryption to change the PDU layout
         * to have that extra byte between header and body
         */
        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        struct pdu_layout_by_radio<
            nrf52_details::nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >
        {
            /**
             * When using encryption, the Radio and the AES CCM peripheral expect an "RFU" byte between LL header and
             * payload.
             */
            using pdu_layout = bluetoe::nrf_details::encrypted_pdu_layout;
        };
        /** @endcond */
    }

    /**
     * @brief binding for nRF52 microcontrollers
     *
     * Options that are ment to configure the nRF52 bindings are:
     * bluetoe::nrf::sleep_clock_crystal_oscillator
     * bluetoe::nrf::calibrated_sleep_clock
     * bluetoe::nrf::synthesized_sleep_clock
     */
    template < class Server, typename ... Options >
    using nrf52 = typename nrf52_details::link_layer_factory<
        Server,
        details::requires_encryption_support_t< Server >::value,
        typename nrf52_details::radio_options< Options... >::result,
        typename nrf52_details::link_layer_options< Options... >::result
    >::link_layer;
}

#endif
