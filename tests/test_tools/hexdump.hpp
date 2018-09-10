#ifndef FAG_TOOLS_HEXDUMP_HPP
#define FAG_TOOLS_HEXDUMP_HPP

#include <string>
#include <ostream>
#include <sstream>

/**
 * @brief returns an ascii encoded character that is printable
 *
 * If the input char is not printable, a '.' is returned otherwise
 * the char it self.
 */
char as_printable(char maybe_printable);

/**
 * @brief returns a string where none printable chars are replaced by a dot ('.').
 */
std::string as_printable(const std::string& input);

/**
 * @brief prints a byte as a hex text on the given stream
 */
void print_hex(std::ostream& out, char value);
void print_hex(std::ostream& out, unsigned char value);

/** @copydoc print_hex(std::ostream&, char) */
void print_hex_uppercase(std::ostream& out, unsigned char value);

/**
 * @brief prints a character sequence as a hex dump
 *
 * The sequence given by [begin, end) is printed in lines
 * of 16 chars, by first printing there hex values with
 * white spaces seperated followed by a textual representation.
 */
template <class Iter>
void hex_dump(std::ostream& out, Iter begin, Iter end)
{
    static const unsigned   page_width = 16;
    unsigned                char_cnt   = page_width;
    Iter                    line_start = begin;

    for ( ; begin != end && out; )
    {
        print_hex(out, *begin);
        out << ' ';
        ++begin;

        if ( --char_cnt == 0 || begin == end )
        {
            // fill rest of the line
            for ( ; char_cnt != 0; --char_cnt)
                out << "   ";

            for ( ; line_start != begin; ++line_start )
                out << as_printable(*line_start);

            out << '\n';
            char_cnt   = page_width;
        }
    }
}

/**
 * @brief converts a character sequence into a hex-dump
 *
 * this functions calls the function above with a stringstream
 * and returns the result as a text.
 */
template <class Iter>
std::string hex_dump(Iter begin, Iter end)
{
    std::ostringstream out;
    hex_dump(out, begin, end);

    return out.str();
}


/**
 * @brief creates a hexdump of the given object
 *
 * @param output the output, where the hexdump is made
 * @param object the adress of the object to be dumped
 * @param object_size the object size
 */
void hex_dump(std::ostream& output, const void* object, std::size_t object_size);


#endif //FAG_TOOLS_HEXDUMP_HPP

