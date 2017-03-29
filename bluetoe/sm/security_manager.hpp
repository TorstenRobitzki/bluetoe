#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <algorithm>

namespace bluetoe {
    namespace details {
        struct security_manager_meta_type {};
    }

    /**
     * @brief place holder for a security manager, that could be implemented
     */
    class security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        typedef details::security_manager_meta_type meta_type;
        /** @endcond */
    };

    /**
     * @brief current default implementation of the security manager, that actievly rejects every pairing attempt.
     */
    class no_security_manager
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        typedef details::security_manager_meta_type meta_type;
        /** @endcond */
    };


    /*
     * Implementation
     */
    /** @cond HIDDEN_SYMBOLS */
    inline void security_manager::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size )
    {
        static const std::uint8_t response[] = {
            0x02,   // response
            0x03,   // NoInputNoOutput
            0x00,   // OOB Authentication data not present
            0x40,   // Bonding, MITM = 0, SC = 0, Keypress = 0
            0x10,   // Maximum Encryption Key Size
            0x0f,   // LinkKey
            0x0f    // LinkKey
        };

        const std::size_t response_size = sizeof( response ) / sizeof( response[ 0 ] );

        assert( out_size >= response_size );
        std::copy( std::begin( response ), std::end( response ), output );

        out_size = response_size;
    }

    inline void no_security_manager::l2cap_input( const std::uint8_t*, std::size_t, std::uint8_t* output, std::size_t& out_size )
    {
        assert( out_size >= 2 );
        out_size = 2;
        output[ 0 ] = 0x05;
        output[ 1 ] = 0x05;
    }

    /** @endcond */
}
#endif
