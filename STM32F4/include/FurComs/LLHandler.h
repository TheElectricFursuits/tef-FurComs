/*
 * LLHandler.h
 *
 *  Created on: 9 Apr 2020
 *      Author: xasin
 */

#ifndef FURCOMS_LLHANDLER_H_
#define FURCOMS_LLHANDLER_H_

#include "main.h"

#include <stdint.h>
#include <array>

namespace TEF {
namespace FurComs {

enum handler_state_t {
	IDLE, //!< Bus is idle, can start sending any time
	PARTICIPATING_ARBITRATION, //!< Handler is participating in arbitration right now
	WAITING_ARBITRATION, //!< Handler is receiving, waiting for the arbitration to finish
	RECEIVING, //!< Handler is now receiving
	SENDING,   //!< Handler is now sending
};

enum fur_coms_chars_t {
	FURCOM_END = 0x00,
	FURCOM_ESCAPE = 0xDB,
	FURCOM_ESC_END = 0xDC,
	FURCOM_ESC_ESC = 0xDD
};

#pragma pack(1)
struct arbitration_package_t {
	uint8_t  priority;
	uint16_t chip_id;
	uint8_t  _latency_a;
	uint8_t  collision_map[3];
	uint8_t  _latency_b;
};
#pragma pack(0)

class LL_Handler {
private:
	USART_TypeDef *uart_handle;

	handler_state_t state;

	arbitration_package_t tx_arbitration;
	int rx_arbitration_counter;
	uint8_t arbitration_loss_position;

	int tx_data_length;
	int tx_data_rx_compare;
	//! Pre-encoded data to be sent onto the bus
	std::array<uint8_t, 512> tx_data;
	bool tx_data_ready;

	const uint8_t *tx_raw_ptr;
	size_t tx_raw_length;

	//! Pre-decoded data received from the bus
	int rx_data_pos;
	std::array<uint8_t, 512> rx_data;
	bool had_received_escape;

	int get_missmatch_pos(uint8_t a, uint8_t b);

	void raw_start_tx(const void *data, size_t length);

	void handle_stop_char();
	void rx_single(uint8_t c);
	void tx_single();

public:
	LL_Handler(USART_TypeDef *uart_handle);

	void init();

	void handle_isr();

	void set_chip_id(uint16_t id);
	void set_priority(int8_t prio);

	bool can_accept_packet();
	void add_tx_data(const void *data_ptr, size_t length);
	void start_tx();
};

} /* namespace FurComs */
} /* namespace TEF */

#endif /* FURCOMS_LLHANDLER_H_ */