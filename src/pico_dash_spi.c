#include "pico_dash_spi.h"

uint8_t inputBuffer[4];
uint8_t outputBuffer[4];

/**
 * Setup SPI ready for send/recieve.
 */
void setupSpi()
{
	spi_init(spi0, SPI_BAUD);
    spi_set_slave(spi0, true);
    gpio_set_function(SPI_TX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_RX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SCK_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_CSN_GPIO_PIN, GPIO_FUNC_SPI);
}

void startSpiComms()
{
	// Setup the SPI pins.
	setupSpi();

	// Setup GPIO pin so that master can trigger this slave to read/write.
	gpio_init(SPI_MASTER_CONTROL_GPIO_PIN);
	gpio_set_dir(SPI_MASTER_CONTROL_GPIO_PIN, GPIO_IN);

	// TODO ... Some kind of GPIO interrupt management?
}