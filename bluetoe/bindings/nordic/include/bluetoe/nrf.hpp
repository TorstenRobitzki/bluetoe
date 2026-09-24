#ifndef BLUETOE_BINDINGS_NRF_HPP
#define BLUETOE_BINDINGS_NRF_HPP

#include <bluetoe/meta_types.hpp>
#include <bluetoe/default_pdu_layout.hpp>

#include <nrf.h>

namespace bluetoe
{
    /**
     * @brief namespace with nRF52 specific configuration options
     */
    namespace nrf
    {
        /*
         * Some aliases that can be used in the debugger
         */
        static NRF_RADIO_Type* const        nrf_radio            = NRF_RADIO;
        static NRF_TIMER_Type* const        nrf_timer            = NRF_TIMER0;
        static NRF_TIMER_Type* const        nrf_cb_timer         = NRF_TIMER1;
        static NRF_CLOCK_Type* const        nrf_clock            = NRF_CLOCK;
        static NRF_TEMP_Type* const         nrf_temp             = NRF_TEMP;
        static NRF_RTC_Type* const          nrf_rtc              = NRF_RTC0;
        static NRF_CCM_Type* const          nrf_ccm              = NRF_CCM;
        static NRF_AAR_Type* const          nrf_aar              = NRF_AAR;
        static NRF_PPI_Type* const          nrf_ppi              = NRF_PPI;
        static NRF_RNG_Type* const          nrf_random           = NRF_RNG;
        static NRF_ECB_Type* const          nrf_aes              = NRF_ECB;
        static NRF_GPIOTE_Type* const       nrf_gpiote           = NRF_GPIOTE;
        static NVIC_Type* const             nvic                 = NVIC;

        static constexpr auto lfxo_clk_freq = 32768;

        /*
         * Interrupt priorities
         */
        static constexpr uint32_t nrf_interrupt_prio_ble = 0;
        static constexpr uint32_t nrf_interrupt_prio_user_cb = 1 << ( __NVIC_PRIO_BITS - 1 );
        static constexpr uint32_t nrf_interrupt_prio_calibrate_rtc = nrf_interrupt_prio_user_cb - 1;

        namespace nrf_details {
            struct radio_option_meta_type : ::bluetoe::details::binding_option_meta_type {};
            struct sleep_clock_source_meta_type : radio_option_meta_type {};
            struct hfxo_startup_time_meta_type : radio_option_meta_type {};
            struct clock_statistics_meta_type : radio_option_meta_type {};
        }

        /**
         * @brief the sleep clock is synthesized from the high frequency crystal, which then
         *        stays on
         *
         * The default. The sleep clock's accuracy is the crystal's, 20 ppm on the
         * development kits, and nothing has to start up before a radio event, at the price
         * of the crystal's current while the radio is idle. The choice for a device with a
         * power supply, and for a test rig.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         * @sa bluetoe::nrf::calibrated_rc_sleep_clock
         */
        struct synthesized_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;
            /** @endcond */
        };

        /**
         * @brief the sleep clock is the 32.768 kHz crystal
         *
         * Only the sleep clock runs while the radio is idle; the high frequency crystal is
         * started before every radio event and stopped after it, see
         * high_frequency_crystal_oscillator_startup_time. The accuracy is the crystal's,
         * 20 ppm on the development kits.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::calibrated_rc_sleep_clock
         */
        struct sleep_clock_crystal_oscillator
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;
            /** @endcond */
        };

        /**
         * @brief the sleep clock is the RC oscillator, calibrated against the high
         *        frequency crystal
         *
         * Like sleep_clock_crystal_oscillator, for a device without a 32.768 kHz crystal.
         * The RC oscillator is calibrated periodically while the high frequency crystal
         * runs for a radio event anyway; according to the datasheet the accuracy is then
         * 500 ppm.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         */
        struct calibrated_rc_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;
            /** @endcond */
        };

        /**
         * @brief how long the high frequency crystal takes to start
         *
         * With a sleep clock other than bluetoe::nrf::synthesized_sleep_clock the radio
         * switches the high frequency crystal on before every radio event and off after
         * it, and starts it this long before the event, rounded up to whole periods of
         * the sleep clock (30.52 µs). A value below the crystal's real startup time lets
         * the radio transmit on a clock that has not settled; a value above it costs the
         * difference in current at every event.
         *
         * If not given, 400 µs (bluetoe::nrf::high_frequency_crystal_oscillator_startup_time_default):
         * the crystal of the nRF52840-DK takes 347 µs.
         *
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         * @sa bluetoe::nrf::calibrated_rc_sleep_clock
         */
        template < unsigned StartupTimeMicroSeconds >
        struct high_frequency_crystal_oscillator_startup_time
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::hfxo_startup_time_meta_type;

            static constexpr unsigned value = StartupTimeMicroSeconds;
            /** @endcond */
        };

        /**
         * @brief default value for the high frequency crystal oscillator startup time
         *
         * @sa bluetoe::nrf::high_frequency_crystal_oscillator_startup_time
         */
        using high_frequency_crystal_oscillator_startup_time_default = high_frequency_crystal_oscillator_startup_time< 400 >;

        /**
         * @brief count what the radio does with its clocks, for a test to read
         *
         * The radio counts how often it started the high frequency crystal, how many periods
         * of the sleep clock the crystal ran in all, and how often it calibrated the RC sleep
         * clock, and hands the counts out through its clock_statistics(). A firmware has no
         * use for them; the test rigs of the radio have, for a run that has to show the
         * crystal off between events and the calibration at its pace. Costs a few words of
         * RAM and a few instructions at every start and stop of the crystal.
         *
         * @sa bluetoe::nrf52_details::radio_base_t::clock_statistics
         */
        struct clock_statistics
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::clock_statistics_meta_type;
            /** @endcond */
        };

    }

    namespace nrf_details
    {
        struct encrypted_pdu_layout : bluetoe::link_layer::details::layout_base< encrypted_pdu_layout >
        {
            /** @cond HIDDEN_SYMBOLS */
            static constexpr std::size_t header_size = sizeof( std::uint16_t );

            using bluetoe::link_layer::details::layout_base< encrypted_pdu_layout >::header;

            static std::uint16_t header( const std::uint8_t* pdu )
            {
                return ::bluetoe::details::read_16bit( pdu );
            }

            static void header( std::uint8_t* pdu, std::uint16_t header_value )
            {
                ::bluetoe::details::write_16bit( pdu, header_value );
            }

            static std::pair< std::uint8_t*, std::uint8_t* > body( const link_layer::read_buffer& pdu )
            {
                assert( pdu.size >= header_size );

                return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
            }

            static std::pair< const std::uint8_t*, const std::uint8_t* > body( const link_layer::write_buffer& pdu )
            {
                assert( pdu.size >= header_size );

                return { &pdu.buffer[ header_size + 1 ], &pdu.buffer[ pdu.size ] };
            }

            static constexpr std::size_t data_channel_pdu_memory_size( std::size_t payload_size )
            {
                return header_size + payload_size + 1;
            }
            /** @endcond */
        };
    }
}

#endif

