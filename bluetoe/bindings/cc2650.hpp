#ifndef BLUETOE_BINDINGS_CC2650_HPP
#define BLUETOE_BINDINGS_CC2650_HPP

#include "bluetoe/link_layer/link_layer.hpp"

namespace bluetoe
{
    namespace link_layer {
        class delta_time;
    }

    namespace cc2650_details
    {
        template <
            std::size_t TransmitSize,
            std::size_t ReceiveSize,
            class CallBack
        >
        class scheduled_radio
        {
        public:
            /* implementation of the scheduled radio interface */
            void schedule_advertisment_and_receive(
                unsigned                                    channel,
                const bluetoe::link_layer::write_buffer&    transmit,
                bluetoe::link_layer::delta_time             when,
                const bluetoe::link_layer::read_buffer&     receive );

            void schedule_connection_event(
                unsigned                                    channel,
                bluetoe::link_layer::delta_time             start_receive,
                bluetoe::link_layer::delta_time             end_receive,
                bluetoe::link_layer::delta_time             connection_interval );

            void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

            std::uint32_t static_random_address_seed() const;

            void run();

            void wake_up();

            class lock_guard;

            /* implementation of the ll_data_pdu_buffer interface, because the cc2650 implements
               a buffer */
            static constexpr std::size_t    size            = TransmitSize + ReceiveSize;

            /**
             * @brief returns the underlying raw buffer with a size of at least ll_data_pdu_buffer::size
             *
             * The underlying memory can be used when it's not used by other means. The size that can saftely be used is ll_data_pdu_buffer::size.
             * @pre buffer is in stopped mode
             */
            std::uint8_t* raw();

            /**
             * @brief places the buffer in stopped mode.
             *
             * If the buffer is in stopped mode, it's internal memory can be used for other
             * purposes.
             */
            void stop();

            /**
             * @brief places the buffer in running mode.
             *
             * Receive and transmit buffers are empty. Sequence numbers are reseted.
             * max_rx_size() is set to default
             *
             * @post next_received().empty()
             * @post max_rx_size() == 29u
             */
            void reset();

            /**
             * @brief allocates a certain amount of memory to place a PDU to be transmitted.
             *
             * If not enough memory is available, the function will return an empty buffer (size == 0).
             * To indicate that the allocated memory is filled with data to be send, commit_transmit_buffer() must be called.
             *
             * @post r = allocate_transmit_buffer( n ); r.size == 0 || r.size == n
             * @pre  buffer is in running mode
             * @pre size <= max_tx_size()
             */
            bluetoe::link_layer::read_buffer allocate_transmit_buffer( std::size_t size );

            /**
             * @brief calles allocate_transmit_buffer( max_tx_size() );
             */
            bluetoe::link_layer::read_buffer allocate_transmit_buffer();

            /**
             * @brief indicates that prior allocated memory is now ready for transmission
             *
             * To send a PDU, first allocate_transmit_buffer() have to be called, with the maximum size
             * need to assemble the PDU. Then commit_transmit_buffer() have to be called with the
             * size that the PDU is really filled with at the begining of the buffer.
             *
             * @pre a buffer must have been allocated by a call to allocate_transmit_buffer()
             * @pre size
             * @pre buffer is in running mode
             */
            void commit_transmit_buffer( bluetoe::link_layer::read_buffer );

            /**
             * @brief returns the oldest PDU out of the receive buffer.
             *
             * If there is no PDU in the receive buffer, an empty PDU will be returned (size == 0 ).
             * The returned PDU is not removed from the buffer. To remove the buffer after it is
             * not used any more, free_received() must be called.
             *
             * @pre buffer is in running mode
             */
            bluetoe::link_layer::write_buffer next_received() const;

            /**
             * @brief removes the oldest PDU from the receive buffer.
             * @pre next_received() was called without free_received() beeing called.
             * @pre buffer is in running mode
             */
            void free_received();

            // no native white list implementation atm
            static constexpr std::size_t radio_maximum_white_list_entries = 0;
        };
    }

    template < class Server, typename ... Options >
    using cc2650 = link_layer::link_layer< Server, cc2650_details::scheduled_radio, Options... >;
}

#endif