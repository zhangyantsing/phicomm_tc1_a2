/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MBED_SERIAL_API_H
#define MBED_SERIAL_API_H

#include "device.h"
#include "buffer.h"
#include "dma_api.h"

#if DEVICE_SERIAL

#define SERIAL_EVENT_TX_SHIFT (2)
#define SERIAL_EVENT_RX_SHIFT (8)

#define SERIAL_EVENT_TX_MASK (0x00FC)
#define SERIAL_EVENT_RX_MASK (0x3F00)

#define SERIAL_EVENT_ERROR (1 << 1)

/**
 * @defgroup SerialTXEvents Serial TX Events Macros
 *
 * @{
 */
#define SERIAL_EVENT_TX_COMPLETE (1 << (SERIAL_EVENT_TX_SHIFT + 0))
#define SERIAL_EVENT_TX_ALL      (SERIAL_EVENT_TX_COMPLETE)
/**@}*/

/**
 * @defgroup SerialRXEvents Serial RX Events Macros
 *
 * @{
 */
#define SERIAL_EVENT_RX_COMPLETE        (1 << (SERIAL_EVENT_RX_SHIFT + 0))
#define SERIAL_EVENT_RX_OVERRUN_ERROR   (1 << (SERIAL_EVENT_RX_SHIFT + 1))
#define SERIAL_EVENT_RX_FRAMING_ERROR   (1 << (SERIAL_EVENT_RX_SHIFT + 2))
#define SERIAL_EVENT_RX_PARITY_ERROR    (1 << (SERIAL_EVENT_RX_SHIFT + 3))
#define SERIAL_EVENT_RX_OVERFLOW        (1 << (SERIAL_EVENT_RX_SHIFT + 4))
#define SERIAL_EVENT_RX_CHARACTER_MATCH (1 << (SERIAL_EVENT_RX_SHIFT + 5))
#define SERIAL_EVENT_RX_ALL             (SERIAL_EVENT_RX_OVERFLOW | SERIAL_EVENT_RX_PARITY_ERROR | \
                                         SERIAL_EVENT_RX_FRAMING_ERROR | SERIAL_EVENT_RX_OVERRUN_ERROR | \
                                         SERIAL_EVENT_RX_COMPLETE | SERIAL_EVENT_RX_CHARACTER_MATCH)
/**@}*/

#define SERIAL_RESERVED_CHAR_MATCH (255)

typedef enum {
    ParityNone = 0,
    ParityOdd = 1,
    ParityEven = 2,
    ParityForced1 = 3,
    ParityForced0 = 4
} SerialParity;

typedef enum {
    RxIrq,
    TxIrq
} SerialIrq;

typedef enum {
    FlowControlNone,
    FlowControlRTS,
    FlowControlCTS,
    FlowControlRTSCTS
} FlowControl;

typedef void (*uart_irq_handler)(uint32_t id, SerialIrq event);

#if DEVICE_SERIAL_ASYNCH
/** Asynch serial HAL structure
 */
typedef struct {
    struct serial_s serial;  /**< Target specific serial structure */
    struct buffer_s tx_buff; /**< TX buffer */
    struct buffer_s rx_buff; /**< RX buffer */
    uint8_t char_match;      /**< Character to be matched */
    uint8_t char_found;      /**< State of the matched character */
} serial_t;

#else
/** Non-asynch serial HAL structure
 */
typedef struct serial_s serial_t;

#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup hal_GeneralSerial Serial Configuration Functions
 * @{
 */

/** Initialize the serial peripheral. It sets the default parameters for serial
 *  peripheral, and configures its specifieds pins.
 *
 * @param obj The serial object
 * @param tx  The TX pin name
 * @param rx  The RX pin name
 */
void serial_init(serial_t *obj, PinName tx, PinName rx);

/** Release the serial peripheral, not currently invoked. It requires further
 *  resource management.
 *
 * @param obj The serial object
 */
void serial_free(serial_t *obj);

/** Configure the baud rate
 *
 * @param obj      The serial object
 * @param baudrate The baud rate to be configured
 */
void serial_baud(serial_t *obj, int baudrate);

/** Configure the format. Set the number of bits, parity and the number of stop bits
 *
 * @param obj       The serial object
 * @param data_bits The number of data bits
 * @param parity    The parity
 * @param stop_bits The number of stop bits
 */
void serial_format(serial_t *obj, int data_bits, SerialParity parity, int stop_bits);

/** The serial interrupt handler registration
 *
 * @param obj     The serial object
 * @param handler The interrupt handler which will be invoked when the interrupt fires
 * @param id      The SerialBase object
 */
void serial_irq_handler(serial_t *obj, uart_irq_handler handler, uint32_t id);

/** Configure serial interrupt. This function is used for word-approach
 *
 * @param obj    The serial object
 * @param irq    The serial IRQ type (RX or TX)
 * @param enable Set to non-zero to enable events, or zero to disable them
 */
void serial_irq_set(serial_t *obj, SerialIrq irq, uint32_t enable);

/** Get character. This is a blocking call, waiting for a character
 *
 * @param obj The serial object
 */
int  serial_getc(serial_t *obj);

/** Send a character. This is a blocking call, waiting for a peripheral to be available
 *  for writing
 *
 * @param obj The serial object
 * @param c   The character to be sent
 */
void serial_putc(serial_t *obj, int c);

#ifdef _HSF_
/** Check TX FIFO is empty.
 *
 * @param obj The serial object
 * @return Non-zero value if TX FIFO is empty
 */
int serial_txfifo_empty(serial_t *obj) ;
#endif

/** Check if the serial peripheral is readable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be read, 0 if nothing to read
 */
int  serial_readable(serial_t *obj);

/** Check if the serial peripheral is writable
 *
 * @param obj The serial object
 * @return Non-zero value if a character can be written, 0 otherwise.
 */
int  serial_writable(serial_t *obj);

/** Clear the serial peripheral
 *
 * @param obj The serial object
 */
void serial_clear(serial_t *obj);

/** Set the break
 *
 * @param obj The serial object
 */
void serial_break_set(serial_t *obj);

/** Clear the break
 *
 * @param obj The serial object
 */
void serial_break_clear(serial_t *obj);

/** Configure the TX pin for UART function.
 *
 * @param tx The pin name used for TX
 */
void serial_pinout_tx(PinName tx);

/** Configure the serial for the flow control. It sets flow control in the hardware
 *  if a serial peripheral supports it, otherwise software emulation is used.
 *
 * @param obj    The serial object
 * @param type   The type of the flow control. Look at the available FlowControl types.
 * @param rxflow The TX pin name
 * @param txflow The RX pin name
 */
void serial_set_flow_control(serial_t *obj, FlowControl type, PinName rxflow, PinName txflow);

#if DEVICE_SERIAL_ASYNCH

/**@}*/

/**
 * \defgroup hal_AsynchSerial Asynchronous Serial Hardware Abstraction Layer
 * @{
 */

/** Begin asynchronous TX transfer. The used buffer is specified in the serial object,
 *  tx_buff
 *
 * @param obj       The serial object
 * @param tx        The transmit buffer
 * @param tx_length The number of bytes to transmit
 * @param tx_width  Deprecated argument
 * @param handler   The serial handler
 * @param event     The logical OR of events to be registered
 * @param hint      A suggestion for how to use DMA with this transfer
 * @return Returns number of data transfered, otherwise returns 0
 */
int serial_tx_asynch(serial_t *obj, const void *tx, size_t tx_length, uint8_t tx_width, uint32_t handler, uint32_t event, DMAUsage hint);

/** Begin asynchronous RX transfer (enable interrupt for data collecting)
 *  The used buffer is specified in the serial object - rx_buff
 *
 * @param obj        The serial object
 * @param rx         The receive buffer
 * @param rx_length  The number of bytes to receive
 * @param rx_width   Deprecated argument
 * @param handler    The serial handler
 * @param event      The logical OR of events to be registered
 * @param handler    The serial handler
 * @param char_match A character in range 0-254 to be matched
 * @param hint       A suggestion for how to use DMA with this transfer
 */
void serial_rx_asynch(serial_t *obj, void *rx, size_t rx_length, uint8_t rx_width, uint32_t handler, uint32_t event, uint8_t char_match, DMAUsage hint);

/** Attempts to determine if the serial peripheral is already in use for TX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_tx_active(serial_t *obj);

/** Attempts to determine if the serial peripheral is already in use for RX
 *
 * @param obj The serial object
 * @return Non-zero if the RX transaction is ongoing, 0 otherwise
 */
uint8_t serial_rx_active(serial_t *obj);

/** The asynchronous TX and RX handler.
 *
 * @param obj The serial object
 * @return Returns event flags if an RX transfer termination condition was met; otherwise returns 0
 */
int serial_irq_handler_asynch(serial_t *obj);

/** Abort the ongoing TX transaction. It disables the enabled interupt for TX and
 *  flushes the TX hardware buffer if TX FIFO is used
 *
 * @param obj The serial object
 */
void serial_tx_abort_asynch(serial_t *obj);

/** Abort the ongoing RX transaction. It disables the enabled interrupt for RX and
 *  flushes the RX hardware buffer if RX FIFO is used
 *
 * @param obj The serial object
 */
void serial_rx_abort_asynch(serial_t *obj);

/**@}*/

#endif

#ifdef __cplusplus
}
#endif

#endif

#endif
