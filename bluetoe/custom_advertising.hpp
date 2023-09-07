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
        constexpr bool advertising_data_dirty() const
        {
            return false;
        }

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

    struct auto_advertising_data
    {
        /** @cond HIDDEN_SYMBOLS */
        constexpr bool advertising_data_dirty() const
        {
            return false;
        }

        struct meta_type :
            details::advertising_data_meta_type,
            details::valid_server_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief provides data to be advertised at runtime
     *
     * Add the set_runtime_custom_advertising_data() function to the server.
     * By default, the advertings data is of length 0.
     */
    struct runtime_custom_advertising_data
    {
        /**
         * @brief set the advertising data to be used.
         *
         * The function copies to the data given by begin and buffer_size into an
         * internal buffer.
         */
        void set_runtime_custom_advertising_data( const std::uint8_t* begin, std::size_t buffer_size )
        {
            advertising_data_size_ = std::min( std::size_t{ advertising_data_max_size }, buffer_size );
            std::copy( &begin[ 0 ], &begin[ advertising_data_size_ ], &advertising_data_[ 0 ] );

            dirty_ = true;
        }

        /** @cond HIDDEN_SYMBOLS */
        runtime_custom_advertising_data()
            : advertising_data_size_( 0 )
            , dirty_( false )
        {
        }

        bool advertising_data_dirty()
        {
            const bool result = dirty_;
            dirty_ = false;

            return result;
        }

        std::size_t advertising_data( std::uint8_t* begin, std::size_t buffer_size ) const
        {
            const std::size_t copy_size = std::min( advertising_data_size_, buffer_size );
            std::copy( &advertising_data_[ 0 ], &advertising_data_[ copy_size ], begin );

            return copy_size;
        }

        struct meta_type :
            details::advertising_data_meta_type,
            details::valid_server_option_meta_type {};

    private:
        static constexpr std::size_t advertising_data_max_size = 31;
        std::uint8_t advertising_data_[ advertising_data_max_size ];
        std::size_t  advertising_data_size_;
        bool dirty_;
        /** @endcond */
    };

    /**
     * @brief provides data to be use in response to a scan request
     *
     * @sa runtime_custom_scan_response_data
     * @sa auto_scan_response_data
     */
    template < std::size_t Size, const std::uint8_t (&Data)[ Size ] >
    struct custom_scan_response_data
    {
        /** @cond HIDDEN_SYMBOLS */
        constexpr bool scan_response_data_dirty() const
        {
            return false;
        }

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

    /**
     * @brief options, that allows Bluetoe to create the scan response data
     *
     * This is the default and currently, the implementation provides an empty
     * scan response.
     *
     * @sa runtime_custom_scan_response_data
     * @sa custom_scan_response_data
     */
    struct auto_scan_response_data
    {
        /** @cond HIDDEN_SYMBOLS */
        constexpr bool scan_response_data_dirty() const
        {
            return false;
        }

        struct meta_type :
            details::scan_response_data_meta_type,
            details::valid_server_option_meta_type {};
        /** @endcond */
    };

    /**
     * @brief option, that allows to define the scan response data at runtime
     *
     * @sa auto_scan_response_data
     * @sa custom_scan_response_data
     */
    struct runtime_custom_scan_response_data
    {
        /**
         * @brief set the scan response data to be used.
         *
         * The function copies to the data given by begin and buffer_size into an
         * internal buffer.
         */
        void set_runtime_custom_scan_response_data( const std::uint8_t* begin, std::size_t buffer_size )
        {
            scan_response_data_size_ = std::min( std::size_t{ scan_response_data_max_size }, buffer_size );
            std::copy( &begin[ 0 ], &begin[ scan_response_data_size_ ], &scan_response_data_[ 0 ] );

            dirty_ = true;
        }

        /** @cond HIDDEN_SYMBOLS */
        runtime_custom_scan_response_data()
            : scan_response_data_size_( 0 )
            , dirty_( false )
        {
        }

        std::size_t scan_response_data( std::uint8_t* begin, std::size_t buffer_size ) const
        {
            const std::size_t copy_size = std::min( scan_response_data_size_, buffer_size );
            std::copy( &scan_response_data_[ 0 ], &scan_response_data_[ copy_size ], begin );

            return copy_size;
        }

        bool scan_response_data_dirty()
        {
            const bool result = dirty_;
            dirty_ = false;

            return result;
        }

        struct meta_type :
            details::scan_response_data_meta_type,
            details::valid_server_option_meta_type {};

    private:
        static constexpr std::size_t scan_response_data_max_size = 31;
        std::uint8_t scan_response_data_[ scan_response_data_max_size ];
        std::size_t  scan_response_data_size_;
        bool dirty_;
        /** @endcond */
    };
}
#endif

