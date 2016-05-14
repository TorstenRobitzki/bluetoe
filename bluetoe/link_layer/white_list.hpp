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
         * @brief activate/deactivate the white list.
         *
         * If the white list is active, only scan request and connection requests with addresses in the white list
         * are supported. Make sure, the white list is only active, when waiting in advertising mode!
         */
        void activate_white_list( bool white_list_is_active );

        /**
         * @brief returns true, if the white list is active and the addr is in the list, or if the white list is not active
         */
        bool filtered( const device_address& addr ) const;

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
        template < class Radio >
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
            {
            }

            std::size_t white_list_free_size() const
            {
                return free_size_;
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

            void activate_white_list( bool white_list_is_active )
            {
                active_ = true;
            }

            bool filtered( const device_address& addr ) const
            {
                return !active_ || is_in_white_list( addr );
            }

        private:
            bool            active_;
            std::size_t     free_size_;
            device_address  addresses_[ Size ];
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

            void activate_white_list( bool white_list_is_active )
            {
                this_to_radio().radio_activate_white_list( white_list_is_active );
            }

            bool filtered( const device_address& addr ) const
            {
                return true;
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
