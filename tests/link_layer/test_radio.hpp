#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>

#include <vector>
#include <functional>
#include <iosfwd>
#include <initializer_list>

namespace test {

    /**
     * @brief stores all relevant arguments to a schedule function call to the radio
     */
    struct schedule_data
    {
        bluetoe::link_layer::delta_time     schedule_time;     // when was the actions scheduled (from start of simulation)
        bluetoe::link_layer::delta_time     on_air_time;       // when was the action on air (from start of simulation)

        // parameters
        unsigned                            channel;
        bluetoe::link_layer::delta_time     transmision_time;
        std::vector< std::uint8_t >         transmitted_data;
        bluetoe::link_layer::read_buffer    receive_buffer;
    };

    std::ostream& operator<<( std::ostream& out, const schedule_data& data );

    struct incomming_data
    {
        incomming_data();

        unsigned                        channel;
        std::vector< std::uint8_t >     received_data;
        bluetoe::link_layer::delta_time delay;
    };

    std::ostream& operator<<( std::ostream& out, const incomming_data& data );

    class radio_base
    {
    public:
        // test interface
        const std::vector< schedule_data >& scheduling() const;

        /**
         * @brief calls check with every scheduled_data
         */
        void check_scheduling( const std::function< bool ( const schedule_data& ) >& check, const char* message ) const;

        /**
         * @brief calls check with adjanced pairs of schedule_data.
         */
        void check_scheduling( const std::function< bool ( const schedule_data& first, const schedule_data& next ) >& check, const char* message ) const;
        void check_scheduling( const std::function< bool ( const schedule_data& ) >& filter, const std::function< bool ( const schedule_data& first, const schedule_data& next ) >& check, const char* message ) const;

        void all_data( std::function< void ( const schedule_data& ) > ) const;
        void all_data( const std::function< bool ( const schedule_data& ) >& filter, const std::function< void ( const schedule_data& first, const schedule_data& next ) >& ) const;

        /**
         * @brief function to take the arguments to a scheduling function and optional return a response
         */
        typedef std::function< std::pair< bool, incomming_data > ( const schedule_data& ) > responder_t;

        /**
         * @brief simulates an incomming PDU
         *
         * Given that a transmition was scheduled and the function responder() returns a pair with the first bool set to true, when applied to the transmitting
         * data, the given incomming_data is used to simulate an incoming PDU.
         */
        void add_responder( const responder_t& responder );

        /**
         * @brief response to sending on the given channel with the given PDU send on the same channel without delay
         */
        void respond_to( unsigned channel, std::initializer_list< std::uint8_t > pdu );

    protected:
        typedef std::vector< schedule_data > data_list;
        data_list transmitted_data_;

        typedef std::vector< responder_t > responder_list;
        responder_list responders_;

        data_list::const_iterator next( std::vector< schedule_data >::const_iterator, const std::function< bool ( const schedule_data& ) >& filter ) const;

        void pair_wise_check(
            const std::function< bool ( const schedule_data& ) >&                                               filter,
            const std::function< bool ( const schedule_data& first, const schedule_data& next ) >&              check,
            const std::function< void ( data_list::const_iterator first, data_list::const_iterator next ) >&    fail ) const;

        std::pair< bool, incomming_data > find_response( const schedule_data& ) const;
    };

    /**
     * @brief test implementation of the link_layer::scheduled_radio interface, that simulates receiving and transmitted data
     */
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
                const bluetoe::link_layer::read_buffer& receive );

        /**
         * @brief runs the simulation
         */
        void run();

        static const bluetoe::link_layer::delta_time T_IFS;
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
            const bluetoe::link_layer::read_buffer& receive )
    {
        const schedule_data data{
            now_,
            now_ + when,
            channel,
            when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ),
            receive
        };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    void radio< CallBack >::run()
    {
        assert( !transmitted_data_.empty() );

        do
        {
            const schedule_data&                current  = transmitted_data_.back();
            std::pair< bool, incomming_data >   response = find_response( current );

            // for now, only timeouts are simulated
            if ( response.first )
            {
                now_ += T_IFS;
                const auto& received_data = response.second.received_data;

                const std::size_t copy_size = std::min< std::size_t >( current.receive_buffer.size, received_data.size() );

                std::copy( received_data.begin(), received_data.begin() + copy_size, current.receive_buffer.buffer );

                static_cast< CallBack* >( this )->received( current.receive_buffer );
            }
            else
            {
                now_ += transmitted_data_.back().transmision_time;
                static_cast< CallBack* >( this )->timeout();
            }
        } while ( now_ < eos_ );
    }

    template < typename CallBack >
    const bluetoe::link_layer::delta_time radio< CallBack >::T_IFS = bluetoe::link_layer::delta_time( 150u );

}

#endif // include guard
