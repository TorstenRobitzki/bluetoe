#include "test_radio.hpp"
#include "../hexdump.hpp"

#include <boost/test/unit_test.hpp>

namespace test {

    const std::vector< scheduled_data >& radio_base::transmitted_data() const
    {
        return transmitted_data_;
    }

    void radio_base::check_all_data( std::function< bool ( const scheduled_data& ) > check, const char* message )
    {
        unsigned n = 0;

        for ( const scheduled_data& data : transmitted_data_ )
        {
            if ( !check( data ) )
            {
                boost::test_tools::predicate_result check_data( false );
                check_data.message() << "\nfor " << ( n + 1 ) << "th scheduled action at: " << data.transmision_time << " timeout: " << data.timeout;
                check_data.message() << "\nTesting: \"" << message << "\" failed.";
                check_data.message() << "\nData:\n" << hex_dump( data.transmited_data.begin(), data.transmited_data.end() );
                BOOST_CHECK( check_data );
            }

            ++n;
        }
    }

    void radio_base::all_data( std::function< void ( const scheduled_data& ) > f )
    {
        for ( const scheduled_data& data : transmitted_data_ )
            f( data );
    }
}
