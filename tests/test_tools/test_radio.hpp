#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/buffer.hpp>
#include <bluetoe/delta_time.hpp>
#include <bluetoe/ll_data_pdu_buffer.hpp>
#include <bluetoe/link_layer.hpp>
#include <bluetoe/connection_events.hpp>

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

    struct pdu_t {
        std::vector< std::uint8_t > data;
        bool                        encrypted;

        using iterator       = std::vector< std::uint8_t >::iterator;
        using const_iterator = std::vector< std::uint8_t >::const_iterator;

        iterator begin()
        {
            return data.begin();
        }

        iterator end()
        {
            return data.end();
        }

        const_iterator begin() const
        {
            return data.begin();
        }

        const_iterator end() const
        {
            return data.end();
        }

        std::size_t size() const
        {
            return data.size();
        }

        std::uint8_t& operator[]( int index )
        {
            return data[ index ];
        }

        std::uint8_t operator[]( int index ) const
        {
            return data[ index ];
        }

        pdu_t( const std::vector< std::uint8_t > d )
            : data( d )
            , encrypted( false )
        {}

        pdu_t( const std::vector< std::uint8_t > d, bool enc )
            : data( d )
            , encrypted( enc )
        {}

        pdu_t( std::initializer_list< std::uint8_t > list )
            : data( list )
            , encrypted( false )
        {}
    };

    using pdu_list_t = std::vector< pdu_t >;

    std::ostream& operator<<( std::ostream& out, const pdu_t& data );
    std::ostream& operator<<( std::ostream& out, const pdu_list_t& data );
    std::ostream& operator<<( std::ostream& out, bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t phy );

    struct connection_event
    {
        bluetoe::link_layer::delta_time     schedule_time;     // when was the actions scheduled (from start of simulation)

        // parameters
        unsigned                            channel;
        bluetoe::link_layer::delta_time     start_receive;
        bluetoe::link_layer::delta_time     end_receive;

        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving_encoding;
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmission_encoding;

        std::uint32_t                       access_address;
        std::uint32_t                       crc_init;

        pdu_list_t                          transmitted_data;
        pdu_list_t                          received_data;

        bool                                receive_encryption_at_start_of_event;
        bool                                transmit_encryption_at_start_of_event;
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

    struct scheduled_user_timer
    {
        bluetoe::link_layer::delta_time     schedule_time;      // when was the actions scheduled (from start of simulation)
        bluetoe::link_layer::delta_time     current_anchor;     // Anchor on which delay is based
        bluetoe::link_layer::delta_time     delay;
    };

    std::ostream& operator<<( std::ostream& out, const scheduled_user_timer& data );
    std::ostream& operator<<( std::ostream& out, const std::vector< scheduled_user_timer >& data );

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
        const std::vector< scheduled_user_timer >& scheduled_user_timers() const;

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
        void check_connection_events( const std::function< bool ( const connection_event& ) >& check, const char* message );

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

        void increment_receive_packet_counter() {}
        void increment_transmit_packet_counter() {}

        void radio_set_phy(
            bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t receiving_encoding,
            bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t transmiting_c_encoding );

    protected:
        typedef std::vector< advertising_data > advertising_list;
        advertising_list advertised_data_;

        typedef std::vector< connection_event > connection_event_list;
        connection_event_list connection_events_;

        typedef std::vector< advertising_responder_t > responder_list;
        responder_list responders_;

        typedef std::vector< connection_event_response > connection_event_response_list;
        connection_event_response_list connection_events_response_;

        typedef std::vector< scheduled_user_timer > scheduled_user_timers_list;
        scheduled_user_timers_list scheduled_user_timers_;

        std::uint32_t   access_address_;
        std::uint32_t   crc_init_;
        bool            access_address_and_crc_valid_;
        std::uint8_t    central_sequence_number_    = 0;
        std::uint8_t    central_ne_sequence_number_ = 0;

        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t    receiving_encoding_;
        bluetoe::link_layer::phy_ll_encoding::phy_ll_encoding_t    transmiting_encoding_;

        static constexpr std::size_t ll_header_size = 2;

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
     * @brief the test radio uses a layout that requires more memory
     *
     * Use this buffer size during tests as default.
     */
    using buffer_sizes = bluetoe::link_layer::buffer_sizes< 61u, 61u >;

    template < class Accu >
    Accu radio_base::sum_data( std::function< Accu ( const advertising_data&, Accu start_value ) > f, Accu start_value ) const
    {
        for ( const auto& d : advertised_data_ )
            start_value = f( d, start_value );

        return start_value;
    }


    struct pdu_layout : bluetoe::link_layer::details::layout_base< pdu_layout > {
        static constexpr std::size_t header_size = sizeof( std::uint16_t );

        using bluetoe::link_layer::details::layout_base< pdu_layout >::header;

        static std::uint16_t header( const std::uint8_t* pdu )
        {
            return ::bluetoe::details::read_16bit( pdu ) ^ 0xffff;
        }

        static void header( std::uint8_t* pdu, std::uint16_t header_value )
        {
            ::bluetoe::details::write_16bit( pdu, header_value ^ 0xffff );
        }

        static std::pair< std::uint8_t*, std::uint8_t* > body( const bluetoe::link_layer::read_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size + 2 ], &pdu.buffer[ pdu.size ] };
        }

        static std::pair< const std::uint8_t*, const std::uint8_t* > body( const bluetoe::link_layer::write_buffer& pdu )
        {
            assert( pdu.size >= header_size );

            return { &pdu.buffer[ header_size + 2 ], &pdu.buffer[ pdu.size ] };
        }

        static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
        {
            return header_size + payload_size + 2;
        }
    };

}

/*
 * To make sure, that all parts of the library take the PDU layout into account, all tests are done
 * with a special layout, where the header is inverted and where a gap (of 2 octets) between header and octets is inserted.
 */
#endif // include guard
