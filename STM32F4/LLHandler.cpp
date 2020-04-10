/*
 * LLHandler.cpp
 *
 *  Created on: 9 Apr 2020
 *      Author: xasin
 */

#include <FurComs/LLHandler.h>

#include <string.h>

namespace TEF {
namespace FurComs {

LL_Handler::LL_Handler(USART_TypeDef *handle) : uart_handle(handle) {
	state = IDLE;

	tx_arbitration._latency_a = 0xFF;
	tx_arbitration._latency_b = 0xFF;
	set_chip_id(0xFFF);
	set_priority(100);

	rx_arbitration_counter = 0;
	arbitration_loss_position = 0;

	tx_data_length = 0;
	tx_data_rx_compare = 0;

	tx_data_ready = false;

	tx_raw_ptr = nullptr;
	tx_raw_length = 0;

	rx_data_pos = 0;
	had_received_escape = false;
}

void LL_Handler::init() {
	uart_handle->CR1 |= USART_CR1_RXNEIE;
}

void LL_Handler::handle_isr() {
	if(uart_handle->ISR & USART_ISR_RXNE)
		rx_single(uart_handle->RDR);

	if(uart_handle->ISR & USART_ISR_TXE)
		tx_single();
}

int LL_Handler::get_missmatch_pos(uint8_t a, uint8_t b) {
	for(int i=8; i > 0; i--) {
		if((0x80 & a) != (0x80 & b))
			return i;
		a <<= 1;
		b <<= 1;
	}

	return 0;
}

void LL_Handler::raw_start_tx(const void *data_ptr, size_t length) {
	tx_raw_ptr = reinterpret_cast<const uint8_t*>(data_ptr);
	tx_raw_length = length;

	uart_handle->CR1 |= USART_CR1_TXEIE;
}

void LL_Handler::handle_stop_char() {
	switch(state) {
	case RECEIVING:
		// TODO Hand out the received data.
	case PARTICIPATING_ARBITRATION:
	case WAITING_ARBITRATION:
	case IDLE:
		rx_arbitration_counter = 0;
		arbitration_loss_position = 0;
		tx_data_rx_compare = 0;

		if(tx_data_ready) {
			raw_start_tx(&tx_arbitration, 4);

			state = PARTICIPATING_ARBITRATION;
		}
		else {
			state = WAITING_ARBITRATION;
		}
	break;

	case SENDING:
		state = IDLE;
		tx_data_ready = 0;
		tx_data_length = 0;
	break;
	}
}

void LL_Handler::rx_single(uint8_t c) {
	if(c == 0x00) {
		handle_stop_char();
		return;
	}

	switch(state) {
	case IDLE: break;
	case PARTICIPATING_ARBITRATION:
		// Perform actual arbitration
		if(rx_arbitration_counter < 3) {
			uint8_t *d_ptr = &(tx_arbitration.priority);

			if(d_ptr[rx_arbitration_counter] != c && arbitration_loss_position == 0) {
				arbitration_loss_position = get_missmatch_pos(d_ptr[rx_arbitration_counter], c);
				arbitration_loss_position += 8*(2 - rx_arbitration_counter);
			}

			if(rx_arbitration_counter == 2) {
				memset(tx_arbitration.collision_map, 0xFF, 3);
				*reinterpret_cast<uint32_t*>(tx_arbitration.collision_map) &= ~(1<<arbitration_loss_position);

				raw_start_tx(tx_arbitration.collision_map, 4);
			}
		}
		else if(rx_arbitration_counter < 4) {
		}
		else if(rx_arbitration_counter < 7) {
			uint32_t arb_c = uint32_t(~c) << (8*(rx_arbitration_counter-4));
			uint32_t arb_map = 0xFFFFFF >> (24 - arbitration_loss_position);

			if(arb_c & arb_map) {
				state = WAITING_ARBITRATION;
			}
			else if(rx_arbitration_counter == 6) {
				raw_start_tx(tx_data.data(), tx_data_length);
				state = SENDING;
			}
		}

		rx_arbitration_counter++;

		break;

	case WAITING_ARBITRATION:
		if(rx_arbitration_counter++ == 7) {
			state = RECEIVING;
		}
		break;

	case RECEIVING:
		if(had_received_escape) {
			if(c == FURCOM_ESC_ESC)
				rx_data[rx_data_pos++] = FURCOM_ESCAPE;
			else if(c == FURCOM_ESC_END)
				rx_data[rx_data_pos++] = FURCOM_END;

			had_received_escape = false;
		}
		else if(c == FURCOM_ESCAPE)
			had_received_escape = true;
		else
			rx_data[rx_data_pos++] = c;
		break;

	case SENDING:
		// TODO Figure out if we actually want to stop
		// on a missmatch or just ignore it.
		break;
	}
}

void LL_Handler::tx_single() {
	if(tx_raw_length) {
		uart_handle->TDR = *tx_raw_ptr;
		tx_raw_ptr++;
		tx_raw_length--;
	}

	if(tx_raw_length == 0)
		uart_handle->CR1 &= ~USART_CR1_TXEIE;
}

void LL_Handler::set_chip_id(uint16_t chip_id) {
	tx_arbitration.chip_id = 1 | (chip_id & 0x7F ) << 9
							   | 0x100 | (chip_id & 0x3F80) >> 6;
}
void LL_Handler::set_priority(int8_t priority) {
	// TODO Check if the priority was set to 0x00
	tx_arbitration.priority = 127 + priority;
}

bool LL_Handler::can_accept_packet() {
	return !tx_data_ready;
}
void LL_Handler::add_tx_data(const void *data_ptr, size_t length) {
	if(tx_data_ready)
		return;

	const uint8_t *cast_ptr = reinterpret_cast<const uint8_t *>(data_ptr);
	while(length) {
		if(*cast_ptr == FURCOM_END) {
			tx_data[tx_data_length++] = FURCOM_ESCAPE;
			tx_data[tx_data_length++] = FURCOM_ESC_END;
		}
		else if(*cast_ptr == FURCOM_ESCAPE) {
			tx_data[tx_data_length++] = FURCOM_ESCAPE;
			tx_data[tx_data_length++] = FURCOM_ESC_ESC;
		}
		else
			tx_data[tx_data_length++] = *cast_ptr;

		cast_ptr++;
		length--;
	}
}

void LL_Handler::start_tx() {
	// Add mandatory end character
	tx_data[tx_data_length++] = 0x00;
	tx_data_ready = true;

	// Only if we are idle can we start sending!!
	if(state == IDLE)
		uart_handle->TDR = 0;
}

} /* namespace FurComs */
} /* namespace TEF */
