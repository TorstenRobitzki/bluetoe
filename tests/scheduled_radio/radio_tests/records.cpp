#include "radio_tests/records.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <iterator>

namespace bluetoe {
namespace test_rig {

    std::vector< record > callbacks_of( const std::vector< record >& records, callback_kind kind )
    {
        std::vector< record > result;
        std::copy_if( records.begin(), records.end(), std::back_inserter( result ),
            [ kind ]( const record& r ){ return r.kind == record_kind::callback && r.callback == kind; } );

        return result;
    }

    std::vector< record > calls_of( const std::vector< record >& records, call_kind kind )
    {
        std::vector< record > result;
        std::copy_if( records.begin(), records.end(), std::back_inserter( result ),
            [ kind ]( const record& r ){ return r.kind == record_kind::call && r.call == kind; } );

        return result;
    }

    record the_only( const std::vector< record >& records )
    {
        BOOST_REQUIRE_MESSAGE( records.size() == 1, "expected one record, found " << records.size() );

        return records.front();
    }

    std::vector< callback_kind > callbacks( const std::vector< record >& records )
    {
        std::vector< callback_kind > result;

        for ( const record& r : records )
            if ( r.kind == record_kind::callback && r.callback != callback_kind::radio_ready )
                result.push_back( r.callback );

        return result;
    }

    std::string as_text( callback_kind kind )
    {
        switch ( kind )
        {
            case callback_kind::start:                  return "start";
            case callback_kind::radio_ready:            return "radio_ready";
            case callback_kind::adv_received:           return "adv_received";
            case callback_kind::adv_timeout:            return "adv_timeout";
            case callback_kind::user_timer:             return "user_timer";
            case callback_kind::connection_timeout:     return "connection_timeout";
            case callback_kind::connection_end_event:   return "connection_end_event";
        }

        return "unknown";
    }

    std::string as_text( const std::vector< callback_kind >& kinds )
    {
        std::string result;

        for ( const callback_kind kind : kinds )
            result += ( result.empty() ? "" : ", " ) + as_text( kind );

        return result;
    }

    void check_callbacks( const std::vector< record >& records, const std::vector< callback_kind >& expected )
    {
        BOOST_CHECK_EQUAL( as_text( callbacks( records ) ), as_text( expected ) );
    }
}
}
