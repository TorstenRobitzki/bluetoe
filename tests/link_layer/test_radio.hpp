#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <vector>
#include <functional>

namespace test {

    struct scheduled_data
    {
        unsigned                        channel;
        bluetoe::link_layer::delta_time schedule_time;
        bluetoe::link_layer::delta_time transmision_time;
        std::vector< std::uint8_t >     transmited_data;
        bluetoe::link_layer::delta_time timeout;
    };

    class radio_base
    {
    public:
        // test interface
        const std::vector< scheduled_data >& transmitted_data() const;

        void check_all_data( std::function< bool ( const scheduled_data& ) > check, const char* message );

        void all_data( std::function< void ( const scheduled_data& ) > );
    protected:
        std::vector< scheduled_data > transmitted_data_;
    };

    template < typename CallBack >
    class radio : public radio_base
    {
    public:
        /**
         * @brief by default the radio simulates 10s without any response
         */
        radio();

        // scheduled_radio interface
        void schedule_transmit_and_receive(
                unsigned channel,
                const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
                const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout );

        void run();
    private:
        // end of simulations
        const bluetoe::link_layer::delta_time eos_;
              bluetoe::link_layer::delta_time now_;
    };

    // implementation
    template < typename CallBack >
    radio< CallBack >::radio()
        : eos_( bluetoe::link_layer::delta_time::seconds( 10 ) )
        , now_( bluetoe::link_layer::delta_time::now() )
    {
    }

    template < typename CallBack >
    void radio< CallBack >::schedule_transmit_and_receive(
            unsigned channel,
            const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
            const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout )
    {
        const scheduled_data data{
            channel,
            now_,
            when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ),
            timeout
        };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    void radio< CallBack >::run()
    {
        assert( !transmitted_data_.empty() );

        do
        {
            now_ += transmitted_data_.back().timeout;
            static_cast< CallBack* >( this )->timeout();
        } while ( now_ < eos_ );
    }
}

#endif // include guard
