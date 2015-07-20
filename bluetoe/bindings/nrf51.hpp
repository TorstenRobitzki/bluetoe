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
        class callbacks
        {
        public:
            virtual void received( const link_layer::read_buffer& receive ) = 0;
            virtual void timeout() = 0;

            virtual ~callbacks() {}
        };

        class scheduled_radio_base
        {
        public:
            explicit scheduled_radio_base( callbacks& );

            void schedule_transmit_and_receive(
                unsigned                        channel,
                const link_layer::write_buffer& transmit,
                link_layer::delta_time          when,
                const link_layer::read_buffer&  receive );

            void schedule_receive_and_transmit(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             when,
                bluetoe::link_layer::delta_time             window_size,
                const bluetoe::link_layer::read_buffer&     receive,
                const bluetoe::link_layer::write_buffer&    answert );

            void run();

            std::uint32_t static_random_address_seed() const;
        private:

            friend void ::RADIO_IRQHandler(void);
            friend void ::TIMER0_IRQHandler(void);

            void radio_interrupt();
            void timer_interrupt();

            unsigned frequency_from_channel( unsigned channel ) const;

            callbacks& callbacks_;
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

        template < typename CallBack >
        class scheduled_radio : public callbacks, public scheduled_radio_base
        {
        public:
            scheduled_radio() : scheduled_radio_base( static_cast< callbacks& >( *this ) )
            {
            }

        private:
            void received( const link_layer::read_buffer& receive )
            {
                static_cast< CallBack* >( this )->received( receive );
            }

            void timeout()
            {
                static_cast< CallBack* >( this )->timeout();
            }
        };
    }

    template < class Server, typename ... Options >
    using nrf51 = link_layer::link_layer< Server, nrf51_details::scheduled_radio, Options... >;
}

#endif // include guard
