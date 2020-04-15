/*
 * LLHandler.cpp
 *
 *  Created on: 9 Apr 2020
 *      Author: xasin
 */

#include <FurComs/LLHandler.h>

#include <string.h>

int TRACE_com_state = 0;

namespace TEF {
namespace FurComs {

LL_Handler::LL_Handler(USART_TypeDef *handle) :
		uart_handle(handle),
		state(IDLE),
		rx_arbitration_counter(0),
		arbitration_loss_position(0),
		tx_data_head(0), tx_data_tail(0), tx_data(),
		tx_data_packet_count(0),
		tx_raw_ptr(nullptr), tx_raw_length(0),
		rx_buffer_num(0), had_received_escape(false) {
	state = IDLE;

	tx_arbitration._latency_a = 0xFF;
	tx_arbitration._latency_b = 0xFF;
	set_chip_id(0xFFF);
	set_priority(100);

	for(int i=0; i<2; i++) {
		rx_buffers[i].data_end = rx_buffers[i].raw_data.data();
		rx_buffers[i].data_start = nullptr;
	}
}

void LL_Handler::init() {
	uart_handle->CR1 |= USART_CR1_RXNEIE;
}

void LL_Handler::handle_isr() {
	if(uart_handle->ISR & USART_ISR_RXNE)
		rx_single(uart_handle->RDR);

	if(uart_handle->ISR & USART_ISR_TXE)
		tx_single();

	TRACE_com_state = state;
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
		state = IDLE;

		rx_buffers[rx_buffer_num].data_available = true;

		if(tx_data_packet_count)
			uart_handle->TDR = 0;

		break;

	case PARTICIPATING_ARBITRATION:
	case WAITING_ARBITRATION:
	case IDLE:
		rx_arbitration_counter = 0;
		arbitration_loss_position = 0;

		if(tx_data_packet_count) {
			raw_start_tx(&tx_arbitration, 4);

			state = PARTICIPATING_ARBITRATION;
		}
		else {
			state = WAITING_ARBITRATION;
		}
	break;

	case SENDING_COMPLETE:
	case SENDING:
		state = IDLE;
		if(tx_data_packet_count)
			uart_handle->TDR = 0;
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
				*reinterpret_cast<uint32_t*>(tx_arbitration.collision_map) = ~uint32_t(1<<arbitration_loss_position);

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
				state = SENDING;
				uart_handle->CR1 |= USART_CR1_TXEIE;
			}
		}

		rx_arbitration_counter++;

		break;

	case WAITING_ARBITRATION:
		if(rx_arbitration_counter++ == 7) {
			state = RECEIVING;

			rx_buffer_num ^= 1;
			rx_buffers[rx_buffer_num].data_end = rx_buffers[rx_buffer_num].raw_data.data();
			rx_buffers[rx_buffer_num].data_start = 0;
			rx_buffers[rx_buffer_num].data_available = false;
		}
		break;

	case RECEIVING: {
		rx_buffer_t &buffer = rx_buffers[rx_buffer_num];

		if(had_received_escape) {
			if(c == FURCOM_ESC_ESC)
				*(buffer.data_end++) = FURCOM_ESCAPE;
			else if(c == FURCOM_ESC_END) {
				*(buffer.data_end++) = FURCOM_END;
				// First 0x00 will mark the end of the topic,
				// and the start of the data
				if(buffer.data_start == 0) {
					buffer.data_start = buffer.data_end;
				}
			}

			had_received_escape = false;
		}
		else if(c == FURCOM_ESCAPE)
			had_received_escape = true;
		else
			*(buffer.data_end++) = c;
		break;
	}

	case SENDING_COMPLETE:
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
	else if(state == SENDING) {
		uint8_t out_c = tx_data[tx_data_tail++];
		if(tx_data_tail >= tx_data.size())
			tx_data_tail = 0;
		uart_handle->TDR = out_c;

		if((tx_data_tail == tx_data_head) || (out_c == 0)) {
			state = SENDING_COMPLETE;
			tx_data_packet_count--;
		}
	}

	if((tx_raw_length == 0) && (state != SENDING))
		uart_handle->CR1 &= ~USART_CR1_TXEIE;
}

void LL_Handler::set_chip_id(uint16_t chip_id) {
	tx_arbitration.chip_id = 0x1 | 0x100 | ((chip_id & 0xEF) << 9) | ((chip_id >> 6) & 0xEF);
}
void LL_Handler::set_priority(int8_t priority) {
	if(priority < -60)
		tx_arbitration.priority = 1;
	else if(priority > 60)
		tx_arbitration.priority = 0xFF;
	else
		tx_arbitration.priority = 1 | (priority + 64) << 1;
}

void LL_Handler::add_tx_data(const void *data_ptr, size_t length) {
	const uint8_t *cast_ptr = reinterpret_cast<const uint8_t *>(data_ptr);
	while(length) {
		if(*cast_ptr == FURCOM_END) {
			tx_data[(tx_data_head++)] = FURCOM_ESCAPE;
			tx_data_head &= 0x1FF;
			tx_data[(tx_data_head++)] = FURCOM_ESC_END;
		}
		else if(*cast_ptr == FURCOM_ESCAPE) {
			tx_data[(tx_data_head++)] = FURCOM_ESCAPE;
			tx_data_head &= 0x1FF;
			tx_data[(tx_data_head++)] = FURCOM_ESC_ESC;
		}
		else
			tx_data[(tx_data_head++)] = *cast_ptr;

		tx_data_head &= 0x1FF;

		cast_ptr++;
		length--;
	}
}

void LL_Handler::start_tx() {
	// Add mandatory end character
	tx_data[(tx_data_head++)] = 0x00;
	tx_data_head &= 0x1FF;

	tx_data_packet_count++;

	// Only if we are idle can we start sending!!
	if(state == IDLE && (uart_handle->ISR & USART_ISR_IDLE))
		uart_handle->TDR = 0;
}

} /* namespace FurComs */
} /* namespace TEF */
