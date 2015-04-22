#include "hexdump.hpp"
#include <algorithm>
#include <cassert>

char as_printable(char maybe_printable)
{
    return ( static_cast<unsigned char>(maybe_printable) > 31
        && static_cast<unsigned char>(maybe_printable) < 127 )
        ? maybe_printable
        : '.';
}

std::string as_printable(const std::string& input)
{
    std::string result(input);
    char (*f)(char) = &as_printable;
    std::transform(result.begin(), result.end(), result.begin(), f);

    return result;
}

void print_hex(std::ostream& out, char value)
{
    print_hex(out, static_cast<unsigned char>(value));
}

namespace {
    unsigned char nibble(unsigned char val)
    {
        assert(val < 16);
        return val < 10 ? '0' + val : 'a' + (val - 10);
    }

    unsigned char upper_nibble(unsigned char val)
    {
        assert(val < 16);
        return val < 10 ? '0' + val : 'A' + (val - 10);
    }
}

void print_hex(std::ostream& out, unsigned char value)
{
    out << nibble(value >> 4) << nibble(value & 0xf);
}

void print_hex_uppercase( std::ostream& out, unsigned char value )
{
    out << upper_nibble( value >> 4 ) << upper_nibble( value & 0xf );
}

void hex_dump(std::ostream& output,const void* object, std::size_t object_size)
{
    hex_dump(output, static_cast<const char*>(object), static_cast<const char*>(object) + object_size);
}

