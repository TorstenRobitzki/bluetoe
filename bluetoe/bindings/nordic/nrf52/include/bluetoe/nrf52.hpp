#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>

#include <nrf.h>

/*
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
 */
namespace bluetoe
{
    namespace nrf52_details
    {
        /*
         * Some aliases that can be used in the debugger
         */
        static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
        static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
        static NRF_CCM_Type* const          nrf_ccm              = NRF_CCM;
        static NRF_AAR_Type* const          nrf_aar              = NRF_AAR;
        static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;
        static NRF_RNG_Type* const          nrf_random           = NRF_RNG;
        static NRF_ECB_Type* const          nrf_aes              = NRF_ECB;
        static NVIC_Type* const             nvic                 = NVIC;

        static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;
        // position of the connecting address (AdvA)
        static constexpr unsigned           connect_addr_offset          = 2 + 6;

        // Time reserved to setup a connection event in µs
        // time measured to setup a connection event, using GCC 8.3.1 with -O0 is 12µs
        static constexpr std::uint32_t      setup_connection_event_limit_us = 50;

        // after T_IFS (150µs +- 2) at maximum, a connection request will be received (34 Bytes + 1 Byte preable, 4 Bytes Access Address and 3 Bytes CRC)
        // plus some additional 20µs
        static constexpr std::uint32_t      adv_reponse_timeout_us       = 152 + 42 * 8 + 20;
        static constexpr unsigned           us_radio_rx_startup_time     = 138;
        static constexpr unsigned           us_radio_tx_startup_time     = 140;

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

            static void capture_timer_anchor();

            /**
             * @brief store the captured time anchor
             */
            static void store_timer_anchor( int offset_us );

            /**
             * @brief returns true, if a valid PDU was received
             */
            static bool received_pdu();

            static std::uint32_t now();

            static void setup_identity_resolving(
                const std::uint8_t* )
            {
            }

            /**
             * @brief triggers the radio.start task at when, disables the radio timeout_us later
             */
            static void schedule_transmission(
                bluetoe::link_layer::delta_time when,
                std::uint32_t                   timeout_us );

            static void schedule_reception(
                std::uint32_t                   begin_us,
                std::uint32_t                   end_us );

            /**
             * @brief stop the timer from disabling the RADIO
             */
            static void stop_timeout_timer();

            static std::uint32_t static_random_address_seed();

            static void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            static void debug_toggle();

            // TODO should this be inlined?
            // - test code size with and without inlining
            class lock_guard
            {
            public:
                lock_guard();
                ~lock_guard();

                lock_guard( const lock_guard& ) = delete;
                lock_guard& operator=( const lock_guard& ) = delete;
            private:
                const std::uint32_t context_;
            };

        private:

            static bluetoe::link_layer::delta_time anchor_offset_;
        };

        /**
         * @brief set of security tool box functions, both for legacy pairing and LESC pairing
         *
         * It's expected that only the required set of functions are requested by the linker.
         */
        class nrf52_security_tool_box
        {
        public:
            /**
             * security tool box required by legacy pairing
             */
            bluetoe::details::uint128_t create_srand();

            bluetoe::details::longterm_key_t create_long_term_key();

            bluetoe::details::uint128_t c1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& rand,
                const bluetoe::details::uint128_t& p1,
                const bluetoe::details::uint128_t& p2 ) const;

            bluetoe::details::uint128_t s1(
                const bluetoe::details::uint128_t& temp_key,
                const bluetoe::details::uint128_t& srand,
                const bluetoe::details::uint128_t& mrand );

            /**
             * security tool box required by LESC pairing
             */
            bool is_valid_public_key( const std::uint8_t* public_key ) const;

            std::pair< bluetoe::details::ecdh_public_key_t, bluetoe::details::ecdh_private_key_t > generate_keys();

            bluetoe::details::uint128_t select_random_nonce();

            bluetoe::details::ecdh_shared_secret_t p256( const std::uint8_t* private_key, const std::uint8_t* public_key );

            bluetoe::details::uint128_t f4( const std::uint8_t* u, const std::uint8_t* v, const std::array< std::uint8_t, 16 >& k, std::uint8_t z );

            std::pair< bluetoe::details::uint128_t, bluetoe::details::uint128_t > f5(
                const bluetoe::details::ecdh_shared_secret_t dh_key,
                const bluetoe::details::uint128_t& nonce_central,
                const bluetoe::details::uint128_t& nonce_periperal,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            bluetoe::details::uint128_t f6(
                const bluetoe::details::uint128_t& key,
                const bluetoe::details::uint128_t& n1,
                const bluetoe::details::uint128_t& n2,
                const bluetoe::details::uint128_t& r,
                const bluetoe::details::io_capabilities_t& io_caps,
                const bluetoe::link_layer::device_address& addr_controller,
                const bluetoe::link_layer::device_address& addr_peripheral );

            std::uint32_t g2(
                const std::uint8_t*                 u,
                const std::uint8_t*                 v,
                const bluetoe::details::uint128_t&  x,
                const bluetoe::details::uint128_t&  y );

            /**
             * Functions required by IO capabilties
             */
            bluetoe::details::uint128_t create_passkey();
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

            static void enable_ccm();
            static void disable_ccm();
            static void configure_encryption( bool receive, bool transmit );

            static std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm );

            static void increment_receive_packet_counter()
            {
                rx_counter_.increment();
            }

            static void increment_transmit_packet_counter()
            {
                rx_counter_.increment();
            }

        private:
            static bool             receive_encrypted_;
            static bool             transmit_encrypted_;
            static std::uint8_t*    encrypted_area_;
            static counter          tx_counter_;
            static counter          rx_counter_;
        };

        template < typename ... Options >
        struct radio_options
        {
            using result = typename bluetoe::details::find_all_by_meta_type<
                bluetoe::nrf::details::nrf52_radio_option_meta_type,
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

        template < class CallBacks, class Hardware, class Buffer >
        class nrf52_radio_base : public Buffer
        {
        public:
            nrf52_radio_base()
            {
                Hardware::init( []( void* that ){
                    static_cast< nrf52_radio_base* >( that )->radio_interrupt_handler();
                }, this);
            }

            nrf52_radio_base( std::uint8_t* receive_buffer )
            {
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
                assert( response_data.buffer );

                bluetoe::link_layer::write_buffer advertising = advertising_data;
                advertising.size = std::min< std::uint8_t >( advertising.size, maximum_advertising_pdu_size );

                response_data_       = response_data;
                receive_buffer_      = receive;
                receive_buffer_.size = std::min< std::size_t >( receive.size, maximum_advertising_pdu_size );

                Hardware::configure_radio_channel( channel );
                Hardware::configure_transmit_train( advertising );

                // TODO: Move to somewhere else? Conditional?
                Hardware::setup_identity_resolving( receive_buffer_.buffer + connect_addr_offset );

                // Advertising size + LL header + preable + access address + CRC
                const std::uint32_t read_timeout = ( advertising.size + 2 + 1 + 4 + 3 ) * 8 + adv_reponse_timeout_us;

                state_ = state::adv_transmitting;

                Hardware::schedule_transmission( when, read_timeout );
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

                const std::uint32_t start_event = start_receive.usec() - us_radio_rx_startup_time;
                const std::uint32_t end_event   = end_receive.usec() + 500; // TODO: 500: must depend on receive size.

                const auto now = Hardware::now();
                if ( now + setup_connection_event_limit_us > start_event )
                {
                    evt_timeout_ = true;
                    return connection_interval;
                }

                receive_buffer_ = receive_buffer();
                state_          = state::evt_wait_connect;

                Hardware::configure_radio_channel( channel );
                Hardware::configure_receive_train( receive_buffer_ );

                Hardware::schedule_reception( start_event, end_event );

                return bluetoe::link_layer::delta_time( connection_interval.usec() - now );
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
                    static_cast< CallBacks* >( this )->end_event();
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

            using lock_guard = typename Hardware::lock_guard;

        private:
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
                    if ( Hardware::received_pdu() )
                    {
                        Hardware::stop_timeout_timer();

                        if ( is_valid_scan_request() )
                        {
                            Hardware::configure_final_transmit( response_data_ );
                            state_ = state::adv_transmitting_response;

                            return;
                        }
                        else
                        {
                            adv_received_ = true;
                        }
                    }
                    else
                    {
                        Hardware::capture_timer_anchor();
                        adv_timeout_ = true;
                    }

                    Hardware::stop_radio();
                    Hardware::store_timer_anchor( 0 );

                    state_       = state::idle;
                }
                else if ( state_ == state::adv_transmitting_response )
                {
                    state_       = state::idle;
                    adv_timeout_ = true;
                }
                else if ( state_ == state::evt_wait_connect )
                {
                    if ( Hardware::received_pdu() )
                    {
                        // switch to transmission
                        const auto trans = ( receive_buffer_.buffer == &empty_receive_[ 0 ] )
                            ? this->next_transmit()
                            : this->received( receive_buffer_ );

                        // TODO: Hack to disable the more data flag, because this radio implementation is currently
                        // not able to do this (but it should be possible with the given hardware).
                        const_cast< std::uint8_t* >( trans.buffer )[ 0 ] = trans.buffer[ 0 ] & ~more_data_flag;

                        Hardware::configure_final_transmit( trans );
                        state_   = state::evt_transmiting_closing;

                        // TODO: Couldn't we just capture the time at the start of the PDU?
                        // the timer was captured with the end event; the anchor is the start of the receiving.
                        // Additional to the ll PDU length there are 1 byte preamble, 4 byte access address, 2 byte LL header and 3 byte crc.
                        static constexpr std::size_t ll_pdu_overhead = 1 + 4 + 2 + 3;
                        const int total_pdu_length = ( receive_buffer_.buffer[ 1 ] + ll_pdu_overhead ) * 8;

                        // TODO: Anchor must also be taken, if PDU has a CRC error
                        Hardware::store_timer_anchor( -total_pdu_length );
                    }
                    else
                    {
                        Hardware::stop_radio();
                        evt_timeout_  = true;
                        state_        = state::idle;
                    }
                }
                else if ( state_ == state::evt_transmiting_closing )
                {
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

                /* TODO
                if ( identity_resolving_enabled )
                {
                    while ( !nrf_aar->EVENTS_END )
                        ;

                    if ( nrf_aar->EVENTS_NOTRESOLVED )
                        return false;
                } */

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

            static constexpr unsigned connection_event_type_base = 100;

            enum class state {
                idle,
                // timeout while receiving, stopping the radio, waiting for the radio to become disabled
                adv_transmitting,
                adv_receiving,
                adv_transmitting_response,
                adv_shutting_down_radio,
                // connection event
                evt_wait_connect    = connection_event_type_base,
                evt_transmiting_closing,
            };

            volatile state                  state_;

            link_layer::read_buffer         receive_buffer_;
            link_layer::write_buffer        response_data_;
            std::uint8_t                    empty_receive_[ 3 ];
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
            public nrf52_radio_base< CallBacks, Hardware, bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > > >
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
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > > >,
            public nrf52_security_tool_box
        {
        public:
            static constexpr bool hardware_supports_lesc_pairing    = true;
            static constexpr bool hardware_supports_legacy_pairing  = true;
            static constexpr bool hardware_supports_encryption      = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

            using radio_base_t = nrf52_radio_base<
                CallBacks,
                Hardware,
                bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize,
                    nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > > >;

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
                Hardware::enable_ccm();
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
                Hardware::disable_ccm();
            }

            void increment_receive_packet_counter()
            {
                Hardware::increment_receive_packet_counter();
            }

            void increment_transmit_packet_counter()
            {
                Hardware::increment_transmit_packet_counter();
            }

        private:
            static constexpr std::size_t att_mtu = bluetoe::details::find_by_meta_type<
                bluetoe::link_layer::details::mtu_size_meta_type,
                RadioOptions...,
                bluetoe::link_layer::max_mtu_size< bluetoe::details::default_att_mtu_size >
            >::type::mtu;

            static constexpr std::size_t l2cap_mtu      = att_mtu + 4;
            static constexpr std::size_t enrypted_size  = l2cap_mtu + 3 + 4;

            struct alignas( 4 ) encrypted_message_t {
                std::uint8_t data[ enrypted_size ];
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
