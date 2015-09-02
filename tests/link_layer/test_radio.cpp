#include "test_radio.hpp"
#include "../hexdump.hpp"

#include <boost/test/unit_test.hpp>

namespace test {

    std::ostream& operator<<( std::ostream& out, const advertising_data& data )
    {
        out << "at: " << data.schedule_time << "; scheduled: " << data.transmision_time << "; channel: " << data.channel;
        out << "\ndata:\n" << hex_dump( data.transmitted_data.begin(), data.transmitted_data.end() );

        return out;
    }

    std::ostream& operator<<( std::ostream& out, const connection_event_response& rsp )
    {
        if ( rsp.timeout )
        {
            out << "response-timeout";
        }
        else
        {
            out << "\ndata:\n";

            for ( const auto& pdu: rsp.data )
            {
                hex_dump( out, pdu.begin(), pdu.end() );
            }
        }

        return out;
    }

    std::ostream& operator<<( std::ostream& out, const connection_event& data )
    {
        out << "schedule_time: " << data.schedule_time << "; channel: " << data.channel
            << "\nstart_receive: " << data.start_receive << "; end_receive: " << data.end_receive << "; connection_interval: " << data.connection_interval
            << "\nreceived_data:\n";

        for ( const auto& pdu: data.received_data )
        {
            hex_dump( out, pdu.begin(), pdu.end() );
        }

        out << "transmitted_data:\n";
        for ( const auto& pdu: data.transmitted_data )
        {
            hex_dump( out, pdu.begin(), pdu.end() );
        }

        return out;
    }

    advertising_response::advertising_response()
        : channel( 0 )
        , delay( bluetoe::link_layer::delta_time( 0 ) )
        , has_crc_error( false )
    {
    }

    advertising_response::advertising_response( unsigned c, std::vector< std::uint8_t > d, const bluetoe::link_layer::delta_time l )
        : channel( c )
        , received_data( d )
        , delay( l )
        , has_crc_error( false )
    {
    }

    advertising_response advertising_response::crc_error()
    {
        advertising_response result;
        result.has_crc_error = true;

        return result;
    }

    std::ostream& operator<<( std::ostream& out, const advertising_response& data )
    {
        out << "channel: " << data.channel << "; delayed: " << data.delay;
        out << "\ndata:\n" << hex_dump( data.received_data.begin(), data.received_data.end() );

        return out;
    }

    const std::vector< advertising_data >& radio_base::advertisings() const
    {
        return advertised_data_;
    }

    const std::vector< connection_event >& radio_base::connection_events() const
    {
        return connection_events_;
    }

    radio_base::radio_base()
        : access_address_and_crc_valid_( false )
        , eos_( bluetoe::link_layer::delta_time::seconds( 10 ) )
    {
    }

    void radio_base::check_scheduling( const std::function< bool ( const advertising_data& ) >& check, const char* message ) const
    {
        unsigned n = 0;

        for ( const advertising_data& data : advertised_data_ )
        {
            if ( !check( data ) )
            {
                boost::test_tools::predicate_result result( false );
                result.message() << "\nfor " << ( n + 1 ) << "th scheduled action " << data;
                BOOST_CHECK( result );
                return;
            }

            ++n;
        }
    }

    void radio_base::check_scheduling( const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const
    {
        check_scheduling(
            [&]( const advertising_data& ) { return true; },
            check,
            message
        );
    }

    void radio_base::check_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const
    {
        pair_wise_check(
            filter,
            check,
            [&]( advertising_list::const_iterator first, advertising_list::const_iterator next )
            {
                const auto n  = std::distance( advertised_data_.begin(), first ) + 1;
                const auto nn = std::distance( first, next ) + n;

                boost::test_tools::predicate_result result( false );
                result.message() << "\nfor " << n << "th and " << nn << "th scheduled action";
                result.message() << "\nTesting: \"" << message << "\" failed.";
                result.message() << "\n" << n << "th scheduled action, " << *first;
                result.message() << "\n" << nn << "th scheduled action, " << *next;
                BOOST_CHECK( result );
            }
        );
    }

    void radio_base::check_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& data ) >& check, const char* message ) const
    {
        check_scheduling(
            [&]( const advertising_data& data ) -> bool
            {
                return !filter( data ) || check( data );
            },
            message
        );
    }

    void radio_base::check_first_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const std::function< bool ( const advertising_data& data ) >& check, const char* message ) const
    {
        bool ignore = false;
        check_scheduling(
            [&ignore, filter]( const advertising_data& d )
            {
                const bool result = !ignore && filter( d );
                ignore = true;

                return result;
            },
            check,
            message
        );
    }

    void radio_base::find_scheduling( const std::function< bool ( const advertising_data& ) >& filter, const char* message ) const
    {
        unsigned found = 0;

        for ( const advertising_data& data : advertised_data_ )
        {
            if ( filter( data ) )
                ++found;
        }

        if ( found != 1u )
        {
            boost::test_tools::predicate_result result( false );
            if ( found )
            {
                result.message() << message << ": required to find only in scheduling, but found: " << found;
            }
            else
            {
                result.message() << message << ": no required scheduling found!";
            }
            BOOST_CHECK( result );
        }
    }

    void radio_base::find_scheduling( const std::function< bool ( const advertising_data& first, const advertising_data& next ) >& check, const char* message ) const
    {
        int count = 0;

        all_data(
            []( const advertising_data& ){ return true; },
            [ &count, &check ]( const advertising_data& first, const advertising_data& next )
            {
                if ( check( first, next ) )
                    ++count;
            }
        );

        if ( count != 1 )
        {
            boost::test_tools::predicate_result result( false );
            if ( count == 0 )
            {
                result.message() << message << ": no required scheduling found!";
            }
            else
            {
                result.message() << message << ": required to find only in scheduling, but found: " << count;
            }
            BOOST_CHECK( result );
        }
    }

    std::vector< advertising_data >::const_iterator radio_base::next( std::vector< advertising_data >::const_iterator first, const std::function< bool ( const advertising_data& ) >& filter ) const
    {
        while ( first != advertised_data_.end() && !filter( *first ) )
            ++first;

        return first;
    }

    void radio_base::pair_wise_check(
        const std::function< bool ( const advertising_data& ) >&                                               filter,
        const std::function< bool ( const advertising_data& first, const advertising_data& next ) >&              check,
        const std::function< void ( advertising_list::const_iterator first, advertising_list::const_iterator next ) >&    fail ) const
    {
        for ( auto data = next( advertised_data_.begin(), filter ); data != advertised_data_.end(); )
        {
            const auto next_data = next( data +1, filter );

            if ( next_data != advertised_data_.end() )
            {
                if ( !check( *data, *next_data ) )
                {
                    fail( data, next_data );
                    return;
                }
            }

            data = next_data;
        }
    }

    void radio_base::all_data( std::function< void ( const advertising_data& ) > f ) const
    {
        for ( const advertising_data& data : advertised_data_ )
            f( data );
    }

    void radio_base::all_data(
        const std::function< bool ( const advertising_data& ) >& filter,
        const std::function< void ( const advertising_data& first, const advertising_data& next ) >& iter ) const
    {
        for ( auto data = next( advertised_data_.begin(), filter ); data != advertised_data_.end(); )
        {
            const auto next_data = next( data +1, filter );

            if ( next_data != advertised_data_.end() )
            {
                iter( *data, *next_data );
            }

            data = next_data;
        }
    }

    unsigned radio_base::count_data( const std::function< bool ( const advertising_data& ) >& filter ) const
    {
        return sum_data< unsigned >(
            [filter]( const advertising_data& data, unsigned count )
            {
                return filter( data )
                    ? count + 1
                    : count;
            }, 0u
        );
    }

    void radio_base::add_responder( const advertising_responder_t& responder )
    {
        responders_.push_back( responder );
    }

    void radio_base::respond_to( unsigned channel, std::initializer_list< std::uint8_t > pdu )
    {
        respond_to( channel, std::vector< std::uint8_t >( pdu ) );
    }

    void radio_base::respond_to( unsigned channel, std::vector< std::uint8_t > pdu )
    {
        assert( channel < 40 );

        add_responder(
            [=]( const advertising_data& d ) -> std::pair< bool, advertising_response >
            {
                return std::pair< bool, advertising_response >(
                    d.channel == channel,
                    advertising_response{ channel, pdu, T_IFS }
                );
            }
        );
    }

    void radio_base::respond_to( unsigned channel, std::initializer_list< std::uint8_t > pdu, unsigned times )
    {
        for ( ;times; --times )
            respond_to( channel, pdu );
    }

    void radio_base::respond_with_crc_error( unsigned channel )
    {
        assert( channel < 40 );

        add_responder(
            [=]( const advertising_data& ) -> std::pair< bool, advertising_response >
            {
                return std::pair< bool, advertising_response >(
                    true,
                    advertising_response::crc_error()
                );
            }
        );
    }

    std::pair< bool, advertising_response > radio_base::find_response( const advertising_data& data )
    {
        std::pair< bool, advertising_response > result( false, advertising_response{} );

        for ( auto f = responders_.begin(); f != responders_.end(); ++f )
        {
            result = (*f)( data );

            if ( result.first )
            {
                responders_.erase( f );

                return result;
            }
        }

        return result;
    }

    void radio_base::set_access_address_and_crc_init( std::uint32_t access_address, std::uint32_t crc_init )
    {
        access_address_ = access_address;
        crc_init_       = crc_init;

        access_address_and_crc_valid_ = true;
    }

    std::uint32_t radio_base::access_address() const
    {
        assert( access_address_and_crc_valid_ );

        return access_address_;
    }

    std::uint32_t radio_base::crc_init() const
    {
        assert( access_address_and_crc_valid_ );

        return crc_init_;
    }

    void radio_base::add_connection_event_respond( const connection_event_response& resp )
    {
        connection_events_response_.push_back( resp );
    }

    void radio_base::add_connection_event_respond( std::initializer_list< std::uint8_t > pdu )
    {
        add_connection_event_respond(
            connection_event_response{
                false, pdu_list_t( 1, pdu )
            } );
    }

    void radio_base::add_connection_event_respond_timeout()
    {
        add_connection_event_respond( connection_event_response{ true, pdu_list_t() } );
    }

    void radio_base::check_connection_events( const std::function< bool ( const connection_event& ) >& filter, const std::function< bool ( const connection_event& ) >& check, const char* message )
    {
        for ( const auto& event : connection_events_ )
        {
            if ( filter( event ) && !check( event ) )
            {
                boost::test_tools::predicate_result result( false );
                result.message() << message << ": " << event;
                BOOST_CHECK( result );
            }
        }
    }

    std::uint32_t radio_base::static_random_address_seed() const
    {
        return 0x47110815;
    }

    const bluetoe::link_layer::delta_time radio_base::T_IFS = bluetoe::link_layer::delta_time( 150u );

    void radio_base::end_of_simulation( bluetoe::link_layer::delta_time eos )
    {
        eos_ = eos;
    }


    radio_base::lock_guard::lock_guard()
    {
        assert( !locked_ );
        locked_ = true;
    }

    radio_base::lock_guard::~lock_guard()
    {
        locked_ = false;
    }

    bool radio_base::lock_guard::locked_ = false;
}
