#include "radio_tests/timeline.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace bluetoe {
namespace test_rig {

    namespace {

        constexpr char unspecified[] = "*";

        std::span< const std::uint8_t > bytes_of( const captured_pdu& entry )
        {
            return { entry.data.data.data(), entry.data.size };
        }

        std::string as_line(
            std::optional< pdu_direction > direction, std::optional< bool > crc_ok,
            std::optional< std::span< const std::uint8_t > > bytes )
        {
            std::ostringstream out;

            if ( direction )
                out << ( *direction == pdu_direction::received ? "received" : "sent" );
            else
                out << unspecified;

            out << ' ' << ( crc_ok ? ( *crc_ok ? "crc_ok" : "crc_error" ) : unspecified );

            if ( !bytes )
                return out.str() + ' ' + unspecified;

            out << std::hex << std::setfill( '0' );

            for ( const std::uint8_t byte : *bytes )
                out << ' ' << std::setw( 2 ) << static_cast< unsigned >( byte );

            return out.str();
        }
    }

    expected_pdu expected_pdu::with_crc_error() const
    {
        expected_pdu result = *this;
        result.crc_ok = false;

        return result;
    }

    expected_pdu received( std::span< const std::uint8_t > bytes )
    {
        return { .direction = pdu_direction::received, .crc_ok = true, .data = { { bytes.begin(), bytes.end() } } };
    }

    expected_pdu received_anything()
    {
        return { .direction = pdu_direction::received, .crc_ok = true };
    }

    expected_pdu sent( std::span< const std::uint8_t > bytes )
    {
        return { .direction = pdu_direction::transmitted, .data = { { bytes.begin(), bytes.end() } } };
    }

    expected_pdu anything()
    {
        return {};
    }

    std::string as_text( const captured_pdu& entry )
    {
        return as_line( entry.direction, entry.crc_ok, bytes_of( entry ) );
    }

    std::string as_text( const captured_pdu& entry, const expected_pdu& required )
    {
        return as_line(
            required.direction ? std::optional( entry.direction ) : std::nullopt,
            required.crc_ok    ? std::optional( entry.crc_ok )    : std::nullopt,
            required.data      ? std::optional( bytes_of( entry ) ) : std::nullopt );
    }

    std::string as_text( const expected_pdu& expected )
    {
        return as_line( expected.direction, expected.crc_ok,
            expected.data ? std::optional( std::span< const std::uint8_t >( *expected.data ) ) : std::nullopt );
    }

    std::string as_text( const std::vector< captured_pdu >& captured )
    {
        std::ostringstream out;

        for ( std::size_t index = 0; index != captured.size(); ++index )
            out << "    " << index << ": " << as_text( captured[ index ] ) << '\n';

        return out.str();
    }

    std::vector< std::string > differences(
        const std::vector< captured_pdu >& captured, const std::vector< expected_pdu >& expected )
    {
        std::vector< std::string > result;

        for ( std::size_t index = 0; index != std::min( captured.size(), expected.size() ); ++index )
        {
            const std::string found    = as_text( captured[ index ], expected[ index ] );
            const std::string required = as_text( expected[ index ] );

            if ( found != required )
                result.push_back( "PDU " + std::to_string( index ) + ": " + found + " != " + required );
        }

        return result;
    }

    void check_captured( const std::vector< captured_pdu >& captured, const std::vector< expected_pdu >& expected )
    {
        BOOST_TEST_CONTEXT( "timeline:\n" << as_text( captured ) )
        {
            for ( const std::string& difference : differences( captured, expected ) )
                BOOST_ERROR( difference );

            BOOST_REQUIRE_EQUAL( captured.size(), expected.size() );
        }
    }
}
}
