#include <bluetoe/server.hpp>
#include <bluetoe/device.hpp>
#include <bluetoe/services/csc.hpp>
#include <bluetoe/sensor_location.hpp>
#include <nrf.h>

struct handler {

    /*
     * Functions required by the bluetoe CSC implementation to support wheel and crank revolutions
     */
    std::pair< std::uint32_t, std::uint16_t > cumulative_wheel_revolutions_and_time()
    {
        return std::make_pair( wheel_revolutions_, last_wheel_event_time_ );
    }

    std::pair< std::uint16_t, std::uint16_t > cumulative_crank_revolutions_and_time()
    {
        return std::make_pair( crank_revolutions_, last_crank_event_time_ );
    }

    void set_cumulative_wheel_revolutions( std::uint32_t new_value );

    volatile std::uint32_t wheel_revolutions_;
    volatile std::uint16_t last_wheel_event_time_;

    volatile std::uint32_t crank_revolutions_;
    volatile std::uint16_t last_crank_event_time_;
};

static constexpr char server_name[] = "Gruener Blitz";

using bicycle = bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::appearance::cycling_speed_and_cadence_sensor,
    bluetoe::cycling_speed_and_cadence<
        bluetoe::sensor_location::hip,
        bluetoe::csc::wheel_revolution_data_supported,
        bluetoe::csc::crank_revolution_data_supported,
        bluetoe::csc::handler< handler >
    >
>;

bluetoe::device< bicycle > server;

void handler::set_cumulative_wheel_revolutions( std::uint32_t new_value )
{
    wheel_revolutions_ = new_value;
    server.confirm_cumulative_wheel_revolutions( server );
}

void init_bike_hardware();

int main()
{
    init_bike_hardware();

    for ( ;; )
        server.run( );
}

static constexpr int wheel_pin_nr       = 16;
static constexpr int wheel_channel_nr   = 0;
static constexpr int cadence_pin_nr     = 17;
static constexpr int cadence_channel_nr = 1;
static constexpr int timer_devider      = 100;

// for easier debugging
static NRF_GPIOTE_Type* const nrf_gpiote = NRF_GPIOTE;
static NRF_GPIO_Type* const   nrf_gpio   = NRF_GPIO;
static NRF_TIMER_Type* const  nrf_timer1 = NRF_TIMER1;

static volatile std::uint16_t now       = 0;
static volatile std::uint16_t last_now  = 0;

extern "C" void TIMER1_IRQHandler()
{
    nrf_timer1->EVENTS_COMPARE[ 0 ] = 0;
    now += timer_devider;
}

extern "C" void GPIOTE_IRQHandler()
{
    if ( nrf_gpiote->EVENTS_IN[ wheel_channel_nr ] )
    {
        ++server.wheel_revolutions_;
        server.last_wheel_event_time_ = now;
        nrf_gpiote->EVENTS_IN[ wheel_channel_nr ] = 0;
    }

    if ( nrf_gpiote->EVENTS_IN[ cadence_channel_nr ] )
    {
        ++server.crank_revolutions_;
        server.last_crank_event_time_ = now;
        nrf_gpiote->EVENTS_IN[ cadence_channel_nr ] = 0;
    }

    server.notify_timed_update( server );
}

void init_bike_hardware()
{
    nrf_gpio->PIN_CNF[ wheel_pin_nr ] =
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);

    nrf_gpio->PIN_CNF[ cadence_pin_nr ] =
        (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos);

    // Enable interrupts on rising age on pin
    nrf_gpiote->CONFIG[ wheel_channel_nr ] =
        (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos)
    |   (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
    |   (wheel_pin_nr << GPIOTE_CONFIG_PSEL_Pos);

    nrf_gpiote->CONFIG[ cadence_channel_nr ] =
        (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos)
    |   (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos)
    |   (cadence_pin_nr << GPIOTE_CONFIG_PSEL_Pos);

    nrf_gpiote->INTENSET =
        (GPIOTE_INTENSET_IN0_Enabled << GPIOTE_INTENSET_IN0_Pos)
    |   (GPIOTE_INTENSET_IN1_Enabled << GPIOTE_INTENSET_IN1_Pos);

    nrf_gpiote->EVENTS_IN[ wheel_channel_nr ] = 0;
    nrf_gpiote->EVENTS_IN[ cadence_channel_nr ] = 0;

    NVIC_SetPriority( GPIOTE_IRQn, 3 );
    NVIC_ClearPendingIRQ( GPIOTE_IRQn );
    NVIC_EnableIRQ( GPIOTE_IRQn );

    nrf_timer1->INTENCLR = 0xffff;
    nrf_timer1->MODE     =
        (TIMER_MODE_MODE_Timer << TIMER_MODE_MODE_Pos);

    nrf_timer1->BITMODE  =
        (TIMER_BITMODE_BITMODE_16Bit << TIMER_BITMODE_BITMODE_Pos);

    // for 1024hz, we would need a prescale of lb(13.931)
    // this makes 16Mhz / 2^9 == 31.25khz
    nrf_timer1->PRESCALER = 9;

    // if we want the ISR to be called with 1024hz/100, we would reset the timer at
    nrf_timer1->CC[ 0 ] = 31250 * timer_devider / 1024;
    nrf_timer1->SHORTS  =
        (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos);
    nrf_timer1->INTENSET =
        (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);

    NVIC_SetPriority( TIMER1_IRQn, 2 );
    NVIC_ClearPendingIRQ( TIMER1_IRQn );
    NVIC_EnableIRQ( TIMER1_IRQn );

    nrf_timer1->TASKS_START = 1;
}
