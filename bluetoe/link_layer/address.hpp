#ifndef BLUETOE_LINK_LAYER_ADDRESS_HPP
#define BLUETOE_LINK_LAYER_ADDRESS_HPP

#include <cstdint>
#include <initializer_list>
#include <iosfwd>

namespace bluetoe {
namespace link_layer {

    /**
     * @brief a 48-bit universal LAN MAC address
     */
    class address
    {
    public:
        /*+
         * @brief creates the address with all octes beeing zero (00:00:00:00:00:00)
         */
        address();

        /**
         * @brief initialize an address by a initializer list with exactly 6 elements
         */
        explicit address( const std::initializer_list< std::uint8_t >& initial_values );

        /**
         * @brief initializing an address by taking 6 bytes from the given start of an array
         */
        explicit address( const std::uint8_t* initial_values );

        /**
         * @brief generates a valid static random address out of a given seed value
         *
         * The function generates a random addess that is valid and that has always the same value for the same seed.
         */
        static address generate_static_random_address( std::uint32_t seed );

        /**
         * @brief prints this in a human readable manner
         */
        std::ostream& print( std::ostream& ) const;

        /**
         * @brief returns the most significant byte of the address
         */
        std::uint8_t msb() const;

        /**
         * @brief returns true, if this address is the same as the rhs address
         */
        bool operator==( const address& rhs ) const;

        /**
         * @brief returns false, if this address is the same as the rhs address
         */
        bool operator!=( const address& rhs ) const;

        /**
         * @brief random access iterator
         */
        typedef std::uint8_t const * const_iterator;

        /**
         * @brief returns an iterator to the first byte (LSB) of the address
         */
        const_iterator begin() const;

        /**
         * @brief returns an iterator one behind the last byte of the address
         */
        const_iterator end() const;
    private:
        static constexpr std::size_t address_size_in_bytes = 6;
        std::uint8_t value_[ address_size_in_bytes ];

    };

    /**
     * @brief prints the given address in a human readable manner
     */
    std::ostream& operator<<( std::ostream& out, const address& a );
}
}

#endif