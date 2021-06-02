#ifndef BLUETOE_BINDINGS_NRF51_HPP
#define BLUETOE_BINDINGS_NRF51_HPP

#include <bluetoe/link_layer.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <cstdint>

extern "C" void RADIO_IRQHandler(void);
extern "C" void TIMER0_IRQHandler(void);

namespace bluetoe
{
    namespace nrf51_details
    {
        /* Counter used for CCM */
        struct counter {
            std::uint32_t   low;
            std::uint8_t    high;

            // set to zero
            counter();

            void increment();
            void copy_to( std::uint8_t* target ) const;
        };

        // map compile time callbacks to runtime callbacks for faster development cycles.
        class adv_callbacks
        {
        public:
            virtual void adv_received( const link_layer::read_buffer& receive ) = 0;
            virtual void adv_timeout() = 0;
            virtual void timeout() = 0;
            virtual void end_event() = 0;

            virtual link_layer::write_buffer received_data( const link_layer::read_buffer& ) = 0;
            virtual link_layer::write_buffer next_transmit() = 0;
            virtual link_layer::read_buffer allocate_receive_buffer() = 0;
            virtual void load_transmit_counter() = 0;

            virtual bool is_scan_request_in_filter_callback( const link_layer::device_address& ) const = 0;
        };

        class scheduled_radio_base
        {
        public:
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

            scheduled_radio_base( adv_callbacks&, std::uint32_t encrypted_area );
            explicit scheduled_radio_base( adv_callbacks& );

            void schedule_advertisment(
                unsigned                        channel,
                const link_layer::write_buffer& advertising_data,
                const link_layer::write_buffer& response_data,
                link_layer::delta_time          when,
                const link_layer::read_buffer&  receive );

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            void run();

            void wake_up();

            std::uint32_t static_random_address_seed() const;

            // no native white list implementation atm
            static constexpr std::size_t radio_maximum_white_list_entries = 0;

            static constexpr bool hardware_supports_encryption = false;

            void increment_receive_packet_counter()
            {
            }

            void increment_transmit_packet_counter()
            {
            }

            /**
             * @brief experimental interface to be called from a connection event callback
             *
             * To be called, once the CPU will be stopped due to flash memory eraseing.
             */
            void nrf_flash_memory_access_begin();
            void nrf_flash_memory_access_end();

        protected:

            bluetoe::link_layer::delta_time start_connection_event_impl(
                unsigned                        channel,
                bluetoe::link_layer::delta_time start_receive,
                bluetoe::link_layer::delta_time end_receive,
                const link_layer::read_buffer&  receive_buffer );

            void configure_encryption( bool receive, bool transmit );

        private:
            friend void ::RADIO_IRQHandler(void);
            friend void ::TIMER0_IRQHandler(void);

            bool is_valid_scan_request() const;
            void stop_radio();

            void adv_radio_interrupt();
            void adv_timer_interrupt();
            void evt_radio_interrupt();
            void evt_timer_interrupt();
            void radio_interrupt();
            void timer_interrupt();

            unsigned frequency_from_channel( unsigned channel ) const;

            adv_callbacks& callbacks_;
            volatile bool timeout_;
            volatile bool received_;
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

            bluetoe::link_layer::delta_time anchor_offset_;

            link_layer::read_buffer         receive_buffer_;
            link_layer::write_buffer        response_data_;
            std::uint8_t                    empty_receive_[ 3 ];
            bool                            receive_encrypted_;
            bool                            transmit_encrypted_;
            std::uint32_t                   encrypted_area_;
        };

        class scheduled_radio_base_with_encryption_base : public scheduled_radio_base
        {
        protected:
            scheduled_radio_base_with_encryption_base( adv_callbacks& cbs, std::uint32_t scratch_area, std::uint32_t encrypted_area );

            void load_transmit_packet_counter();

            bluetoe::link_layer::delta_time start_connection_event(
                unsigned                        channel,
                bluetoe::link_layer::delta_time start_receive,
                bluetoe::link_layer::delta_time end_receive,
                const link_layer::read_buffer&  receive_buffer )
            {
                load_receive_packet_counter();
                return start_connection_event_impl( channel, start_receive, end_receive, receive_buffer );
            }

        public:
            static constexpr bool hardware_supports_encryption = true;

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

            std::pair< std::uint64_t, std::uint32_t > setup_encryption( bluetoe::details::uint128_t key, std::uint64_t skdm, std::uint32_t ivm );

            void start_receive_encrypted();
            void start_transmit_encrypted();
            void stop_receive_encrypted();
            void stop_transmit_encrypted();

            void increment_receive_packet_counter()
            {
                rx_counter_.increment();
            }

            void increment_transmit_packet_counter()
            {
                tx_counter_.increment();
            }

            /**
             * @brief sets an IRK filter for incomming scan requests and connection requests
             *
             * Has to be called when the radio is not in a connection or after disconnected.
             * Experimental!
             */
            void set_identity_resolving_key( const details::identity_resolving_key_t& irk );

        private:
            void load_receive_packet_counter();

            counter     tx_counter_;
            counter     rx_counter_;
        };

        /*
         * There are some data structures where the size depends on the configuration of the link layer (namely the
         * maximum MTU size).
         */
        template < typename ... Options >
        class scheduled_radio_base_with_encryption : public scheduled_radio_base_with_encryption_base
        {
        protected:
            scheduled_radio_base_with_encryption( adv_callbacks& cbs )
                : scheduled_radio_base_with_encryption_base( cbs,
                    reinterpret_cast< std::uintptr_t >( &scratch_area_.data[ 0 ] ),
                    reinterpret_cast< std::uintptr_t >( &encrypted_message_.data[ 0 ] ) )
            {
            }

        private:
            static constexpr std::size_t att_mtu = bluetoe::details::find_by_meta_type<
                bluetoe::link_layer::details::mtu_size_meta_type,
                Options...,
                bluetoe::link_layer::max_mtu_size< bluetoe::details::default_att_mtu_size >
            >::type::mtu;

            static constexpr std::size_t l2cap_mtu      = att_mtu + 4;
            // the value MAXPACKETSIZE from the documentation seems to be the maximum value, the size field can store,
            // and is independent from the MTU size (https://devzone.nordicsemi.com/f/nordic-q-a/13123/what-is-actual-size-required-for-scratch-area-for-ccm-on-nrf52/50031#50031)
            static constexpr std::size_t scratch_size   = 267;
            static constexpr std::size_t enrypted_size  = l2cap_mtu + 3 + 4;

            struct alignas( 4 ) scratch_area_t {
                std::uint8_t data[ scratch_size ];
            } scratch_area_;

            struct alignas( 4 ) encrypted_message_t {
                std::uint8_t data[ enrypted_size ];
            } encrypted_message_;
        };

        class scheduled_radio_without_encryption_base : public scheduled_radio_base
        {
        protected:
            bluetoe::link_layer::delta_time start_connection_event(
                unsigned                        channel,
                bluetoe::link_layer::delta_time start_receive,
                bluetoe::link_layer::delta_time end_receive,
                const link_layer::read_buffer&  receive_buffer )
            {
                return start_connection_event_impl( channel, start_receive, end_receive, receive_buffer );
            }

            scheduled_radio_without_encryption_base( adv_callbacks& cbs ) : scheduled_radio_base( cbs, 0 )
            {
            }

            void load_transmit_packet_counter()
            {
            }
        };

        template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack, typename Base >
        class scheduled_radio :
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack, Base > >,
            private adv_callbacks,
            public Base
        {
        public:
            scheduled_radio() : Base( static_cast< adv_callbacks& >( *this ) )
            {
            }

            bluetoe::link_layer::delta_time schedule_connection_event(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             start_receive,
                bluetoe::link_layer::delta_time             end_receive,
                bluetoe::link_layer::delta_time             /* connection_interval */ )
            {
                link_layer::read_buffer read;
                {
                    class Base::lock_guard lock;
                    read = buffer::allocate_receive_buffer();
                }

                return this->start_connection_event( channel, start_receive, end_receive, read );
            }
        private:
            using buffer = bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack, Base > >;

            void adv_received( const link_layer::read_buffer& receive ) override
            {
                static_cast< CallBack* >( this )->adv_received( receive );
            }

            void adv_timeout() override
            {
                static_cast< CallBack* >( this )->adv_timeout();
            }

            void timeout() override
            {
                static_cast< CallBack* >( this )->timeout();
            }

            void end_event() override
            {
                static_cast< CallBack* >( this )->end_event();
            }

            link_layer::write_buffer received_data( const link_layer::read_buffer& b ) override
            {
                // this function is called within an ISR context, so no need to disable interrupts
                return this->received( b );
            }

            link_layer::write_buffer next_transmit() override
            {
                // this function is called within an ISR context, so no need to disable interrupts
                return buffer::next_transmit();
            }

            link_layer::read_buffer allocate_receive_buffer() override
            {
                return buffer::allocate_receive_buffer();
            }

            void load_transmit_counter() override
            {
                this->load_transmit_packet_counter();
            }

            bool is_scan_request_in_filter_callback( const link_layer::device_address& addr ) const override
            {
                return static_cast< const CallBack* >( this )->is_scan_request_in_filter( addr );
            }
        };

        template < typename Base >
        struct scheduled_radio_factory
        {
            template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
            using scheduled_radio = nrf51_details::scheduled_radio< TransmitSize, ReceiveSize, CallBack, Base >;
        };
    } // namespace nrf51_details

    /*
     * nrf51 without encryption
     */
    template < class Server, typename ... Options >
    using nrf51_without_encryption = link_layer::link_layer<
        Server,
        nrf51_details::template scheduled_radio_factory<
            nrf51_details::scheduled_radio_without_encryption_base
        >::scheduled_radio,
        Options... >;

    /*
     * nrf51 with encryption
     */
    template < class Server, typename ... Options >
    using nrf51 = link_layer::link_layer<
        Server,
        nrf51_details::template scheduled_radio_factory<
            nrf51_details::scheduled_radio_base_with_encryption< Options... >
        >::template scheduled_radio,
        Options... >;

    namespace link_layer {

        template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack, typename ... Options >
        struct pdu_layout_by_radio<
            nrf51_details::scheduled_radio< TransmitSize, ReceiveSize, CallBack, nrf51_details::scheduled_radio_base_with_encryption< Options... > > >
        {
            /*
             * When using encryption, the Radio and the AES CCM peripheral expect an "RFU" byte between LL header and
             * payload.
             */
            struct pdu_layout : details::layout_base< pdu_layout > {
                static constexpr std::size_t header_size = sizeof( std::uint16_t );

                using bluetoe::link_layer::details::layout_base< pdu_layout >::header;

                static std::uint16_t header( const std::uint8_t* pdu )
                {
                    return ::bluetoe::details::read_16bit( pdu );
                }

                static void header( std::uint8_t* pdu, std::uint16_t header_value )
                {
                    ::bluetoe::details::write_16bit( pdu, header_value );
                }

                static std::pair< std::uint8_t*, std::uint8_t* > body( const read_buffer& pdu )
                {
                    assert( pdu.size >= header_size );

                    return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
                }

                static std::pair< const std::uint8_t*, const std::uint8_t* > body( const write_buffer& pdu )
                {
                    assert( pdu.size >= header_size );

                    return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
                }

                static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
                {
                    return header_size + payload_size + 1;
                }
            };
        };
   }

} // namespace bluetoe

#endif // include guard
