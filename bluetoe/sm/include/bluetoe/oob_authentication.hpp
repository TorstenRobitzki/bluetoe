#ifndef BLUETOE_SM_INCLUDE_OOB_AUTHENTICATION_HPP
#define BLUETOE_SM_INCLUDE_OOB_AUTHENTICATION_HPP

#include <bluetoe/ll_meta_types.hpp>

#include <cstdint>
#include <array>
#include <utility>

namespace bluetoe {

    namespace details {
        struct oob_authentication_callback_meta_type {};
    }

    /**
     * @brief 128 bit of out of band authentication data
     */
    using oob_authentication_data_t = std::array< std::uint8_t, 16 >;

    /**
     * @brief interface to provide OOB data to the pairing process
     *
     * Provides a mean to install a callback that is called, once a pairing request is accepted, to provide
     * the OOB key for the requesting device.
     *
     * The parameter T have to be a class type with following none static member
     * function:
     *
     * std::pair< bool, bluetoe::oob_authentication_data_t > sm_oob_authentication_data( const bluetoe::link_layer::device_address& address );
     *
     * If the first member of the returned pair is true, the second member contains the OOB data for the requesting device identified
     * by the given address.
     *
     * This option is ment to be passed as a link layer option to the selected device binding.
     */
    template < typename T, T& Obj >
    class oob_authentication_callback
    {
        /** @cond HIDDEN_SYMBOLS */
    public:
        oob_authentication_callback()
            : oob_data_present_( false )
        {
        }

        void request_oob_data_presents_for_remote_device( const bluetoe::link_layer::device_address& address )
        {
            std::tie( oob_data_present_, oob_data_ ) = Obj.sm_oob_authentication_data( address );
        }

        bool has_oob_data_for_remote_device() const
        {
            return oob_data_present_;
        }

        oob_authentication_data_t get_oob_data_for_last_remote_device() const
        {
            return oob_data_;
        };

        struct meta_type :
            details::oob_authentication_callback_meta_type,
            link_layer::details::valid_link_layer_option_meta_type {};

    private:
        bool oob_data_present_;
        std::array< std::uint8_t, 16 > oob_data_;
        /** @endcond */
    };

    namespace details {
        class no_oob_authentication
        {
        public:
            void request_oob_data_presents_for_remote_device( const bluetoe::link_layer::device_address& )
            {
            }

            bool has_oob_data_for_remote_device() const
            {
                return false;
            }

            std::array< std::uint8_t, 16 > get_oob_data_for_last_remote_device() const
            {
                return std::array< std::uint8_t, 16 >{{ 0 }};
            };

            struct meta_type :
                details::oob_authentication_callback_meta_type,
                link_layer::details::valid_link_layer_option_meta_type {};

        };
    }
}

#endif // include guard
