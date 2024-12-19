#ifndef PICO_DASH_SPI_H
#define PICO_DASH_SPI_H

#include "hardware/gpio.h"
#include "hardware/spi.h"

// SPI communication.
// spi0 Is used exclusively.

/**
 * The GPIO pin to use for the SPI master for latched data to indicate a command is active and either already has been or is
 * currently being written.
 * @note This is _not_ part of the SPI subsystem.
 *
 * Edge rise: Master has completed write of a command to the SPI bus.
 * Edge fall: Master has completed reading the command result from the SPI bus. This should always result in the SPI state
 *            being reset so that the FIFO's are clear. Hopefully this will make the comms protocol reasonably resistent to
 *            SPI transfer failure.
 */
#define SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN 20

#define SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN 25

/**
 * The GPIO pin to use to indicate to the SPI master that this Pico is ready for a command.
 * This pin going high indicates this Pico is ready.
 * @note This is _not_ part of the SPI subsystem.
 */
#define SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN 21

/** The command/response frame size, in bytes. */
#define SPI_COMMAND_RESPONSE_FRAME_SIZE 8

/**
 * SPI baud rate. My understanding is that, as a slave, this specifies the maximum baud rate that master can use,
 * with the minimum being the system clock rate divided by (254 x 256).
 */
#define SPI_BAUD 8000000

/** GPIO pin for SPI TX (transmit). */
#define SPI_TX_GPIO_PIN 19
/** GPIO pin for SPI RX (recieve). */
#define SPI_RX_GPIO_PIN 16
/** GPIO pin for SPI SCK (clock). */
#define SPI_SCK_GPIO_PIN 18
/** GPIO pin for SPI CSN (chip select negative). */
#define SPI_CSN_GPIO_PIN 17


/**
 * Commands that a SPI master can issue.
 * SPI commands are single char (byte).
 * Using high numbers because they are less likely to collide with data that follows the command. This gives an element of
 * robustness to detecting the beginning of a command.
 * @note Do _not_ allow request ID's to overlap command values numerically.
 * @note If a bad command is encountered the value 0xFF is returned in the first byte.
 */
enum SpiCommand
{
	/**
	 * Get index to use to retrieve latched data.
	 *
	 *     Incoming data payload:
	 *                            1 byte command.
	 *                            1 byte that is the request id.
	 *                            3 characters (bytes) which represents the latched data index name.
	 *
	 *     Outgoing data payload:
	 *                            1 byte that is the request id supplied from the incoming data payload.
	 *                            1 byte that contains the latched data index. -1 Indicates an error condition.
	 */
	GET_LATCHED_DATA_INDEX = 0xF1,

	/**
	 * Get latched data resolution. This maps the latched data value, in integer form, to the actual decimal value.
	 * ie Latched data is transmitted as a 32bit integer and the resolution is required to convert that to a decimal value.
	 * The resolution is the number of integer steps allocated per graduation. ie The integer latched data value is converted
	 * to a double and divided by the resolution (after also being converted to a double).
 	 * For example, engine temperature might have 10 integer steps per degree celsius giving it a resolution of 0.1.
	 *
	 *     Incoming data payload:
	 *                            1 byte command.
	 *                            1 byte that is the request id.
	 *                            1 byte that contains the latched data index to get the resolution for.
	 *
	 *     Outgoing data payload:
	 *                            1 byte that is the request id supplied from the incoming data payload.
	 *                            16 bit integer (2 bytes). Byte order, little endian (ie lowest order byte first).
	 */
	GET_LATCHED_DATA_RESOLUTION = 0xF2,

	/**
	 * Get latched data.
	 *
	 *     Incoming data payload:
	 *                            1 byte command.
	 *                            1 byte that is the request id.
	 *                            1 byte that contains the latched data index to return.
	 *
	 *     Outgoing data payload:
	 *                            1 byte that is the request id supplied from the incoming data payload.
	 *                            32 bit integer (4 bytes). Byte order, little endian (ie lowest order byte first).
	 */
	GET_LATCHED_DATA = 0xF3
};

/**
 * Start the SPI latched data communication subsystem.
 * This will "bind" to the calling core and should only ever be run by one core.
 */
void spiLatchStartSubsystem();

/**
 * Do any required SPI processing of latched data commands.
 */
void spiLatchProcess();

/**
 * SPI processing of latched data is required.
 */
bool spiLatchProcReq();

#endif