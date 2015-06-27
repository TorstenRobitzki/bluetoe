#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <vector>

namespace test {

    struct scheduled_data
    {
        unsigned                        channel;
        bluetoe::link_layer::delta_time transmision_time;
        std::vector< std::uint8_t >     transmited_data;
    };

    template < typename CallBack >
    class radio
    {
    public:
        // scheduled_radio interface
        void schedule_transmit_and_receive(
                unsigned channel,
                const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
                const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout );

        // test interface
        const std::vector< scheduled_data >& transmitted_data() const;

    private:
        std::vector< scheduled_data > transmitted_data_;
    };

    // implementation
    template < typename CallBack >
    void radio< CallBack >::schedule_transmit_and_receive(
            unsigned channel,
            const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
            const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout )
    {
        const scheduled_data data{
            channel, when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ) };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    const std::vector< scheduled_data >& radio< CallBack >::transmitted_data() const
    {
        return transmitted_data_;
    }

}

#endif // include guard
