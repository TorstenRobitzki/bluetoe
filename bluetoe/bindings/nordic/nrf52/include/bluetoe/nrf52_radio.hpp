#ifndef BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP
#define BLUETOE_BINDINGS_NORDIC_NRF52_NRF52_RADIO_HPP

/**
 * @file nrf52_radio.hpp
 *
 * The scheduled radio of the nRF52, as bluetoe/scheduled_radio2.hpp requires it. Consumers
 * name it through <bluetoe/radio.hpp>.
 *
 * This is the first slice, decision 22 of documentation/scheduled_radio_test_rig.md: the
 * security toolbox, run() and wake_up(), and the feature constants; every scheduling
 * function is present and declines. The radio and the timer come with step 3 of decision 11.
 */

#include <bluetoe/security_tool_box.hpp>

#include <bluetoe/abs_time.hpp>
#include <bluetoe/address.hpp>
#include <bluetoe/buffer.hpp>
#include <bluetoe/phy_encodings.hpp>

#include <cstddef>
#include <cstdint>

namespace bluetoe
{
    namespace nrf52_details
    {
        /**
         * @brief what does not depend on the callbacks type: the execution context, and
         *        the setup the toolbox needs
         */
        class radio_base
        {
        protected:
            /**
             * @brief starts the random number generator the toolbox draws from
             */
            radio_base();

            /**
             * @brief sleeps until something happened, then returns
             *
             * Wait-for-event returns on an interrupt and on wake_up(); the event register
             * latches a wake_up() that came before the sleep, which is what makes the
             * guarantee of decision 17 hold.
             */
            void run();

            /**
             * @brief makes run() return, from any context
             */
            void wake_up();
        };

        /**
         * @brief the scheduled radio of the nRF52
         *
         * CallBacks is the type the callbacks are delivered to, which derives from this class
         * and is reached through that relation. Options are the radio's options; there are
         * none yet.
         */
        template < typename CallBacks, typename... Options >
        class radio : public radio_base, public security_tool_box
        {
            static_assert( sizeof...( Options ) == 0, "the nRF52 scheduled radio has no options yet" );

        public:
            static constexpr bool           hardware_supports_encryption                = false;
            static constexpr bool           hardware_supports_lesc_pairing              = true;
            static constexpr bool           hardware_supports_legacy_pairing            = false;
            static constexpr bool           hardware_supports_2mbit                     = false;
            static constexpr bool           hardware_supports_synchronized_user_timer   = false;
            static constexpr bool           hardware_supports_link_layer_context        = false;

            /**
             * @brief the hardware generates preamble, access address and CRC itself and stores
             *        only the PDU
             */
            static constexpr std::size_t    radio_package_overhead                      = 0;
            static constexpr std::uint32_t  radio_max_supported_payload_length          = 255;

            /**
             * @brief the 32.768 kHz crystal of the development kits
             */
            static constexpr std::uint32_t  sleep_time_accuracy_ppm                     = 20;

            struct ccm_counter_t
            {
                std::uint64_t value = 0;
            };

            /**
             * @brief nothing to exclude: the callbacks are delivered from run()
             */
            struct lock_guard {};

            using radio_base::run;
            using radio_base::wake_up;

            /**
             * @name Setup and scheduling, not implemented yet
             *
             * Present so that the class satisfies the concept and the toolbox can be tested
             * over the link; every scheduling function declines.
             * @{
             */
            void set_access_address_and_crc_init( std::uint32_t, std::uint32_t ) {}
            void set_ccm_counter( const ccm_counter_t&, const ccm_counter_t& ) {}
            void set_phy( link_layer::phy_ll_encoding::phy_ll_encoding_t, link_layer::phy_ll_encoding::phy_ll_encoding_t ) {}
            void set_local_address( const link_layer::device_address& ) {}

            bool start_advertising( std::uint32_t, const link_layer::write_buffer&, const link_layer::write_buffer&, const link_layer::read_buffer& )
            {
                return false;
            }

            bool schedule_advertising_event( std::uint32_t, link_layer::abs_time, const link_layer::write_buffer&, const link_layer::write_buffer&, const link_layer::read_buffer& )
            {
                return false;
            }

            bool schedule_connection_event( std::uint32_t, link_layer::abs_time, link_layer::abs_time )
            {
                return false;
            }

            bool cancel_radio_event()
            {
                return false;
            }

            bool schedule_timer( link_layer::abs_time )
            {
                return false;
            }

            bool cancel_timer()
            {
                return false;
            }
            /** @} */
        };
    }
}

#endif
