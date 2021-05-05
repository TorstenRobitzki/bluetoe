#include "attribute_io.hpp"

#include <ostream>


std::ostream& bluetoe::details::operator<<( std::ostream& out, const bluetoe::details::attribute_access_result& result )
{
    switch ( result )
    {
        case attribute_access_result::success:
            out << "success";
            break;

        case attribute_access_result::invalid_offset:
            out << "invalid_offset";
            break;

        case attribute_access_result::write_not_permitted:
            out << "write_not_permitted";
            break;

        case attribute_access_result::read_not_permitted:
            out << "read_not_permitted";
            break;

        case attribute_access_result::invalid_attribute_value_length:
            out << "invalid_attribute_value_length";
            break;

        case attribute_access_result::attribute_not_long:
            out << "attribute_not_long";
            break;

        case attribute_access_result::request_not_supported:
            out << "request_not_supported";
            break;

        case attribute_access_result::insufficient_encryption:
            out << "insufficient_encryption";
            break;

        case attribute_access_result::insufficient_authentication:
            out << "insufficient_authentication";
            break;

        case attribute_access_result::uuid_equal:
            out << "uuid_equal";
            break;

        case attribute_access_result::value_equal:
            out << "value_equal";
            break;

        default:
            out << "invalid_attribute_access_result(" << static_cast< int >( result ) << ")";
            break;
    }

    return out;
}
