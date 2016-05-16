#ifndef BLUETOE_LINK_LAYER_WHITE_LIST_HPP
#define BLUETOE_LINK_LAYER_WHITE_LIST_HPP

#include <bluetoe/link_layer/address.hpp>
#include <iterator>
#include <algorithm>

namespace bluetoe {
namespace link_layer {

    namespace details {
        struct white_list_meta_type {};

        template < std::size_t RequiredSize, bool SoftwareRequired, typename Radio, typename LinkLayer >
        class white_list_implementation;
    }

    /**
     * @brief adds a white list to the link layer with white list related functions.
     *
     * @tparam Size the maximum number of device addresses, the white list will contain.
     *
     * @note the functions is_connection_request_in_filter() and is_scan_request_in_filter() might
     *       return always true, for a hardware implementation and the hardware would then not
     *       call the receive callback for devices that are not within the white list.
     */
    template < std::size_t Size = 8 >
    class white_list
    {
    public:
        /** @cond HIDDEN_SYMBOLS */
        // this functions are purly for documentation purpose, the used implementations is in details::white_list_implementation
        /** @endcond */

        /**
         * @brief The maximum number of device addresses, the white list can contain.
         */
        static constexpr std::size_t maximum_white_list_entries = Size;

        /**
         * @brief add the given address to the white list.
         *
         * Function will return true, if it was possible to add the address to the white list
         * or if the address was already in the white list.
         * If there was not enough room to add the address to the white list, the function
         * returns false.
         */
        bool add_to_white_list( const device_address& addr );

        /**
         * @brief remove the given address from the white list
         *
         * The function returns true, if the address was in the list.
         * @post addr is not in the white list.
         */
        bool remove_from_white_list( const device_address& addr );

        /**
         * @brief returns true, if the given address in within the white list
         */
        bool is_in_white_list( const device_address& addr ) const;

        /**
         * @brief returns the number of addresses that could be added to the
         *        white list before add_to_white_list() would return false.
         */
        std::size_t white_list_free_size() const;

        /**
         * @brief remove all entries from the white list
         */
        void clear_white_list();

        /**
         * @brief Accept connection requests only from devices within the white list.
         *
         * If the property is set to true, only connection requests from from devices
         * that are in the white list, should be answered.
         * If the property is set to false, all connection requests should be answered.
         *
         * The default value of the is property is false.
         *
         * @post connection_request_filter() == b
         * @sa connection_request_filter()
         */
        void connection_request_filter( bool b );

        /**
         * @brief current value of the property.
         */
        bool connection_request_filter() const;

        /**
         * @brief Accept scan requests only from devices within the white list.
         *
         * If the property is set to true, only scan requests from from devices
         * that are in the white list, should be answered.
         * If the property is set to false, all scan requests should be answered.
         *
         * The default value of the is property is false.
         *
         * @post scan_request_filter() == b
         * @sa scan_request_filter()
         */
        void scan_request_filter( bool b );

        /**
         * @brief current value of the property.
         */
        bool scan_request_filter() const;

        /**
         * @brief returns true, if a connection request from the given address should be answered.
         */
        bool is_connection_request_in_filter( const device_address& addr ) const;

        /**
         * @brief returns true, if a scan request from the given address should be answered.
         */
        bool is_scan_request_in_filter( const device_address& addr ) const;

        /** @cond HIDDEN_SYMBOLS */
        typedef details::white_list_meta_type meta_type;

        template < class Radio, class LinkLayer >
        struct impl :
            details::white_list_implementation<
                Size,
                ( Size > Radio::radio_maximum_white_list_entries ),
                Radio,
                LinkLayer
            >
        {
        };
        /** @endcond */
    };

    /**
     * @brief no white list in the link layer
     *
     * This is the default
     */
    struct no_white_list {
        /** @cond HIDDEN_SYMBOLS */
        template < class Radio, class LinkLayer >
        struct impl {
        };

        typedef details::white_list_meta_type meta_type;
        /** @endcond */
    };

    namespace details {

        /**
         * pure software implementation
         */
        template < std::size_t Size, typename Radio, typename LinkLayer >
        class white_list_implementation< Size, true, Radio, LinkLayer >
        {
        public:
            static constexpr std::size_t maximum_white_list_entries = Size;

            white_list_implementation()
                : active_( false )
                , free_size_( Size )
                , connection_filter_( false )
                , scan_filter_( false )
            {
            }

            std::size_t white_list_free_size() const
            {
                return free_size_;
            }

            void clear_white_list()
            {
                free_size_ = Size;
            }

            bool add_to_white_list( const device_address& addr )
            {
                if ( is_in_white_list( addr ) )
                    return true;

                if ( free_size_ == 0 )
                    return false;

                addresses_[ Size - free_size_ ] = addr;
                --free_size_;
                return true;
            }

            bool is_in_white_list( const device_address& addr ) const
            {
                const auto end = std::begin( addresses_ ) + ( Size - free_size_ );
                return std::find( std::begin( addresses_ ), end, addr ) != end;
            }

            bool remove_from_white_list( const device_address& addr )
            {
                const auto end = std::begin( addresses_ ) + ( Size - free_size_ );
                const auto pos = std::find( std::begin( addresses_ ), end, addr );

                if ( pos == end )
                    return false;

                *pos = *( end - 1 );
                ++free_size_;

                return true;
            }

            void connection_request_filter( bool b )
            {
                connection_filter_ = b;
            }

            bool connection_request_filter() const
            {
                return connection_filter_;
            }

            void scan_request_filter( bool b )
            {
                scan_filter_ = b;
            }

            bool scan_request_filter() const
            {
                return scan_filter_;
            }

            bool is_connection_request_in_filter( const device_address& addr ) const
            {
                return !connection_filter_ || is_in_white_list( addr );
            }

            bool is_scan_request_in_filter( const device_address& addr ) const
            {
                return !scan_filter_ || is_in_white_list( addr );
            }

        private:
            bool            active_;
            std::size_t     free_size_;
            device_address  addresses_[ Size ];
            bool            connection_filter_;
            bool            scan_filter_;
        };

        /**
         * Hardware only implemenation
         */
        template < std::size_t Size, typename Radio, typename LinkLayer >
        class white_list_implementation< Size, false, Radio, LinkLayer >
        {
        public:
            static constexpr std::size_t maximum_white_list_entries = Size;

            std::size_t white_list_free_size() const
            {
                return this_to_radio().radio_white_list_free_size();
            }

            void clear_white_list()
            {
                this_to_radio().radio_clear_white_list();
            }

            bool add_to_white_list( const device_address& addr )
            {
                return this_to_radio().radio_add_to_white_list( addr );
            }

            bool is_in_white_list( const device_address& addr ) const
            {
                return this_to_radio().radio_is_in_white_list( addr );
            }

            bool remove_from_white_list( const device_address& addr )
            {
                return this_to_radio().radio_remove_from_white_list( addr );
            }

            void connection_request_filter( bool b )
            {
                this_to_radio().radio_connection_request_filter( b );
            }

            bool connection_request_filter() const
            {
                return this_to_radio().radio_connection_request_filter();
            }

            void scan_request_filter( bool b )
            {
                this_to_radio().radio_scan_request_filter( b );
            }

            bool scan_request_filter() const
            {
                return this_to_radio().radio_scan_request_filter();
            }

            bool is_connection_request_in_filter( const device_address& addr ) const
            {
                return this_to_radio().radio_is_connection_request_in_filter( addr );
            }

            bool is_scan_request_in_filter( const device_address& addr ) const
            {
                return this_to_radio().radio_is_scan_request_in_filter( addr );
            }
        private:
            Radio& this_to_radio()
            {
                return static_cast< Radio& >( static_cast< LinkLayer& >( *this ) );
            }

            const Radio& this_to_radio() const
            {
                return static_cast< const Radio& >( static_cast< const LinkLayer& >( *this ) );
            }
        };
    }
}
}
#endif
