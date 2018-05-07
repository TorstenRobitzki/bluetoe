#ifndef BLUETOE_BINDINGS_NRF51_HPP
#define BLUETOE_BINDINGS_NRF51_HPP

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/ll_data_pdu_buffer.hpp>
#include <cstdint>

extern "C" void RADIO_IRQHandler(void);
extern "C" void TIMER0_IRQHandler(void);

namespace bluetoe
{
    namespace nrf51_details
    {
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

        protected:
            bluetoe::link_layer::delta_time start_connection_event(
                unsigned                        channel,
                bluetoe::link_layer::delta_time start_receive,
                bluetoe::link_layer::delta_time end_receive,
                const link_layer::read_buffer&  receive_buffer );
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
            std::uint8_t                    empty_receive_[ 2 ];
        };

        class scheduled_radio_base_with_encryption : public scheduled_radio_base
        {
        public:
            explicit scheduled_radio_base_with_encryption( adv_callbacks& cbs );

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

            void start_encryption();

            void stop_encryption();
        private:
        };

        template < typename Base >
        struct scheduled_radio_factory
        {
            template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
            class scheduled_radio :
                public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack > >,
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
                using buffer = bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack > >;

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

                bool is_scan_request_in_filter_callback( const link_layer::device_address& addr ) const override
                {
                    return static_cast< const CallBack* >( this )->is_scan_request_in_filter( addr );
                }

            };
        };

    }

    template < class Server, typename ... Options >
    using nrf51 = link_layer::link_layer< Server, nrf51_details::template scheduled_radio_factory<
        nrf51_details::scheduled_radio_base >::scheduled_radio, Options... >;

    template < class Server, typename ... Options >
    using nrf51_with_encryption = link_layer::link_layer< Server, nrf51_details::template scheduled_radio_factory<
        nrf51_details::scheduled_radio_base_with_encryption >::scheduled_radio, Options... >;
}

#endif // include guard
