#ifndef BLUETOE_BINDINGS_NRF52_HPP
#define BLUETOE_BINDINGS_NRF52_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/nrf.hpp>
#include <bluetoe/meta_tools.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>

#include <nrf.h>

namespace bluetoe
{
    namespace nrf52_details
    {
        static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
        static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
        static NRF_CCM_Type* const          nrf_ccm              = NRF_CCM;
        static NRF_AAR_Type* const          nrf_aar              = NRF_AAR;
        static NVIC_Type* const             nvic                 = NVIC;
        static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;

        static constexpr std::uint8_t       maximum_advertising_pdu_size = 0x3f;
        // position of the connecting address (AdvA)
        static constexpr unsigned           connect_addr_offset          = 2 + 6;

        // after T_IFS (150µs +- 2) at maximum, a connection request will be received (34 Bytes + 1 Byte preable, 4 Bytes Access Address and 3 Bytes CRC)
        // plus some additional 20µs
        static constexpr std::uint32_t      adv_reponse_timeout_us       = 152 + 42 * 8 + 20;
        static constexpr unsigned           us_radio_tx_startup_time     = 140;

        /**
         * @brief abstraction of the hardware that can be replaced during tests
         */
        class radio_hardware_with_crypto_support
        {
        public:
            static int pdu_gap_required_by_encryption()
            {
                return 1;
            }

            static void init();
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

            static void configure_final_transmit(
                const bluetoe::link_layer::write_buffer&    transmit_data );

            /**
             * @brief configure radio to transmit data and then switch to
             *        receving.
             */
            static void configure_receive_train(
                const bluetoe::link_layer::read_buffer&     receive_buffer );

            static void stop_radio();

            /**
             * @brief store the captured time anchor
             */
            static void store_timer_anchor();

            /**
             * @brief returns true, if a valid PDU was received
             */
            static bool received_pdu();

            static void setup_identity_resolving(
                const std::uint8_t* )
            {
            }

            /**
             * @brief triggers the radio.start task at when, disables the radio timeout_us later
             */
            static void schedule_radio_start(
                bluetoe::link_layer::delta_time when,
                std::uint32_t                   timeout_us );

            static std::uint32_t static_random_address_seed();

            static void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

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

        template < class CallBacks, class Hardware >
        class nrf52_radio_base
        {
        public:
            nrf52_radio_base()
            {
                Hardware::init( []( void* that ){
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

                const std::uint32_t read_timeout = ( advertising.size + 1 + 4 + 3 ) * 8 + adv_reponse_timeout_us;

                state_ = state::adv_transmitting;

                Hardware::schedule_radio_start( when, read_timeout );
            }

            bluetoe::link_layer::delta_time schedule_connection_event(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             start_receive,
                bluetoe::link_layer::delta_time             end_receive,
                bluetoe::link_layer::delta_time             connection_interval )
            {
                (void)channel;
                (void)start_receive;
                (void)end_receive;
                (void)connection_interval;
                return bluetoe::link_layer::delta_time();
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
            void radio_interrupt_handler()
            {
                if ( state_ == state::adv_transmitting )
                {
                    Hardware::configure_receive_train( receive_buffer_ );
                    state_ = state::adv_receiving;
                }
                else if ( state_ == state::adv_receiving )
                {
                    Hardware::store_timer_anchor();

                    if ( Hardware::received_pdu() )
                    {
                        if ( is_valid_scan_request() )
                        {
                            Hardware::configure_final_transmit( response_data_ );
                            state_ = state::adv_transmitting_response;
                        }
                        else
                        {
                            Hardware::stop_radio();
                            state_        = state::idle;
                            adv_received_ = true;
                        }
                    }
                    else
                    {
                        Hardware::stop_radio();
                        state_       = state::idle;
                        adv_timeout_ = true;
                    }

                }
                else if ( state_ == state::adv_transmitting_response )
                {
                    Hardware::stop_radio();
                    state_       = state::idle;
                    adv_timeout_ = true;
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
                // connection event
                evt_wait_connect    = connection_event_type_base,
                evt_transmiting_closing,
            };

            volatile state                  state_;

            link_layer::read_buffer         receive_buffer_;
            link_layer::write_buffer        response_data_;
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
            public nrf52_radio_base< CallBacks, Hardware >,
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, false, CallBacks, Hardware, RadioOptions... > >
        {
        public:
            static constexpr bool hardware_supports_encryption = false;
        };

        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBacks,
            class Hardware,
            typename ... RadioOptions
        >
        class nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > :
            public nrf52_radio_base< CallBacks, Hardware >,
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, nrf52_radio< TransmitSize, ReceiveSize, true, CallBacks, Hardware, RadioOptions... > >
        {
        public:
            static constexpr bool hardware_supports_lesc_pairing = true;
            static constexpr bool hardware_supports_legacy_pairing = true;
            static constexpr bool hardware_supports_encryption = hardware_supports_lesc_pairing || hardware_supports_legacy_pairing;

            std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm );

            /**
             * @brief start the encryption of received PDUs with the next connection event.
             */
            void start_receive_encrypted();

            /**
             * @brief start to encrypt transmitted PDUs with the next connection event.
             */
            void start_transmit_encrypted();

            /**
             * @brief stop receiving encrypted with the next connection event.
             */
            void stop_receive_encrypted();

            /**
             * @brief stop transmitting encrypted with the next connection event.
             */
            void stop_transmit_encrypted();
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
