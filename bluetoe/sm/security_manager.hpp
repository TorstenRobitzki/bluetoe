#ifndef BLUETOE_SM_SECURITY_MANAGER_HPP
#define BLUETOE_SM_SECURITY_MANAGER_HPP

#include <cstddef>

namespace bluetoe {
    namespace details {
        struct security_manager_meta_type {};
    }

    class security_manager
    {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        typedef details::security_manager_meta_type meta_type;
    };

    class no_security_manager
    {
    public:
        void l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size );

        typedef details::security_manager_meta_type meta_type;
    };


    /*
     * Implementation
     */
    inline void security_manager::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
    }

    inline void no_security_manager::l2cap_input( const std::uint8_t* input, std::size_t in_size, std::uint8_t* output, std::size_t& out_size )
    {
        out_size = 0;
    }
}
#endif
