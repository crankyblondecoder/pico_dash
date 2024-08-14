#include "hardware/gpio.h"
#include "hardware/spi.h"

// SPI communication.

/** The GPIO pin to use for the SPI master to indicate read/write ready. This is _not_ part of the SPI subsystem. */
#define SPI_MASTER_CONTROL_GPIO_PIN 20

/** SPI baud rate. As a slave, not sure this even matters? */
#define SPI_BAUD 1000000

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
 */
enum SpiCommand
{
	/**
	 * Get index to use to retrieve latched data.
	 *
	 *     Incoming data payload: 3 characters (bytes) which represents the latched data index name.
	 *     Outgoing data payload: 1 byte that contains the latched data index.
	 */
	GET_LATCHED_DATA_INDEX = 0x11,

	/**
	 * Get latched data.
	 *
	 *     Incoming data payload: 1 byte that contains the latched data index to return.
	 *     Outgoing data payload: 32 bit integer (4 bytes). Byte order, little endian.
	 */
	GET_LATCHED_DATA = 0x12
};

/**
 * Start the SPI communication subsystem.
 */
void startSpiComms();