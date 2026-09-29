/*
 * The spike of the CC2340 radio port: no Bluetoe yet, the Radio Control Layer alone. An
 * ADV_IND from the address the radio tests know, on channel 37, every 100 ms at an absolute
 * start time of the system timer, the LED toggled at every event. What it answers: whether the
 * toolchain builds and links against the SDK's libraries, how large the result is, and, on the
 * tester, whether the start times hold the interval as the scheduled radio's concept needs.
 */
#include <stdint.h>

#include <NoRTOS.h>
#include <ti/drivers/Board.h>
#include <ti/drivers/GPIO.h>
#include <ti/drivers/RCL.h>
#include <ti/drivers/rcl/RCL_Scheduler.h>
#include <ti/drivers/rcl/commands/ble5.h>

#include "ti_drivers_config.h"
#include "ti_radio_config.h"

/* the address the device under test advertises from, in the radio tests: public */
static const uint8_t device_address[ 6 ] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0xc0 };

/* the advertising interval, in the system timer's 0.25 us */
#define ADVERTISING_INTERVAL RCL_SCHEDULER_SYSTIM_MS( 100 )

/* the first event this long after the start, so that the command is well ahead */
#define FIRST_EVENT_DELAY RCL_SCHEDULER_SYSTIM_MS( 10 )

static RCL_Client               client;
static RCL_CmdBle5Advertiser    command;
static RCL_CtxAdvertiser        context;
static RCL_StatsAdvScanInit     stats;

/* the transmit buffer of the ADV_IND: the buffer's own header, then the PDU header and payload */
#define ADVERTISING_PAYLOAD_SIZE ( 6 + 3 )
static uint32_t advertisement[ ( sizeof( RCL_Buffer_TxBuffer ) + 2 + ADVERTISING_PAYLOAD_SIZE + 3 ) / 4 ];

static volatile uint32_t events = 0;

static void on_command( RCL_Command* cmd, LRF_Events lrf, RCL_Events rcl )
{
    (void)cmd;
    (void)lrf;

    if ( rcl.lastCmdDone )
    {
        ++events;
        GPIO_toggle( CONFIG_GPIO_LED );
    }
}

/*
 * ADV_IND: header type 0, TxAdd 0 for a public address, length; AdvA; one AD structure, the
 * flags, general discoverable and BR/EDR not supported.
 */
static void build_advertisement( void )
{
    RCL_Buffer_TxBuffer* buffer = (RCL_Buffer_TxBuffer*)advertisement;
    uint8_t*             pdu    = RCL_TxBuffer_init( buffer, 0, 2, ADVERTISING_PAYLOAD_SIZE );

    pdu[ 0 ] = 0x00;
    pdu[ 1 ] = ADVERTISING_PAYLOAD_SIZE;

    for ( int i = 0; i != 6; ++i )
        pdu[ 2 + i ] = device_address[ i ];

    pdu[ 8 ]  = 0x02;
    pdu[ 9 ]  = 0x01;
    pdu[ 10 ] = 0x06;

    RCL_TxBuffer_put( &context.txBuffers, buffer );
}

int main( void )
{
    Board_init();
    NoRTOS_start();

    GPIO_setConfig( CONFIG_GPIO_LED, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW );

    RCL_init();
    RCL_Handle radio = RCL_open( &client, &LRF_config );

    context = RCL_CtxAdvertiser_DefaultRuntime();
    context.advA[ 0 ] = (uint16_t)( device_address[ 0 ] | ( device_address[ 1 ] << 8 ) );
    context.advA[ 1 ] = (uint16_t)( device_address[ 2 ] | ( device_address[ 3 ] << 8 ) );
    context.advA[ 2 ] = (uint16_t)( device_address[ 4 ] | ( device_address[ 5 ] << 8 ) );
    context.addrType.own = 0;

    command = RCL_CmdBle5Advertiser_DefaultRuntime();
    command.ctx     = &context;
    command.stats   = &stats;
    command.chanMap = 0x1;
    command.common.runtime.callback             = on_command;
    command.common.runtime.rclCallbackMask.value = RCL_EventLastCmdDone.value;

    uint32_t start = RCL_Scheduler_getCurrentTime() + FIRST_EVENT_DELAY;

    for ( ;; )
    {
        build_advertisement();

        command.common.scheduling          = RCL_Schedule_AbsTime;
        command.common.timing.absStartTime = start;
        command.common.status              = RCL_CommandStatus_Idle;

        RCL_Command_submit( radio, &command );
        RCL_Command_pend( &command );

        start += ADVERTISING_INTERVAL;
    }
}
