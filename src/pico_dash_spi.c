#include "pico_dash_gpio.h"
#include "pico_dash_spi.h"

uint8_t inputBuffer[4];
uint8_t outputBuffer[4];

/**
 * Read command from SPI and write response.
 */
void __not_in_flash_func(processSpiCommandResponse)()
{
	// Example of direct to SPI register.
	spi0_hw -> dr;

	// TODO ... If the tx fifo is full, use a local buffer.
}

/**
 * Reset SPI back to state ready for next command.
 */
void __not_in_flash_func(resestSpiReadyForCommand)()
{

}

void __not_in_flash_func(spiGpioIrqCallback)(uint gpio, uint32_t event_mask)
{
	// TODO ... This should just trigger the processing, not actually do it during the interupt

	if(event_mask & GPIO_IRQ_EDGE_RISE)
	{
		// Rising edge indicates request command has been sent by master.
		processSpiCommandResponse();
	}

	if(event_mask & GPIO_IRQ_EDGE_FALL)
	{
		// Falling edge indicates transmited data from previous command has been read by master.
		resestSpiReadyForCommand();
	}
}

void startSpiSubsystem()
{
	// Setup the SPI pins.
	spi_init(spi0, SPI_BAUD);
    spi_set_slave(spi0, true);
    gpio_set_function(SPI_TX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_RX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SCK_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_CSN_GPIO_PIN, GPIO_FUNC_SPI);

	// Setup GPIO pin so that master can trigger this slave to read/write.
	gpio_init(SPI_MASTER_CONTROL_GPIO_PIN);
	gpio_set_dir(SPI_MASTER_CONTROL_GPIO_PIN, GPIO_IN);

	// Setup the gpio callback. This doesn't set the irq event though.
	setGpioIrqCallBack(SPI_MASTER_CONTROL_GPIO_PIN, spiGpioIrqCallback);

	// Enable the IRQ for the gpio pin.
	gpio_set_irq_enabled(SPI_MASTER_CONTROL_GPIO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}