#ifndef BLUETOE_BINDINGS_NRF51_HPP
#define BLUETOE_BINDINGS_NRF51_HPP

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/buffer.hpp>
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

            virtual ~adv_callbacks() {}
        };

        class scheduled_radio_base
        {
        public:
            explicit scheduled_radio_base( adv_callbacks& );

            void schedule_advertisment_and_receive(
                unsigned                        channel,
                const link_layer::write_buffer& transmit,
                link_layer::delta_time          when,
                const link_layer::read_buffer&  receive );

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            void run();

            std::uint32_t static_random_address_seed() const;
        private:

            friend void ::RADIO_IRQHandler(void);
            friend void ::TIMER0_IRQHandler(void);

            void radio_interrupt();
            void timer_interrupt();

            unsigned frequency_from_channel( unsigned channel ) const;

            adv_callbacks& callbacks_;
            volatile bool timeout_;
            volatile bool received_;

            enum class state {
                idle,
                // timeout while receiving, stopping the radio, waiting for the radio to become disabled
                timeout_stopping,
                transmitting,
                // wait until the right time to transmit
                transmitting_pending,
                receiving,
            };

            volatile state state_;

            link_layer::read_buffer receive_buffer_;
        };

        template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
        class scheduled_radio :
            public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, scheduled_radio< TransmitSize, ReceiveSize, CallBack > >,
            private adv_callbacks,
            public scheduled_radio_base
        {
        public:
            scheduled_radio() : scheduled_radio_base( static_cast< adv_callbacks& >( *this ) )
            {
            }

            void schedule_connection_event(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             start_receive,
                bluetoe::link_layer::delta_time             end_receive,
                bluetoe::link_layer::delta_time             connection_interval );

        private:
            void adv_received( const link_layer::read_buffer& receive )
            {
                static_cast< CallBack* >( this )->adv_received( receive );
            }

            void adv_timeout()
            {
                static_cast< CallBack* >( this )->adv_timeout();
            }
        };


        // implementation
        template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
        void scheduled_radio< TransmitSize, ReceiveSize, CallBack >::schedule_connection_event(
            unsigned                                    channel,
            bluetoe::link_layer::delta_time             start_receive,
            bluetoe::link_layer::delta_time             end_receive,
            bluetoe::link_layer::delta_time             connection_interval )
        {
        }

    }

    template < class Server, typename ... Options >
    using nrf51 = link_layer::link_layer< Server, nrf51_details::scheduled_radio, Options... >;
}

#endif // include guard
