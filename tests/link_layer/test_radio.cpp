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

    incomming_data::incomming_data()
        : channel( 0 )
        , delay( bluetoe::link_layer::delta_time( 0 ) )
        , has_crc_error( false )
    {
    }

    incomming_data::incomming_data( unsigned c, std::vector< std::uint8_t > d, const bluetoe::link_layer::delta_time l )
        : channel( c )
        , received_data( d )
        , delay( l )
        , has_crc_error( false )
    {
    }

    incomming_data incomming_data::crc_error()
    {
        incomming_data result;
        result.has_crc_error = true;

        return result;
    }

    std::ostream& operator<<( std::ostream& out, const incomming_data& data )
    {
        out << "channel: " << data.channel << "; delayed: " << data.delay;
        out << "\ndata:\n" << hex_dump( data.received_data.begin(), data.received_data.end() );

        return out;
    }

    const std::vector< advertising_data >& radio_base::scheduling() const
    {
        return transmitted_data_;
    }

    const std::vector< connection_event >& radio_base::connection_events() const
    {
        return connection_events_;
    }

    radio_base::radio_base()
        : access_address_and_crc_valid_( false )
    {
    }

    void radio_base::check_scheduling( const std::function< bool ( const advertising_data& ) >& check, const char* message ) const
    {
        unsigned n = 0;

        for ( const advertising_data& data : transmitted_data_ )
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
                const auto n  = std::distance( transmitted_data_.begin(), first ) + 1;
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

        for ( const advertising_data& data : transmitted_data_ )
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
        while ( first != transmitted_data_.end() && !filter( *first ) )
            ++first;

        return first;
    }

    void radio_base::pair_wise_check(
        const std::function< bool ( const advertising_data& ) >&                                               filter,
        const std::function< bool ( const advertising_data& first, const advertising_data& next ) >&              check,
        const std::function< void ( advertising_list::const_iterator first, advertising_list::const_iterator next ) >&    fail ) const
    {
        for ( auto data = next( transmitted_data_.begin(), filter ); data != transmitted_data_.end(); )
        {
            const auto next_data = next( data +1, filter );

            if ( next_data != transmitted_data_.end() )
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
        for ( const advertising_data& data : transmitted_data_ )
            f( data );
    }

    void radio_base::all_data(
        const std::function< bool ( const advertising_data& ) >& filter,
        const std::function< void ( const advertising_data& first, const advertising_data& next ) >& iter ) const
    {
        for ( auto data = next( transmitted_data_.begin(), filter ); data != transmitted_data_.end(); )
        {
            const auto next_data = next( data +1, filter );

            if ( next_data != transmitted_data_.end() )
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

    void radio_base::add_responder( const responder_t& responder )
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
            [=]( const advertising_data& d ) -> std::pair< bool, incomming_data >
            {
                return std::pair< bool, incomming_data >(
                    d.channel == channel,
                    incomming_data{ channel, pdu, T_IFS }
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
            [=]( const advertising_data& ) -> std::pair< bool, incomming_data >
            {
                return std::pair< bool, incomming_data >(
                    true,
                    incomming_data::crc_error()
                );
            }
        );
    }

    std::pair< bool, incomming_data > radio_base::find_response( const advertising_data& data )
    {
        std::pair< bool, incomming_data > result( false, incomming_data{} );

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

    std::uint32_t radio_base::static_random_address_seed() const
    {
        return 0x47110815;
    }

    const bluetoe::link_layer::delta_time radio_base::T_IFS = bluetoe::link_layer::delta_time( 150u );

}
