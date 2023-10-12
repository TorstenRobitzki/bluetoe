#include "tester_config.hpp"
#include <nrf.h>

#include <initializer_list>

static NRF_QSPI_Type& nrf_qspi = *NRF_QSPI;
static NRF_GPIO_Type& nrf_p0   = *NRF_P0;

extern char external_flash_start_address;

// Make sure, that this function will be located in the internal flash memory
extern "C" void external_flash_init(void)
{
    nrf_qspi.PSEL.SCK = ext_flash_clk;
    nrf_qspi.PSEL.CSN = ext_flash_cs;
    nrf_qspi.PSEL.IO0 = ext_flash_d0;
    nrf_qspi.PSEL.IO1 = ext_flash_d1;
    nrf_qspi.PSEL.IO2 = ext_flash_d2;
    nrf_qspi.PSEL.IO3 = ext_flash_d3;

    for ( auto const pin: {
        ext_flash_clk, ext_flash_cs,
        ext_flash_d0, ext_flash_d1, ext_flash_d2, ext_flash_d3 } )
    {
        nrf_p0.PIN_CNF[ pin ] = ( GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos );
    }

    nrf_qspi.IFCONFIG0 =
        ( QSPI_IFCONFIG0_READOC_READ4IO << QSPI_IFCONFIG0_READOC_Pos )
      | ( QSPI_IFCONFIG0_WRITEOC_PP4IO << QSPI_IFCONFIG0_WRITEOC_Pos )
      | ( QSPI_IFCONFIG0_ADDRMODE_24BIT << QSPI_IFCONFIG0_ADDRMODE_Pos )
      | ( QSPI_IFCONFIG0_DPMENABLE_Disable << QSPI_IFCONFIG0_DPMENABLE_Pos )
      | ( QSPI_IFCONFIG0_PPSIZE_256Bytes << QSPI_IFCONFIG0_PPSIZE_Pos );

    nrf_qspi.IFCONFIG1 =
        ( 1 << QSPI_IFCONFIG1_SCKDELAY_Pos )
      | ( QSPI_IFCONFIG1_SPIMODE_MODE0 << QSPI_IFCONFIG1_SPIMODE_Pos )
      | ( 0 << QSPI_IFCONFIG1_SCKFREQ_Pos );

    nrf_qspi.XIPOFFSET = reinterpret_cast< std::uint32_t >( &external_flash_start_address );
    nrf_qspi.ENABLE = ( QSPI_ENABLE_ENABLE_Enabled << QSPI_ENABLE_ENABLE_Pos );

    nrf_qspi.TASKS_ACTIVATE = QSPI_TASKS_ACTIVATE_TASKS_ACTIVATE_Trigger;

    nrf_qspi.EVENTS_READY = 0;
    nrf_qspi.CINSTRDAT0 =
        ( 0x40 << QSPI_CINSTRDAT0_BYTE0_Pos )
      | ( 0x00 << QSPI_CINSTRDAT0_BYTE1_Pos )
      | ( 0x02 << QSPI_CINSTRDAT0_BYTE2_Pos );

    nrf_qspi.CINSTRCONF =
        ( 0b00000001 << QSPI_CINSTRCONF_OPCODE_Pos )
      | ( 4 << QSPI_CINSTRCONF_LENGTH_Pos )
      | ( QSPI_CINSTRCONF_WREN_Enable << QSPI_CINSTRCONF_WREN_Pos )
      | ( QSPI_CINSTRCONF_WIPWAIT_Enable << QSPI_CINSTRCONF_WIPWAIT_Pos )
      | ( 0 << QSPI_CINSTRCONF_LIO2_Pos )
      | ( 1 << QSPI_CINSTRCONF_LIO3_Pos );

    while ( nrf_qspi.EVENTS_READY == 0 )
        ;
}
