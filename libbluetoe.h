/*
 Name:		libbluetoe.h
 Created:	6/18/2018 3:24:17 PM
 Author:	Tony
 Editor:	http://www.visualmicro.com
*/

#ifndef _libbluetoe_h
#define _libbluetoe_h

#pragma pack(1)

#undef F
#include "bluetoe/server.hpp"
//#include "bluetoe/services/dis.hpp"
#include "hcidefs.h"
#include "hcimsgs.h"



struct {
	uint8_t packet_type = HCIT_TYPE_COMMAND;
	uint16_t opcode = HCI_RESET;
	uint8_t len = 0;
} hci_reset_cmd;

struct {
	uint8_t packet_type = HCIT_TYPE_COMMAND;
	uint16_t opcode = HCI_BLE_WRITE_ADV_PARAMS;
	uint8_t len = HCIC_PARAM_SIZE_BLE_WRITE_ADV_PARAMS;
	struct {
		uint16_t interval_min = 0x0100;
		uint16_t interval_max = 0x0200;
		uint8_t adv_type = 0x00;
		uint8_t own_addr_type = 0;
		uint8_t peer_addr_type = 0;
		uint8_t peer_addr[6] = {};
		uint8_t channel_map = 0x03;
		uint8_t filter_policy = 0;
	} adv_params;
} hci_ble_write_adv_params_cmd;

struct {
	uint8_t packet_type = HCIT_TYPE_COMMAND;
	uint16_t opcode = HCI_BLE_WRITE_ADV_DATA;
	uint8_t len = HCIC_PARAM_SIZE_BLE_WRITE_ADV_DATA+1;
	uint8_t adv_data_len = 0;
	uint8_t adv_data[31];
} hci_ble_write_adv_data_cmd;

struct {
	uint8_t packet_type = HCIT_TYPE_COMMAND;
	uint16_t opcode = HCI_BLE_WRITE_ADV_ENABLE;
	uint8_t len = 1;
	uint8_t en = 1;
} hci_ble_write_adv_enable_cmd;

struct DataPacketHeader {
	uint8_t packet_type = HCIT_TYPE_ACL_DATA;
	struct {
		uint16_t handle : 12;
		uint16_t pb : 2;
		uint16_t bc : 2;
	};
	uint16_t len;
	uint16_t pdu_len;
	uint16_t ch_id;
	uint8_t pdu;
};

#endif

