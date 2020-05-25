/*!
 * \file LLHandler.h
 * \date 23.05.2020
 * \author The System (Neira)
 * \version 1.0
 *
 * \copyright GNU Public License v3
 */

#ifndef FURCOMS_LLHANDLER_H_
#define FURCOMS_LLHANDLER_H_

#include "main.h"

#include <cmsis_os.h>

#include <stdint.h>
#include <array>

#ifndef FURCOM_RX_BUFFER_NUM
#define FURCOM_RX_BUFFER_NUM 4
#endif

//! \brief Namespace of The Electric Fursuits code
//! \see https://github.com/TheElectricFursuits/
namespace TEF {
//! \brief Namespace of FurComs related definitions
//! \see https://github.com/TheElectricFursuits/tef-FurComs
namespace FurComs {

/*! \brief Transceiver bus state.
 *  \details This enum defines the states that a bus transceiver can be in
 *    during any given moment in time, be it a idle bus, waiting for
 *    transmission of receiving data.
 *
 *    The following states are relevant for transmission only:
 *    - PARTICIPATING_ARBITRATION
 *    - SENDING
 *    - SENDING_COMPLETE
 *
 *   A listen-only node may leave these states out.
 *
 *   At any given time, if the bus has been idle for more than 5ms, any
 *   received 0x00 will count as a START and not packet STOP, and thusly trigger
 *   a new message.
 *   However, it is advised that nodes may only send a 0x00 after 10ms of
 *   bus idle time, to ensure that all nodes agree on the START condition.
 */
enum handler_state_t {
	IDLE, //!< Bus is idle, sending is permitted at any time.
	PARTICIPATING_ARBITRATION, //!< Handler is participating in arbitration right now
	WAITING_ARBITRATION, //!< Handler is receiving, waiting for the arbitration to finish
	RECEIVING, //!< Handler is now receiving
	SENDING,   //!< Handler is now sending
	SENDING_COMPLETE, //!< Handler completed, waiting to receive final 0x00
};

/*! \brief Special FURCOMS characters.
 *  \details This enum defines the special characters used in Furcoms encoding.
 *   Encoding is performed in the same manner as for SLIP encoding (RFC 1055),
 *   however with the END character modified to be 0x00. This ensures that
 *   END is a dominant character on a CAN-Bus line, and that it can thusly
 *   overwrite messages.
 * \see https://tools.ietf.org/html/rfc1055
 */
enum fur_coms_chars_t {
	FURCOM_END = 0x00,
	FURCOM_ESCAPE = 0xDB,
	FURCOM_ESC_END = 0xDC,
	FURCOM_ESC_ESC = 0xDD
};

/*! \brief Struct defining the arbitration phase data.
 *	\details This struct defines the data used during the arbitration phase.
 *   It shall be used as follows:
 *   - After receiving a starting 0x00, all nodes that wish to transmit will
 *     fill the 'priority' and 'chip_id' fields.
 *     Default priority will be 64, chip_id is user-settable. See the in-struct
 *     documentation for how to fill these fields.
 *   - The _latency_a and _latency_b fields will be filled with 0xFF
 *   - All sending nodes may then participate in the arbitration by sending
 *     the first 4 bytes (up to, including _latency_a) onto the bus.
 *   - All participating nodes MUST be listening to the bus line, and MUST
 *     record the first bit missmatch of bus-data. Earlier packets are lower-value,
 *     MSB is lower-value than LSB, thusly priority MSB holds bit missmatch position 24
 *     and chip_id LSB holds missmatch position 1. Note that receiving NO
 *     missmatch must also be recorded as missmatch position 0.
 *   - Any node with a bit-missmatch in priority MSB (position 24) MUST stop
 *     transmitting and MAY enter receive mode.
 *   - All other nodes MUST fill collision_map with ~(1<< missmatch),
 *     and must continue sending their arbitration package up to and including
 *     _latency_b.
 *   - All nodes MUST receive the collision_map, and must compute the lowest
 *     missmatch of all nodes. Only the node with the lowest missmatch position
 *     (this being 0 if no collission was detected) MAY continue transmission,
 *     all other nodes MUST stop and switch to receiving mode!
 *   - Only the now successful node MAY continue transmitting its data, encoded
 *     in modified SLIP encoding. It MUST send a 0x00 to signal end of packet,
 *     and MUST also send a second 0x00 to indicate start of arbitration should
 *     it wish to transmit again.
 */
#pragma pack(1)
struct arbitration_package_t {
	uint8_t  priority;	/*!< Priority of this package.
								     Computed as: (priority + 64) << 1 | 1 */
	uint16_t chip_id;		/*!< Chip ID.
									  Computed as:
									  0x1 | 0x100 | ((chip_id & 0xEF) << 9) | ((chip_id >> 6) & 0xEF); */
	uint8_t  _latency_a;	//!< Latency byte. Must always be 0xFF!
	uint8_t  collision_map[3]; /*!< Collission map.
											  Contains a bitmap of detected collissions of
											  arbitration participating nodes. See struct explanation. */
	uint8_t  _latency_b;	//!< Latency byte. Must always be 0xFF.
};
#pragma pack(0)

/*! \brief FurComs RX Buffer.
 *  \details A buffer for exactly one received packet. Packet length is
 *    limited to 256 bytes to ease storing. Each packet is stored in its own
 *    continuous buffer, ensuring easy handling at the cost of slight memory
 *    inefficiency.
 *
 *  \todo Only buffer two packets, use a FreeRTOS Queue and the FreeRTOS
 *    timer task to shuffle data into it.
 */
struct rx_buffer_t {
	std::array<char, 256> raw_data; //!< Data of the packet.
	char * data_end;  //!< Pointer to the end of data.

	bool data_available; /*!< Indicates available data. FurComs ISR will set to true,
									  application must set to false. */
};

/*! \brief STM32F4 FurComs handler.
 *  \details This class handles sending and receiving for a FurComs version 1
 *   bus, and was made for STM32F4 hardware. It handles the ISR for receiving
 *   and sending data, handling arbitration, as well as higher-level tasks such
 *   as decoding and encoding the payload with SLIP and handling receive
 *   callbacks on received messages.
 *
 *   \pre The user must provide adequate hardware for sending and transmitting
 *     data onto a FurComs bus, this being a CAN-Compliant transceiver IC. No
 *     other means of connecting to a bus (i.e. RS485) are supported!
 *   \pre The user must also pre-configure the UART module in question using
 *     standard STM32 HAL API. It shall be configured to the appropriate
 *     baudrate (usually 250000 or 115200 (if AVR chips are on the bus)), with
 *     no parity, 1 stop bit, MSB first transmission.
 *   \pre The user must also call the instance's handle_isr() within the
 *     appropriate UARTx_Handler() ISR
 *   \pre The user must call init() before sending or receiving messages.
 *     This will create and start the FurComs FreeRTOS thread. Received messages
 *     can then be received by providing a callback function to on_rx.
 *
 *   \todo Support static task allocation, or alternatively FreeRTOS Timer
 *     usage, to reduce the number of active FreeRTOS tasks.
 *   \todo Add support for other convenient sending functions, such as a printf.
 *
 * \date 23.05.2020
 * \author The System (Neira)
 * \version 1.0
 *
 * \copyright GNU Public License v3
 */
class LL_Handler {
private:
	USART_TypeDef *uart_handle;

	handler_state_t state;

	arbitration_package_t tx_arbitration;
	int rx_arbitration_counter;
	uint8_t arbitration_loss_position;

	/*! Counters of internal TX data.
	 *  \todo Add a way to re-send a failed TX packet, by providing two
	 *   tx_data_tail pointers, one of which will only be incremented after
	 *   successful transmission.
	 */
	int tx_data_head;
	int tx_data_tail;
	//! Pre-encoded data to be sent onto the bus
	std::array<uint8_t, 512> tx_data;
	//! Count of currently pending FurComs packets
	int tx_data_packet_count;

	//! Raw data pointer used to transmit the tx_arbitration struct.
	//! \todo Remove this and simply implement it as in-software data loading.
	const uint8_t *tx_raw_ptr;
	size_t tx_raw_length;

	int rx_buffer_num;
	//! Pre-decoded data received from the bus
	rx_buffer_t rx_buffers[FURCOM_RX_BUFFER_NUM];

	//! Last FreeRTOS tick that a message was received on.
	//! \todo This still assumes 1kHz ticks, compare to FreeRTOS Tickrate.
	//! \todo Remove the FreeRTOS Tick need or use more direct access, tick reading incurs slow priority checking!
	uint32_t    last_active_tick;

	bool had_received_escape;

	/*! \brief Packet writing mutex.
	 *  \details This mutex is used to lock packet access, to prevent multiple FreeRTOS
	 *    threads from mangling data. It is automatically locked in a call to start_packet(),
	 *    and will be unlocked after a call to close_packet().
	 */
	osMutexId_t  write_mutex;
	/*! Pointer to the receiver thread.
	 * \todo Replace this with the FreeRTOS Timer task to reduce stack usage.
	 */
	osThreadId_t handler_thread;

	int get_missmatch_pos(uint8_t a, uint8_t b);

	void raw_start_tx(const void *data, size_t length);

	void handle_stop_char();
	void rx_single(uint8_t c);
	void tx_single();

public:
	/*! \private
	 *  Internal function, do not call!
	 *  Necessary to provide FreeRTOS with a function to call as task.
	 *  Will treat payload as LL_Handler instance and call _run_thread()
	 */
	static void run_handler_thread(void *args);

	/*! \brief Construct a new FurComs handler.
	 *  \details This will construct a new FurComs handler. It will not
	 *    immediately be initialised, the init() function MUST be called before
	 *    transfers can be performed!
	 *    Additionally, the USART Module must have been configured by the user
	 *    to the speeds and settings of the bus, and the handle_isr() function
	 *    MUST be placed within the fitting USARTx_Handler() function!
	 *
	 *    Also, for proper arbitration handling during busy conditions,
	 *    a chip id and priority must be configured.
	 *
	 *  \see init()
	 *  \see handle_isr()
	 *  \see set_chip_id()
	 * @param uart_handle Pointer to the USART instance used by this handler.
	 */
	LL_Handler(USART_TypeDef *uart_handle);

	/*! \private
	 *  Internal function, do not call!
	 *  Necessary to provide FreeRTOS with a task to run, thusly has to
	 *  be public. Will not return, will block!
	 *  \todo Delegate to the FreeRTOS Timer task.
	 */
	void _run_thread();

	/*! \brief Initialize the UART Handler.
	 *  \details This function MUST be called before any transmission or reception
	 *    is possible. It configures the FreeRTOS RX task and mutexes, and sets up
	 *    the UART interrupts properly.
	 *  \see handle_isr()
	 */
	void init();

	/*! \brief Handle the UARTx_Handler() interrupt.
	 *  \details This function MUST be called from the UARTx_Handler interrupt
	 *    in order to properly receive and send data. No transmission will be
	 *    possible without it.
	 */
	void handle_isr();

	/*! \brief Set chip ID
	 *  \details This will configure the Chip ID used during arbitration phase.
	 *  Note that lower chip IDs may get access to the bus more often, so
	 *  for busy lines, a good chip ID distribution can be helpful.
	 *
	 *  \note It is recommended that all chips have a unique Chip ID to
	 *   prevent data collissions in busy bus conditions.
	 *
	 * @param id 14-bit ID used for identification of this node.
	 */
	void set_chip_id(uint16_t id);

	/*! \brief Set the priority of this chip.
	 *  \details This is the first byte sent over the bus, and thusly has
	 *   the highest weight during arbitration. Lower numbers will be preferred,
	 *   allowing reliable transmission of important messages during higher-load
	 *   conditions.
	 *
	 *  \todo Change this to a per-message priority value with priority
	 *   inheritance for transmitting (i.e. the system always assumes the highest
	 *   currently known priority).
	 *  \todo Reduce allowed priority count to 4?
	 *
	 * @param prio The priority value, allowed range is from -60 to 60
	 */
	void set_priority(int8_t prio);

	/*!\brief Returns if the bus is free at the moment.
	 * \details This function returns true if the bus is currently free.
	 *   A free bus is either in 'IDLE' state, or the last received
	 *   byte has been 10ms or longer ago, in which case the former
	 *   transmission is assumed to have aborted.
	 */
	bool is_idle();

	/*! \brief Begin writing a packet into the FurComs buffer.
	 *  \details This function MUST be called before any data may be written
	 *   into the buffer for transmission. It appends the given topic to the
	 *   buffer and adds a proper separator, and will additionally lock the
	 *   FreeRTOS Mutex to avoid concurrent buffer access.
	 *
	 *	\attention close_packet() MUST be called after start_packet() and add_packet_data()
	 *	 have been used. Ignoring this WILL cause a deadlock, as the mutex used for buffer
	 *	 access will never be released!
	 *
	 *  \param topic Topic to send this message under. Must be a valid string, null-terminated.
	 */
	void start_packet(const char *topic);

	/*! \brief Append data to the packet.
	 *  \details Append the given 'length' bytes of data from 'data_ptr' to
	 *    the internal buffer. Data will be appropriately escaped according
	 *    to the modified SLIP encoding, and can thusly be binary.
	 *    start_packet() MUST have been called before this function to
	 *    properly configure the buffer
	 *    Note that maximum packet length is 256 bytes including
	 *    topic but excluding escape characters!
	 *
	 *  \param data_ptr Pointer to the data to be copied into the buffer.
	 *  \param length Length, in bytes, of the data to be copied.
	 */
	void add_packet_data(const void *data_ptr, size_t length);
	/*! \brief Begin sending a packet.
	 *  \details This will release the transmit buffer mutex, and will
	 *   start transmission of this packet.
	 */
	void close_packet();

	/*!\brief FurComs receive callback
	 * \details This function pointer will be called for any data received
	 *   on the FurComs bus. It will be called from the context of the receiver
	 *   thread, which may be high priority and thusly may preempt user threads!
	 *   Be aware that this may necessitate Mutexes to prevent data corruption.
	 *
	 * @param topic String of the topic that data was received on. Always null-terminated.
	 * @param data Pointer to the received binary data. May not be a readable string, nor null-terminated.
	 * @param length Length of the received data, in bytes.
	 */
	void (*on_rx)(const char * topic, const void * data, size_t length);
};

} /* namespace FurComs */
} /* namespace TEF */

#endif /* FURCOMS_LLHANDLER_H_ */
