/**
 * @brief the purpose of this example is to show:
 * - How pairing can be configured
 * - How bonding can be implemented
 */

#include <bluetoe/server.hpp>
#include <bluetoe/service.hpp>
#include <bluetoe/characteristic.hpp>
#include <bluetoe/device.hpp>

#include <nrf.h>

// Button 1 and LED 1 a Nordic PCA10056 eval board
static constexpr std::uint32_t input_pin    = 11u;
static constexpr std::uint32_t output_pin   = 13u;

static NRF_TIMER_Type& display_timer    = *NRF_TIMER1;
static NRF_TIMER_Type& debounce_timer   = *NRF_TIMER2;
static NRF_GPIOTE_Type& gpiote          = *NRF_GPIOTE;
static NRF_GPIO_Type&  port0            = *NRF_P0;

static constexpr std::uint32_t debounce_timeout = 1000u;

static const char server_name[] = "GPIO-Example";
static const char input_pin_name[] = "Button 1";
static const char output_pin_name[] = "LED 1";

static void set_output_pin( bool state )
{
    ( state
        ? static_cast< volatile std::uint32_t& >( port0.OUTCLR )
        : static_cast< volatile std::uint32_t& >( port0.OUTSET ) ) = ( 1 << output_pin );
}

static bool output_pin_state()
{
    return ( port0.OUT & ( 1 << output_pin ) ) == 0;
}

static bool input_pin_state()
{
    return ( port0.IN & ( 1 << input_pin ) ) == 0;
}

static bool input_pin_value = input_pin_state();

static std::uint8_t write_output_pin( bool state )
{
    set_output_pin( state );

    return bluetoe::error_codes::success;
}

using gatt_server = bluetoe::server<
    bluetoe::server_name< server_name >,
    bluetoe::service<
        bluetoe::requires_encryption,
        bluetoe::service_uuid< 0xDE346753, 0x5194, 0x4748, 0xADA4, 0xAB483FE1D370 >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xDE346753, 0x5194, 0x4748, 0xADA4, 0xAB483FE1D370 >,
            bluetoe::characteristic_name< input_pin_name >,
            bluetoe::notify,
            bluetoe::bind_characteristic_value< bool, &input_pin_value >
        >,
        bluetoe::characteristic<
            bluetoe::characteristic_uuid< 0xDE346753, 0x5194, 0x4748, 0xADA4, 0xAB483FE1D371 >,
            bluetoe::characteristic_name< output_pin_name >,
            bluetoe::free_write_handler< bool, write_output_pin >
        >
    >
>;

class pairing_io_t
{
public:
    pairing_io_t()
        : response_( nullptr )
        , timer_elapsed_( false )
    {
    }

    void sm_pairing_numeric_output( int pass_key )
    {
        store_pass_key( pass_key );
        display_digit_ = 0;

        setup_timer();
        display_first_digit();
    }

    void sm_pairing_yes_no( bluetoe::pairing_yes_no_response& response )
    {
        response_ = &response;
    }

    void interrupt_handler()
    {
        timer_elapsed_ = true;
    }

    void input_pin_toggled()
    {
        input_pin_toggled_ = true;
    }

    void handle_events();

private:
    void handle_key_press();
    void handle_display_timer();

    void store_pass_key( int pass_key )
    {
        int result = 0;

        for ( int digit = 0; digit != pass_key_length; ++digit )
        {
            auto& num_blinks = pass_key_[ pass_key_length - digit - 1 ];

            num_blinks = static_cast< std::uint8_t >( pass_key % 10 );
            if ( num_blinks == 0 )
                num_blinks = 10;

            pass_key /= 10;
        }
    }

    void display_first_digit()
    {
        --pass_key_[ display_digit_ ];
        set_output_pin( true );

        display_timer.CC[ 0 ]     = led_on_time;
        display_timer.TASKS_START = 1;
    }

    void setup_timer()
    {
        timer_elapsed_ = false;
        display_timer.EVENTS_COMPARE[ 0 ] = 0;
        display_timer.PRESCALER = ( 15 << TIMER_PRESCALER_PRESCALER_Pos );
        display_timer.BITMODE   = ( TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos );
        display_timer.SHORTS    =
            ( TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos )
          | ( TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos );
        display_timer.INTENSET = ( TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos );

        NVIC_ClearPendingIRQ( TIMER1_IRQn );
        NVIC_EnableIRQ( TIMER1_IRQn );
    }

    void disable_timer()
    {
        NVIC_DisableIRQ( TIMER1_IRQn );
        display_timer.INTENCLR = ( TIMER_INTENCLR_COMPARE0_Clear << TIMER_INTENCLR_COMPARE0_Pos );
        display_timer.TASKS_STOP = 1;
    }

    static constexpr int led_on_time     = 4000;
    static constexpr int led_off_time    = 10000;
    static constexpr int pause_time      = 3 * ( led_on_time + led_off_time );
    static constexpr int pass_key_length = 6;

    bluetoe::pairing_yes_no_response* response_;
    int display_digit_;
    std::uint8_t pass_key_[ pass_key_length ];
    volatile bool timer_elapsed_;
    volatile bool input_pin_toggled_;
} pairing_io;

gatt_server gatt;

bluetoe::device<
    gatt_server,
    // Support for legacy and LESC pairing
    bluetoe::security_manager,
    // Configure Link Layer to support at least an MTU of 65
    bluetoe::link_layer::buffer_sizes< 200, 200 >,
    bluetoe::link_layer::max_mtu_size< 65 >,
    // This application has some meaning to output a pass key and to input a yes/no
    bluetoe::pairing_numeric_output< pairing_io_t, pairing_io >,
    bluetoe::pairing_yes_no< pairing_io_t, pairing_io >
> server;

static void setup_input_gpiote()
{

}

static void init_hardware()
{
    set_output_pin( false );
    port0.PIN_CNF[ output_pin ] =
        ( GPIO_PIN_CNF_DRIVE_S0H1 << GPIO_PIN_CNF_DRIVE_Pos ) |
        ( GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos );

    port0.PIN_CNF[ input_pin ] =
        ( GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos )
      | ( GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos );

    gpiote.CONFIG[ 0 ] =
        ( GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos )
      | ( input_pin << GPIOTE_CONFIG_PSEL_Pos )
      | ( GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos );

    gpiote.EVENTS_IN[ 0 ] = 0;
    gpiote.INTENSET = ( GPIOTE_INTENSET_IN0_Set << GPIOTE_INTENSET_IN0_Pos );

    NVIC_ClearPendingIRQ( GPIOTE_IRQn );
    NVIC_EnableIRQ( GPIOTE_IRQn );
}

int main()
{
    init_hardware();

    for ( ;; )
    {
        pairing_io.handle_events();
        server.run( gatt );
    }
}

void pairing_io_t::handle_events()
{
    handle_key_press();
    handle_display_timer();
}

void pairing_io_t::handle_key_press()
{
    if ( !input_pin_toggled_ )
        return;

    input_pin_toggled_ = false;

    input_pin_value = input_pin_state();

    gatt.notify( input_pin_value );

    if ( response_ )
    {
        response_->yes_no_response( input_pin_value );
        response_ = nullptr;
    }
}

void pairing_io_t::handle_display_timer()
{
    if ( !timer_elapsed_ )
        return;

    timer_elapsed_ = false;

    if ( output_pin_state() )
    {
        // Switch to OFF
        set_output_pin( false );

        if ( pass_key_[ display_digit_ ] == 0 )
        {
            ++display_digit_;

            if ( display_digit_ == pass_key_length )
            {
                disable_timer();
                return;
            }

            display_timer.CC[ 0 ] = pause_time;
        }
        else
        {
            display_timer.CC[ 0 ] = led_off_time;
        }

        assert( pass_key_[ display_digit_ ] <= 10 );
        --pass_key_[ display_digit_ ];
    }
    else
    {
        // Switch to ON
        set_output_pin( true );
        display_timer.CC[ 0 ]    = led_on_time;
    }

    display_timer.TASKS_START = 1;
}

extern "C" void TIMER1_IRQHandler()
{
    display_timer.EVENTS_COMPARE[ 0 ] = 0;
    pairing_io.interrupt_handler();
}

extern "C" void TIMER2_IRQHandler()
{
    debounce_timer.INTENCLR            = ( TIMER_INTENCLR_COMPARE0_Enabled << TIMER_INTENCLR_COMPARE0_Pos );
    debounce_timer.EVENTS_COMPARE[ 0 ] = 0;

    if ( input_pin_value != input_pin_state() )
    {
        pairing_io.input_pin_toggled();
    }

    gpiote.INTENSET = ( GPIOTE_INTENSET_IN0_Set << GPIOTE_INTENSET_IN0_Pos );
}

extern "C" void GPIOTE_IRQHandler()
{
    // disable input interrupt, setup timer for debouncing
    gpiote.INTENCLR       = ( GPIOTE_INTENCLR_IN0_Clear << GPIOTE_INTENCLR_IN0_Pos );
    gpiote.EVENTS_IN[ 0 ] = 0;

    debounce_timer.EVENTS_COMPARE[ 0 ] = 0;
    debounce_timer.PRESCALER = ( 15 << TIMER_PRESCALER_PRESCALER_Pos );
    debounce_timer.BITMODE   = ( TIMER_BITMODE_BITMODE_32Bit << TIMER_BITMODE_BITMODE_Pos );
    debounce_timer.SHORTS    =
        ( TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos )
      | ( TIMER_SHORTS_COMPARE0_STOP_Enabled << TIMER_SHORTS_COMPARE0_STOP_Pos );
    debounce_timer.INTENSET = ( TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos );

    NVIC_ClearPendingIRQ( TIMER2_IRQn );
    NVIC_EnableIRQ( TIMER2_IRQn );

    debounce_timer.CC[ 0 ] = debounce_timeout;
    debounce_timer.TASKS_START = 1;

    pairing_io.input_pin_toggled();
}
