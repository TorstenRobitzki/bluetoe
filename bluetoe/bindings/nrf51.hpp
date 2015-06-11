#ifndef BLUETOE_BINDINGS_NRF51_HPP
#define BLUETOE_BINDINGS_NRF51_HPP

#include <bluetoe/link_layer/link_layer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/buffer.hpp>
#include <cstdint>

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
                    unsigned channel,
                    const link_layer::write_buffer& transmit, link_layer::delta_time when,
                    const link_layer::read_buffer& receive, link_layer::delta_time timeout );

        private:
            callbacks& callbacks_;
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

    template < class Server >
    using nrf51 = link_layer::link_layer< Server, nrf51_details::scheduled_radio >;
}

#endif // include guard
