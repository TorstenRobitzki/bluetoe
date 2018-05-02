#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <bluetoe/link_layer/ll_data_pdu_buffer.hpp>
#include <bluetoe/sm/security_manager.hpp>

#include <vector>
#include <functional>
#include <iosfwd>
#include <initializer_list>
#include <iostream>

namespace test {

    /**
     * @brief expression that can be used in some of the finder functions to denote that this is always a match
     */
    static constexpr std::uint16_t X = 0x0100;

    /**
     * @brief expresssion that can be used as a last element of an expression to a finder function to denote that
     *        you do not care about the reset of the pdu.
     */
    static constexpr std::uint16_t and_so_on = 0x0200;

    /**
     * @brief stores all relevant arguments to a schedule_advertisment() function call to the radio
     */
    struct advertising_data
    {
        bluetoe::link_layer::delta_time     schedule_time;     // when was the actions scheduled (from start of simulation)
        bluetoe::link_layer::delta_time     on_air_time;       // when was the action on air (from start of simulation)

        // parameters
        unsigned                            channel;
        bluetoe::link_layer::delta_time     transmision_time;  // or start of receiving
        std::vector< std::uint8_t >         transmitted_data;
        bluetoe::link_layer::read_buffer    receive_buffer;

        std::uint32_t                       access_address;
        std::uint32_t                       crc_init;
    };

    std::ostream& operator<<( std::ostream& out, const advertising_data& data );
    std::ostream& operator<<( std::ostream& out, const std::vector< advertising_data >& data );

    using pdu_t = std::vector< std::uint8_t >;
    using pdu_list_t = std::vector< pdu_t >;

    std::ostream& operator<<( std::ostream& out, const pdu_t& data );
    std::ostream& operator<<( std::ostream& out, const pdu_list_t& data );

    struct connection_event
    {
        bluetoe::link_layer::delta_time     schedule_time;     // when was the actions scheduled (from start of simulation)

        // parameters
        unsigned                            channel;
        bluetoe::link_layer::delta_time     start_receive;
        bluetoe::link_layer::delta_time     end_receive;
        bluetoe::link_layer::delta_time     connection_interval;

        std::uint32_t                       access_address;
        std::uint32_t                       crc_init;

        pdu_list_t                          transmitted_data;
        pdu_list_t                          received_data;
    };

    std::ostream& operator<<( std::ostream& out, const connection_event& );
    std::ostream& operator<<( std::ostream& out, const std::vector< connection_event >& list );

    struct connection_event_response
    {
        bool                                timeout; // respond with an timeout
        pdu_list_t                          data;    // respond with data (including no data)
        std::function< pdu_list_t () >      func;    // inquire respond by calling func

        /**
         * @brief simulating no response, not even an empty PDU.
         */
        connection_event_response()
            : timeout( true )
        {}

        explicit connection_event_response( const pdu_list_t& d )
            : timeout( false )
            , data( d )
        {}

        explicit connection_event_response( const std::function< pdu_list_t () >& f )
            : timeout( false )
            , func( f )
        {}

        explicit connection_event_response( const std::function< void() >& f )
            : timeout( false )
            , func( [f](){ f(); return pdu_list_t(); } )
        {}
    };

    std::ostream& operator<<( std::ostream& out, const connection_event_response& );

    struct advertising_response
    {
        advertising_response();

        advertising_response( unsigned c, std::vector< std::uint8_t > d, const bluetoe::link_layer::delta_time l );

        static advertising_response crc_error();

        unsigned                        channel;
        std::vector< std::uint8_t >     received_data;
        bluetoe::link_layer::delta_time delay;
        bool                            has_crc_error;
    };

    std::ostream& operator<<( std::ostream& out, const advertising_response& data );

    /**
     * @brief returns true, if pdu matches pattern.
     * @sa X
     * @sa and_so_on
     */
    bool check_pdu( const pdu_t& pdu, std::initializer_list< std::uint16_t > pattern );

    /**
     * @brief prints a pattern, so that it's easy comparable to a PDU
     */
    std::string pretty_print_pattern( std::initializer_list< std::uint16_t > pattern );

    class radio_base
    {
    public:
        radio_base();

        // test interface
        const std::vector< advertising_data >& advertisings() const;
        const std::vector< connection_event >& connection_events() const;

        /**
         * @brief calls check with every scheduled_data
         */
        void check_scheduling( const std::function< bool ( const advertising_data& ) >& check, const char* message ) const;

        /**
         * @brief calls check with adjanced pairs of advertising_data.
         */
        void check_scheduling( const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const;
        void check_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const;
        void check_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& data ) >& check, const char* message ) const;

        void check_first_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& data ) >& check, const char* message ) const;

        /**
         * @brief there must be exactly one scheduled_data that fitts to the given filter
         */
        void find_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const char* message ) const;
        void find_scheduling( const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const;

        void all_data( std::function< void ( const advertising_data& ) > ) const;
        void all_data( const std::function< bool ( const advertising_data& ) >& filter, const std::function< void ( const advertising_data& first, const advertising_data& next ) >& ) const;

        template < class Accu >
        Accu sum_data( std::function< Accu ( const advertising_data&, Accu start_value ) >, Accu start_value ) const;

        /**
         * @brief counts the number of times the given filter returns true for all advertising_data
         */
        unsigned count_data( const std::function< bool ( const advertising_data& ) >& filter ) const;

        /**
         * @brief function to take the arguments to a scheduling function and optional return a response
         */
        typedef std::function< std::pair< bool, advertising_response > ( const advertising_data& ) > advertising_responder_t;

        /**
         * @brief simulates an incomming PDU
         *
         * Given that a transmition was scheduled and the function responder() returns a pair with the first bool set to true, when applied to the transmitting
         * data, the given advertising_response is used to simulate an incoming PDU. The first function that returns true, will be applied and removed from the list.
         */
        void add_responder( const advertising_responder_t& responder );

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

        void add_connection_event_respond( const connection_event_response& );
        void add_connection_event_respond( std::initializer_list< std::uint8_t > );
        void add_connection_event_respond( std::function< void() > );
        void add_connection_event_respond_timeout();

        void check_connection_events( const std::function< bool ( const connection_event& ) >& filter, const std::function< bool ( const connection_event& ) >& check, const char* message );

        /**
         * @brief check that exacly one outgoing l2cap layer pdu matches the given pattern
         */
        void check_outgoing_l2cap_pdu( std::initializer_list< std::uint16_t > pattern );

        /**
         * @brief check that exacly one outgoing link layer pdu matches the given pattern
         */
        void check_outgoing_ll_control_pdu( std::initializer_list< std::uint16_t > pattern );

        /**
         * @brief clear all events
         */
        void clear_events();

        /**
         * @brief returns 0x47110815
         */
        std::uint32_t static_random_address_seed() const;

        static const bluetoe::link_layer::delta_time T_IFS;

        void end_of_simulation( bluetoe::link_layer::delta_time );

        class lock_guard
        {
        public:
            lock_guard();
            ~lock_guard();

            lock_guard( const lock_guard& ) = delete;
            lock_guard& operator=( const lock_guard& ) = delete;
        private:
            static bool locked_;
        };

        static constexpr std::size_t radio_maximum_white_list_entries = 0;

    protected:
        typedef std::vector< advertising_data > advertising_list;
        advertising_list advertised_data_;

        typedef std::vector< connection_event > connection_event_list;
        connection_event_list connection_events_;

        typedef std::vector< advertising_responder_t > responder_list;
        responder_list responders_;

        typedef std::vector< connection_event_response > connection_event_response_list;
        connection_event_response_list connection_events_response_;

        std::uint32_t   access_address_;
        std::uint32_t   crc_init_;
        bool            access_address_and_crc_valid_;
        std::uint8_t    master_sequence_number_    = 0;
        std::uint8_t    master_ne_sequence_number_ = 0;

        // end of simulations
        bluetoe::link_layer::delta_time eos_;

        advertising_list::const_iterator next( std::vector< advertising_data >::const_iterator, const std::function< bool ( const advertising_data& ) >& filter ) const;

        void pair_wise_check(
            const std::function< bool ( const advertising_data& ) >&                                               filter,
            const std::function< bool ( const advertising_data& first, const advertising_data& next ) >&              check,
            const std::function< void ( advertising_list::const_iterator first, advertising_list::const_iterator next ) >&    fail ) const;

        std::pair< bool, advertising_response > find_response( const advertising_data& );
    };

    /**
     * @brief test implementation of the link_layer::scheduled_radio interface, that simulates receiving and transmitted data
     */
    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    class radio : public radio_base, public bluetoe::link_layer::ll_data_pdu_buffer< TransmitSize, ReceiveSize, radio< TransmitSize, ReceiveSize, CallBack > >
    {
    public:
        /**
         * @brief by default the radio simulates 10s without any response
         */
        radio();

        // scheduled_radio interface
        void schedule_advertisment(
            unsigned                                    channel,
            const bluetoe::link_layer::write_buffer&    advertising_data,
            const bluetoe::link_layer::write_buffer&    response_data,
            bluetoe::link_layer::delta_time             when,
            const bluetoe::link_layer::read_buffer&     receive );

        bluetoe::link_layer::delta_time schedule_connection_event(
            unsigned                                    channel,
            bluetoe::link_layer::delta_time             start_receive,
            bluetoe::link_layer::delta_time             end_receive,
            bluetoe::link_layer::delta_time             connection_interval );

        void wake_up();

        /**
         * @brief runs the simulation
         */
        void run();

        static constexpr bool hardware_supports_encryption = false;

    private:
        bluetoe::link_layer::delta_time now_;

        void simulate_advertising_response();
        void simulate_connection_event_response();

        // make sure, there is only one action scheduled
        bool idle_;
        bool advertising_response_;
        bool connection_event_response_;
        int  wake_ups_;
    };

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    class radio_with_encryption : public radio< TransmitSize, ReceiveSize, CallBack >
    {
    public:
        static constexpr bool hardware_supports_encryption = true;

        // Security functions
        bluetoe::details::uint128_t create_srand()
        {
            const bluetoe::details::uint128_t r{{
                0xE0, 0x2E, 0x70, 0xC6,
                0x4E, 0x27, 0x88, 0x63,
                0x0E, 0x6F, 0xAD, 0x56,
                0x21, 0xD5, 0x83, 0x57
            }};

            return r;
        }

        bluetoe::details::uint128_t c1(
            const bluetoe::details::uint128_t& temp_key,
            const bluetoe::details::uint128_t& /* srand */,
            const bluetoe::details::uint128_t& /* p1 */,
            const bluetoe::details::uint128_t& /* p2 */ ) const
        {
            return temp_key;
        }

        bluetoe::details::uint128_t s1(
            const bluetoe::details::uint128_t& stk,
            const bluetoe::details::uint128_t& /* srand */,
            const bluetoe::details::uint128_t& /* mrand */)
        {
            return stk;
        }
    };

    // implementation
    template < class Accu >
    Accu radio_base::sum_data( std::function< Accu ( const advertising_data&, Accu start_value ) > f, Accu start_value ) const
    {
        for ( const auto& d : advertised_data_ )
            start_value = f( d, start_value );

        return start_value;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    radio< TransmitSize, ReceiveSize, CallBack >::radio()
        : now_( bluetoe::link_layer::delta_time::now() )
        , idle_( true )
        , advertising_response_( false )
        , connection_event_response_( false )
        , wake_ups_( 0 )
    {
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    void radio< TransmitSize, ReceiveSize, CallBack >::schedule_advertisment(
            unsigned                                    channel,
            const bluetoe::link_layer::write_buffer&    transmit,
            const bluetoe::link_layer::write_buffer&,
            bluetoe::link_layer::delta_time             when,
            const bluetoe::link_layer::read_buffer&     receive )
    {
        assert( idle_ );
        assert( access_address_and_crc_valid_ );
        assert( transmit.buffer );

        idle_ = false;
        advertising_response_ = true;
        connection_event_response_ = false;

        const advertising_data data{
            now_,
            now_ + when,
            channel,
            when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ),
            receive,
            access_address_,
            crc_init_
        };

        advertised_data_.push_back( data );
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    bluetoe::link_layer::delta_time radio< TransmitSize, ReceiveSize, CallBack >::schedule_connection_event(
        unsigned                                    channel,
        bluetoe::link_layer::delta_time             start_receive,
        bluetoe::link_layer::delta_time             end_receive,
        bluetoe::link_layer::delta_time             connection_interval )
    {
        advertising_response_ = false;
        connection_event_response_ = true;

        const connection_event data{
            now_,
            channel,
            start_receive,
            end_receive,
            connection_interval,
            access_address_,
            crc_init_,
            pdu_list_t(),
            pdu_list_t()
        };

        connection_events_.push_back( data );

        return bluetoe::link_layer::delta_time();
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    void radio< TransmitSize, ReceiveSize, CallBack >::wake_up()
    {
        ++wake_ups_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    void radio< TransmitSize, ReceiveSize, CallBack >::run()
    {
        bool new_scheduling_added = false;
        master_sequence_number_    = 0;
        master_ne_sequence_number_ = 0;

        do
        {
            unsigned count = advertised_data_.size() + connection_events_.size();;

            if ( advertising_response_ )
            {
                advertising_response_ = false;
                simulate_advertising_response();
            }
            else if ( connection_event_response_ )
            {
                connection_event_response_ = false;
                simulate_connection_event_response();
            }

            // there should be at max one call to a schedule function
            assert( count + 1 >= advertised_data_.size() + connection_events_.size() );

            new_scheduling_added = advertised_data_.size() + connection_events_.size() > count;

        } while ( now_ < eos_ && new_scheduling_added && wake_ups_ == 0 );

        if ( wake_ups_ )
            --wake_ups_;
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    void radio< TransmitSize, ReceiveSize, CallBack >::simulate_advertising_response()
    {
        assert( !advertised_data_.empty() );

        advertising_data&                       current  = advertised_data_.back();
        std::pair< bool, advertising_response > response = find_response( current );

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
                static_cast< CallBack* >( this )->adv_received( current.receive_buffer );
            }
        }
        else
        {
            now_ += advertised_data_.back().transmision_time;
            idle_ = true;
            static_cast< CallBack* >( this )->adv_timeout();
        }
    }

    template < std::size_t TransmitSize, std::size_t ReceiveSize, typename CallBack >
    void radio< TransmitSize, ReceiveSize, CallBack >::simulate_connection_event_response()
    {
        connection_event_response response = connection_events_response_.empty()
            ? connection_event_response()
            : connection_events_response_.front();

        assert( !connection_events_.empty() );
        auto& event = connection_events_.back();

        if ( !connection_events_response_.empty() )
        {
            connection_events_response_.erase( connection_events_response_.begin() );
        }

        if ( response.timeout )
        {
            now_ += event.end_receive;
            static_cast< CallBack* >( this )->timeout();
        }
        else
        {
            now_ += event.start_receive;

            static constexpr std::uint8_t sn_flag        = 0x8;
            static constexpr std::uint8_t nesn_flag      = 0x4;
            static constexpr std::uint8_t more_data_flag = 0x10;

            bool more_data = false;

            pdu_list_t pdus = response.data;

            if ( pdus.empty() && response.func )
                pdus = response.func();

            do
            {
                auto receive_buffer = this->allocate_receive_buffer();

                more_data = false;

                if ( receive_buffer.size )
                {
                    // what is the link layer going to receive?
                    if ( pdus.empty() )
                    {
                        receive_buffer.buffer[ 0 ] = 1;
                        receive_buffer.buffer[ 1 ] = 0;
                    }
                    else
                    {
                        const auto pdu = pdus.front();
                        pdus.erase( pdus.begin() );
                        std::copy( pdu.begin(), pdu.end(), receive_buffer.buffer );
                        more_data = !pdus.empty();
                    }

                    receive_buffer.buffer[ 0 ] &= ~( sn_flag | nesn_flag );
                    receive_buffer.buffer[ 0 ] |= master_sequence_number_ | master_ne_sequence_number_;
                    master_sequence_number_    ^= sn_flag;
                }

                if ( more_data && receive_buffer.size )
                    receive_buffer.buffer[ 0 ] |= more_data_flag;

                auto response = this->received( receive_buffer );

                more_data = more_data || ( response.buffer[ 0 ] & more_data_flag );
                master_ne_sequence_number_ ^= nesn_flag;

                event.received_data.push_back(
                    std::vector< std::uint8_t >( &receive_buffer.buffer[ 0 ], &receive_buffer.buffer[ 2 ] + receive_buffer.buffer[ 1 ] ) );

                event.transmitted_data.push_back(
                    std::vector< std::uint8_t >( &response.buffer[ 0 ], &response.buffer[ 2 ] + response.buffer[ 1 ] ) );

            } while ( more_data );

            static_cast< CallBack* >( this )->end_event();
        }
    }
}

#endif // include guard
