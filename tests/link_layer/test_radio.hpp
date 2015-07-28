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
        bluetoe::link_layer::delta_time     transmision_time;  // or start of receiving
        std::vector< std::uint8_t >         transmitted_data;
        bluetoe::link_layer::read_buffer    receive_buffer;

        bool                                receive_and_transmit;
        bluetoe::link_layer::delta_time     end_receive;

        std::uint32_t                       access_address;
        std::uint32_t                       crc_init;
    };

    bool sn( const schedule_data& );
    bool nesn( const schedule_data& );

    std::ostream& operator<<( std::ostream& out, const schedule_data& data );

    struct incomming_data
    {
        incomming_data();

        incomming_data( unsigned c, std::vector< std::uint8_t > d, const bluetoe::link_layer::delta_time l );

        static incomming_data crc_error();

        unsigned                        channel;
        std::vector< std::uint8_t >     received_data;
        bluetoe::link_layer::delta_time delay;
        bool                            has_crc_error;
    };

    std::ostream& operator<<( std::ostream& out, const incomming_data& data );

    class radio_base
    {
    public:
        radio_base();

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
        void check_scheduling( const std::function< bool ( const schedule_data& ) >& filter, const std::function< bool ( const schedule_data& data ) >& check, const char* message ) const;

        void check_first_scheduling( const std::function< bool ( const schedule_data& ) >& filter, const std::function< bool ( const schedule_data& data ) >& check, const char* message ) const;

        /**
         * @brief there must be exactly one scheduled_data that fitts to the given filter
         */
        void find_scheduling( const std::function< bool ( const schedule_data& ) >& filter, const char* message ) const;
        void find_scheduling( const std::function< bool ( const schedule_data& first, const schedule_data& next ) >& check, const char* message ) const;

        void all_data( std::function< void ( const schedule_data& ) > ) const;
        void all_data( const std::function< bool ( const schedule_data& ) >& filter, const std::function< void ( const schedule_data& first, const schedule_data& next ) >& ) const;

        template < class Accu >
        Accu sum_data( std::function< Accu ( const schedule_data&, Accu start_value ) >, Accu start_value ) const;

        /**
         * @brief counts the number of times the given filter returns true for all schedule_data
         */
        unsigned count_data( const std::function< bool ( const schedule_data& ) >& filter ) const;

        /**
         * @brief function to take the arguments to a scheduling function and optional return a response
         */
        typedef std::function< std::pair< bool, incomming_data > ( const schedule_data& ) > responder_t;

        /**
         * @brief simulates an incomming PDU
         *
         * Given that a transmition was scheduled and the function responder() returns a pair with the first bool set to true, when applied to the transmitting
         * data, the given incomming_data is used to simulate an incoming PDU. The first function that returns true, will be applied and removed from the list.
         */
        void add_responder( const responder_t& responder );

        /**
         * @brief response to sending on the given channel with the given PDU send on the same channel without delay
         */
        void respond_to( unsigned channel, std::initializer_list< std::uint8_t > pdu );
        void respond_to( unsigned channel, std::vector< std::uint8_t > pdu );
        void respond_with_crc_error( unsigned channel );

        /**
         * @brief response `times` times
         */
        void respond_to( unsigned channel, std::initializer_list< std::uint8_t > pdu, unsigned times );

        void set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init );

        std::uint32_t access_address() const;
        std::uint32_t crc_init() const;

        /**
         * @brief returns 0x47110815
         */
        std::uint32_t static_random_address_seed() const;

        static const bluetoe::link_layer::delta_time T_IFS;
    protected:
        typedef std::vector< schedule_data > data_list;
        data_list transmitted_data_;

        typedef std::vector< responder_t > responder_list;
        responder_list responders_;

        std::uint32_t   access_address_;
        std::uint32_t   crc_init_;
        bool            access_address_and_crc_valid_;

        data_list::const_iterator next( std::vector< schedule_data >::const_iterator, const std::function< bool ( const schedule_data& ) >& filter ) const;

        void pair_wise_check(
            const std::function< bool ( const schedule_data& ) >&                                               filter,
            const std::function< bool ( const schedule_data& first, const schedule_data& next ) >&              check,
            const std::function< void ( data_list::const_iterator first, data_list::const_iterator next ) >&    fail ) const;

        std::pair< bool, incomming_data > find_response( const schedule_data& );
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
            unsigned                                    channel,
            const bluetoe::link_layer::write_buffer&    transmit,
            bluetoe::link_layer::delta_time             when,
            const bluetoe::link_layer::read_buffer&     receive );

        void schedule_receive_and_transmit(
            unsigned                                    channel,
            bluetoe::link_layer::delta_time             when,
            bluetoe::link_layer::delta_time             window_size,
            const bluetoe::link_layer::read_buffer&     receive,
            const bluetoe::link_layer::write_buffer&    answert );

        /**
         * @brief runs the simulation
         */
        void run();
    private:
        // end of simulations
        const bluetoe::link_layer::delta_time eos_;
              bluetoe::link_layer::delta_time now_;

        // make sure, there is only one action scheduled
        bool idle_;
    };

    // implementation
    template < class Accu >
    Accu radio_base::sum_data( std::function< Accu ( const schedule_data&, Accu start_value ) > f, Accu start_value ) const
    {
        for ( const auto& d : transmitted_data_ )
            start_value = f( d, start_value );

        return start_value;
    }

    template < typename CallBack >
    radio< CallBack >::radio()
        : eos_( bluetoe::link_layer::delta_time::seconds( 10 ) )
        , now_( bluetoe::link_layer::delta_time::now() )
        , idle_( true )
    {
    }

    template < typename CallBack >
    void radio< CallBack >::schedule_transmit_and_receive(
            unsigned channel,
            const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
            const bluetoe::link_layer::read_buffer& receive )
    {
        assert( idle_ );
        assert( access_address_and_crc_valid_ );

        idle_ = false;

        const schedule_data data{
            now_,
            now_ + when,
            channel,
            when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ),
            receive,
            false,
            bluetoe::link_layer::delta_time::now(),
            access_address_,
            crc_init_
        };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    void radio< CallBack >::schedule_receive_and_transmit(
        unsigned                                    channel,
        bluetoe::link_layer::delta_time             start_receive,
        bluetoe::link_layer::delta_time             end_receive,
        const bluetoe::link_layer::read_buffer&     receive,
        const bluetoe::link_layer::write_buffer&    answert )
    {
        assert( idle_ );
        assert( access_address_and_crc_valid_ );
        idle_ = false;

        const schedule_data data{
            now_,
            now_ + start_receive,
            channel,
            start_receive,
            std::vector< std::uint8_t >( answert.buffer, answert.buffer + answert.size ),
            receive,
            true,
            end_receive,
            access_address_,
            crc_init_
        };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    void radio< CallBack >::run()
    {
        assert( !transmitted_data_.empty() );

        auto count = transmitted_data_.size();

        do
        {
            count = transmitted_data_.size();

            schedule_data&                      current  = transmitted_data_.back();
            std::pair< bool, incomming_data >   response = find_response( current );

            // for now, only timeouts are simulated
            if ( response.first )
            {
                now_ += T_IFS;

                if ( response.second.has_crc_error )
                {
                    idle_ = true;
                    static_cast< CallBack* >( this )->crc_error();
                }
                else
                {
                    const auto& received_data = response.second.received_data;
                    const std::size_t copy_size = std::min< std::size_t >( current.receive_buffer.size, received_data.size() );

                    std::copy( received_data.begin(), received_data.begin() + copy_size, current.receive_buffer.buffer );
                    current.receive_buffer.size = copy_size;

                    idle_ = true;
                    static_cast< CallBack* >( this )->received( current.receive_buffer );
                }
            }
            else
            {
                now_ += transmitted_data_.back().transmision_time;
                idle_ = true;
                static_cast< CallBack* >( this )->timeout();
            }

//            assert( count + 1 == transmitted_data_.size() );
        } while ( now_ < eos_ && count + 1 == transmitted_data_.size() );
    }

}

#endif // include guard
