#ifndef BLUETOE_BINDINGS_NRF_HPP
#define BLUETOE_BINDINGS_NRF_HPP

#include <bluetoe/meta_types.hpp>
#include <bluetoe/default_pdu_layout.hpp>

#include <nrf.h>

namespace bluetoe
{
    namespace nrf52_details {
        void gpio_debug_hfxo_stopped();
        void init_calibration_timer();
        void deassign_hfxo();
    }

    /**
     * @brief namespace with nRF51/52 specific configuration options
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
            struct leave_run_on_interrupt_type : radio_option_meta_type {};

            static void start_high_frequency_clock()
            {
                // This tasks starts the high frequency crystal oscillator (HFXO)
                nrf_clock->TASKS_HFCLKSTART = 1;

                // TODO: do not wait busy
                // Issue: do not poll for readiness of the high frequency clock #63
                while ( !nrf_clock->EVENTS_HFCLKSTARTED )
                    ;

                nrf_clock->EVENTS_HFCLKSTARTED = 0;
            }

            inline void start_lfclock_and_rtc()
            {
                nrf_clock->EVENTS_LFCLKSTARTED = 0;
                nrf_clock->TASKS_LFCLKSTART = 1;

                while ( nrf_clock->EVENTS_LFCLKSTARTED == 0 )
                    ;

                // https://infocenter.nordicsemi.com/topic/errata_nRF52840_Rev3/ERR/nRF52840/Rev3/latest/anomaly_840_20.html#anomaly_840_20
                nrf_rtc->TASKS_STOP = 0;
                nrf_rtc->TASKS_START = 1;

                // Configure the RTC to generate these two events
                // Overflow flag does not harm the power performance much and is used for
                // debugging.
                nrf_rtc->EVTEN =
                    ( RTC_EVTEN_COMPARE0_Enabled << RTC_EVTEN_COMPARE0_Pos )
                  | ( RTC_EVTEN_COMPARE1_Enabled << RTC_EVTEN_COMPARE1_Pos )
                  | ( RTC_EVTEN_OVRFLW_Enabled << RTC_EVTEN_OVRFLW_Pos );
            }
        }

        /**
         * @brief configure the low frequency clock to be sourced out of the high frequency clock
         *
         * The resulting sleep clock accurary is then the accuarcy of your high frequency clock source.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         * @sa bluetoe::nrf::calibrated_rc_sleep_clock
         */
        struct synthesized_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clocks()
            {
                nrf_details::start_high_frequency_clock();

                nrf_clock->LFCLKSRC = CLOCK_LFCLKSRCCOPY_SRC_Synth << CLOCK_LFCLKSRCCOPY_SRC_Pos;
                nrf_details::start_lfclock_and_rtc();
            }

            static void stop_high_frequency_crystal_oscilator()
            {
            }
            /** @endcond */
        };

        /**
         * @brief configure the low frequency clock to be sourced from a crystal oscilator
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::calibrated_rc_sleep_clock
         */
        struct sleep_clock_crystal_oscillator
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clocks()
            {
                nrf_details::start_high_frequency_clock();

                nrf_clock->LFCLKSRC = CLOCK_LFCLKSRCCOPY_SRC_Xtal << CLOCK_LFCLKSRCCOPY_SRC_Pos;
                nrf_details::start_lfclock_and_rtc();
            }

            static void stop_high_frequency_crystal_oscilator()
            {
                nrf_clock->TASKS_HFCLKSTOP = 1;

#               if defined BLUETOE_NRF52_RADIO_DEBUG
                    bluetoe::nrf52_details::gpio_debug_hfxo_stopped();
#               endif

            }
            /** @endcond */
        };

        /**
         * @brief configure the low frequency clock to run from the RC oscilator.
         *
         * That low frequency RC oscilator will be calibrated by the high frequency
         * crystal oscilator periodically.
         *
         * According to the datasheet, the resulting sleep clock accuarcy is then 500ppm.
         * If no sleep clock configuration is given, this is the default.
         *
         * @sa bluetoe::link_layer::sleep_clock_accuracy_ppm
         * @sa bluetoe::nrf::synthesized_sleep_clock
         * @sa bluetoe::nrf::sleep_clock_crystal_oscillator
         */
        struct calibrated_rc_sleep_clock
        {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::sleep_clock_source_meta_type;

            static void start_clocks()
            {
                nrf_details::start_high_frequency_clock();

                nrf_clock->LFCLKSRC = CLOCK_LFCLKSRCCOPY_SRC_RC << CLOCK_LFCLKSRCCOPY_SRC_Pos;
                nrf_details::start_lfclock_and_rtc();
                nrf52_details::init_calibration_timer();
            }

            static void stop_high_frequency_crystal_oscilator()
            {
                nrf52_details::deassign_hfxo();
            }
            /** @endcond */
        };

        /**
         * @brief configure the high frequency crystal oscillator startup time
         *
         * Unless bluetoe::nrf::synthesized_sleep_clock is used as the sleep clock
         * source, the nRF52 binding is switching on and off the high frequency clock
         * oscillator to save power. It's important that this parameter is in configured
         * to meet the real hardwares startup time to have the best power perfomance
         * _and_ a stable connection.
         *
         * The given value in µs is roundet up to the next full period of the low frequency
         * clock (30.52µs).
         *
         * If this configuration value is not given, 300µs (bluetoe::nrf::high_frequency_crystal_oscillator_startup_time_default)
         * is used as the default.
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
        using high_frequency_crystal_oscillator_startup_time_default = high_frequency_crystal_oscillator_startup_time< 300 >;

        /**
         * @brief configures the radio::run() function to return on every interrupt
         *
         * Usually, run() will return on a call to radio::wake(). With this option, run()
         * will only block for a single call to the WFI ARM assembler instruction. Once that
         * instruction returns, the function will be left.
         */
        struct leave_run_on_interrupt {
            /** @cond HIDDEN_SYMBOLS */
            using meta_type = nrf_details::leave_run_on_interrupt_type;
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

