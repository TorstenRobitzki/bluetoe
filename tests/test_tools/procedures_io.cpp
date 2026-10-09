#include "procedures_io.hpp"

#include <ostream>


std::ostream& bluetoe::link_layer::details::operator<<( std::ostream& out, procedure_outcome oc )
{
    switch ( oc )
    {
    case procedure_outcome::handled:
        out << "handled";
        break;
    case procedure_outcome::stalled:
        out << "stalled";
        break;
    case procedure_outcome::disconnect:
        out << "disconnect";
        break;
    }

    return out;
}

std::ostream& bluetoe::link_layer::details::operator<<( std::ostream& out, const procedure_result& result )
{
    out << "{ outcome: " << result.outcome << "; reason: 0x" << std::hex << static_cast< int >( result.reason )
        << std::dec << " }";

    return out;
}
