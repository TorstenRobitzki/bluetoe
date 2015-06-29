#include "test_radio.hpp"
#include "../hexdump.hpp"

#include <boost/test/unit_test.hpp>

namespace test {

    const std::vector< schedule_data >& radio_base::transmitted_data() const
    {
        return transmitted_data_;
    }

    void radio_base::check_scheduling( std::function< bool ( const schedule_data& ) > check, const char* message ) const
    {
        unsigned n = 0;

        for ( const schedule_data& data : transmitted_data_ )
        {
            if ( !check( data ) )
            {
                boost::test_tools::predicate_result result( false );
                result.message() << "\nfor " << ( n + 1 ) << "th scheduled action at: " << data.transmision_time << " timeout: " << data.timeout;
                result.message() << "\nTesting: \"" << message << "\" failed.";
                result.message() << "\nData:\n" << hex_dump( data.transmited_data.begin(), data.transmited_data.end() );
                BOOST_CHECK( result );
                return;
            }

            ++n;
        }
    }

    void radio_base::check_scheduling( std::function< bool ( const schedule_data& first, const schedule_data& next ) > check, const char* message ) const
    {
        if ( transmitted_data_.size() < 2 )
            return;

        unsigned n = 0;

        for ( auto data = transmitted_data_.begin() + 1; data != transmitted_data_.end(); ++data )
        {
            const schedule_data& first  = *( data - 1 );
            const schedule_data& second = *data;

            if ( !check( first, second ) )
            {
                boost::test_tools::predicate_result result( false );
                result.message() << "\nfor " << ( n + 1 ) << "th and " << ( n + 2 ) << "th scheduled action";
                result.message() << "\nTesting: \"" << message << "\" failed.";
                result.message() << "\n" << ( n + 1 ) << "th scheduled action, at: " << first.transmision_time << " timeout: " << first.timeout;
                result.message() << "\nData:\n" << hex_dump( first.transmited_data.begin(), first.transmited_data.end() );
                result.message() << "\n" << ( n + 2 ) << "th scheduled action, at: " << second.transmision_time << " timeout: " << second.timeout;
                result.message() << "\nData:\n" << hex_dump( second.transmited_data.begin(), second.transmited_data.end() );
                BOOST_CHECK( result );
                return;
            }

            ++n;
        }
    }

    void radio_base::all_data( std::function< void ( const schedule_data& ) > f ) const
    {
        for ( const schedule_data& data : transmitted_data_ )
            f( data );
    }
}
