#include "time.h"

#include "pico_dash_gpio.h"
#include "pico_dash_spi.h"
#include "pico_dash_latch.h"

#define MAX_OUTPUT_BUFFER_SIZE 128

extern int latchedData[];

/** Input buffer to read into. */
uint8_t inputBuffer[6];

/** Current position to read into. */
int inputBufferPosn = 0;

/** Output buffer to write out. */
uint8_t outputBuffer[MAX_OUTPUT_BUFFER_SIZE];

/** Position to read next output value from. */
int outputBufferReadPosn = 0;

/** Position to write next output value from. */
int outputBufferWritePosn = 0;

/** Whether the SPI master is currently idle. */
bool spiMasterIdle = true;

/**
 * Read command from SPI and write response.
 */
void __not_in_flash_func(processSpiCommandResponse)()
{
	// Stay in processing loop while master remains active and there are commands to process or output to write.
	while(!spiMasterIdle && ((outputBufferWritePosn > outputBufferReadPosn) || spi0_hw -> sr & SPI_SSPSR_RNE_BITS))
	{
		while(spi0_hw -> sr & SPI_SSPSR_RNE_BITS)
		{
			// *** Read Commands ***

			// NOTE: Output buffer overflow is discarded.

			// Assume input buffer is incomplete for a command at the beginning of the loop.
			inputBuffer[inputBufferPosn++] = spi0_hw -> dr;

			switch(inputBuffer[0])
			{
				case GET_LATCHED_DATA_INDEX:

					if(inputBufferPosn == 5)
					{
						// Get latch data index command is complete.

						// Two bytes have to be output.
						if(outputBufferWritePosn + 1 < MAX_OUTPUT_BUFFER_SIZE)
						{
							// Null terminate the input string.
							inputBuffer[5] = 0;

							// Request id.
							outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

							// Return latched data index.
							outputBuffer[outputBufferWritePosn++] = getLatchedDataIndex(inputBuffer + 2);
						}

						// Clear input buffer.
						inputBufferPosn = 0;
					}
					break;

				case GET_LATCHED_DATA:

					if(inputBufferPosn == 3)
					{
						// Command is complete.

						// Five bytes have to be output.
						if(outputBufferWritePosn + 4 < MAX_OUTPUT_BUFFER_SIZE)
						{
							int latchedDataIndex = inputBuffer[2];
							int latchedDataVal = latchedData[latchedDataIndex];

							// Request id.
							outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

							// Latched data. Little endian byte order.
							outputBuffer[outputBufferWritePosn++] = latchedDataVal & 0xFF;
							outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 8) & 0xFF;
							outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 16) & 0xFF;
							outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 24) & 0xFF;
						}

						// Clear input buffer.
						inputBufferPosn = 0;
					}
					break;

				default:

					// Bad command.
					inputBufferPosn = 0;
			}
		}

		// *** Write output buffer. ***

		// TODO ...
	}
}

void __not_in_flash_func(spiGpioIrqCallback)(uint gpio, uint32_t event_mask)
{
	// This should just trigger the processing, not actually do it during the interupt.

	if(event_mask & GPIO_IRQ_EDGE_RISE)
	{
		spiMasterIdle = false;
	}

	if(event_mask & GPIO_IRQ_EDGE_FALL)
	{
		spiMasterIdle = true;
	}
}

void spiStartSubsystem()
{
	// Note: The Pi Zero can't do anything other than 8 bit SPI transfers. So we are basically stuck with that size.

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

bool __not_in_flash_func(spiMasterIsIdle)()
{
	return spiMasterIdle;
}

void spiProcessUntilIdle()
{
	while(!spiMasterIdle)
	{
		processSpiCommandResponse();
	}

	// Clear the input buffer.
	inputBufferPosn = 0;

	// Clear the output buffer.
	outputBufferPosn = 0;
	outputBufferCount = 0;
}