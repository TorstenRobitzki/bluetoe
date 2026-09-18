#include "test_tools/records.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace bluetoe {
namespace test_rig {

    namespace {

        std::string flags_of( const connection_end_event& events )
        {
            const std::pair< bool, const char* > flags[] = {
                { events.unacknowledged_data,         "unacknowledged_data" },
                { events.last_received_not_empty,     "last_received_not_empty" },
                { events.last_transmitted_not_empty,  "last_transmitted_not_empty" },
                { events.last_received_had_more_data, "last_received_had_more_data" },
                { events.pending_outgoing_data,       "pending_outgoing_data" },
                { events.error_occured,               "error_occured" } };

            std::string result;

            for ( const auto& [ set, name ] : flags )
                if ( set )
                    result += ( result.empty() ? " " : ", " ) + std::string( name );

            return "{" + result + ( result.empty() ? "}" : " }" );
        }

        std::string flags_of( const link_layer::connection_event_events& events )
        {
            return flags_of( connection_end_event{
                .unacknowledged_data         = events.unacknowledged_data,
                .last_received_not_empty     = events.last_received_not_empty,
                .last_transmitted_not_empty  = events.last_transmitted_not_empty,
                .last_received_had_more_data = events.last_received_had_more_data,
                .pending_outgoing_data       = events.pending_outgoing_data,
                .error_occured               = events.error_occured } );
        }

        std::string joined( const std::vector< std::string >& entries )
        {
            std::string result;

            for ( const std::string& entry : entries )
                result += ( result.empty() ? "" : ", " ) + entry;

            return result;
        }
    }

    expected_callback::expected_callback( callback_kind expected_kind )
        : kind( expected_kind )
    {
    }

    expected_callback::expected_callback( const connection_end_event& expected_events )
        : kind( callback_kind::connection_end_event )
        , events( expected_events )
    {
    }

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

    std::string as_text( const std::vector< record >& records, const std::vector< expected_callback >& expected )
    {
        std::vector< std::string > found;

        for ( const record& r : records )
        {
            if ( r.kind != record_kind::callback || r.callback == callback_kind::radio_ready )
                continue;

            const std::size_t index = found.size();

            found.push_back( index < expected.size() && expected[ index ].events
                ? as_text( r.callback ) + flags_of( r.events )
                : as_text( r.callback ) );
        }

        return joined( found );
    }

    std::string as_text( const std::vector< expected_callback >& expected )
    {
        std::vector< std::string > required;

        for ( const expected_callback& entry : expected )
            required.push_back( entry.events ? as_text( entry.kind ) + flags_of( *entry.events ) : as_text( entry.kind ) );

        return joined( required );
    }

    void check_callbacks( const std::vector< record >& records, const std::vector< expected_callback >& expected )
    {
        BOOST_CHECK_EQUAL( as_text( records, expected ), as_text( expected ) );
    }
}
}
