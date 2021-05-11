#ifndef BLUETOE_CUSTOM_ADVERTISING_HPP
#define BLUETOE_CUSTOM_ADVERTISING_HPP

#include <bluetoe/meta_types.hpp>

#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace bluetoe {

    /**
     * @brief provides data to be advertised
     */
    template < std::size_t Size, const std::uint8_t (&Data)[ Size ] >
    struct custom_advertising_data
    {
        /** @cond HIDDEN_SYMBOLS */
        std::size_t advertising_data( std::uint8_t* begin, std::size_t buffer_size ) const
        {
            const std::size_t copy_size = std::min( Size, buffer_size );
            std::copy( &Data[ 0 ], &Data[ copy_size ], begin );

            return copy_size;
        }

        struct meta_type :
            details::advertising_data_meta_type,
            details::valid_server_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief provides data to be use in response to a scan request
     */
    template < std::size_t Size, const std::uint8_t (&Data)[ Size ] >
    struct custom_scan_response_data
    {
        /** @cond HIDDEN_SYMBOLS */
        std::size_t scan_response_data( std::uint8_t* begin, std::size_t buffer_size ) const
        {
            const std::size_t copy_size = std::min( Size, buffer_size );
            std::copy( &Data[ 0 ], &Data[ copy_size ], begin );

            return copy_size;
        }

        struct meta_type :
            details::scan_response_data_meta_type,
            details::valid_server_option_meta_type {};
        /** @endcond */
    };
}
#endif

