#include <stdio.h>

#include "time.h"

#include "pico_dash_gpio.h"
#include "pico_dash_spi.h"
#include "pico_dash_latch.h"

#define MAX_OUTPUT_BUFFER_SIZE 128

extern int latchedData[];

extern bool debugMsgActive;

/** Input buffer to read into. */
uint8_t inputBuffer[6];

/** Current position to read into. */
int inputBufferPosn = 0;

/** Output buffer to write out. */
uint8_t outputBuffer[MAX_OUTPUT_BUFFER_SIZE];

/** Position to read next output value from. Will be less than write position if data needs to be written. */
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
		// Flag to indicate no command was processed. Used to allow fallthrough to FIFO write.
		bool noCommand = false;

		while(spi0_hw -> sr & SPI_SSPSR_RNE_BITS && !noCommand)
		{
			// *** Read Commands from SPI rx fifo ***

			// NOTE: Output buffer overflow is discarded.

			// Get rx data from dr register.
			uint32_t dr = spi0_hw -> dr;

			// Assume input buffer is incomplete for a command at the beginning of the loop.
			inputBuffer[inputBufferPosn++] = dr;

			switch(inputBuffer[0])
			{
				case GET_LATCHED_DATA_INDEX:

					if(inputBufferPosn == 5)
					{
						// Get latch data index command is complete.
						if(debugMsgActive) printf("Proc GET_LATCHED_DATA_INDEX\n");

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

				case GET_LATCHED_DATA_RESOLUTION:

					if(inputBufferPosn == 3)
					{
						// Command is complete.
						if(debugMsgActive) printf("Proc GET_LATCHED_DATA_RESOLUTION\n");

						// Three bytes have to be output.
						if(outputBufferWritePosn + 2 < MAX_OUTPUT_BUFFER_SIZE)
						{
							int latchedDataIndex = inputBuffer[2];
							int latchedDataResolution = getLatchedDataResolution(latchedDataIndex);

							// Request id.
							outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

							// Latched data. Little endian byte order.
							outputBuffer[outputBufferWritePosn++] = latchedDataResolution & 0xFF;
							outputBuffer[outputBufferWritePosn++] = (latchedDataResolution >> 8) & 0xFF;
						}

						// Clear input buffer.
						inputBufferPosn = 0;
					}
					break;

				case GET_LATCHED_DATA:

					if(inputBufferPosn == 3)
					{
						// Command is complete.
						if(debugMsgActive) printf("Proc GET_LATCHED_DATA\n");

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

					// Bad or no command.
					inputBufferPosn = 0;
					noCommand = true;
			}
		}

		// *** Write output buffer to SPI tx fifo. ***

		// Disable transmit until output buffer is written but wait until transmit buffer is empty before doing that.
		// Assume slave select state is taken into account when checking for SPI busy via CR1 SSE bit.
		// Make sure that if master goes idle this wait is exited and the output is discarded.

		while(spi_is_busy(spi0) && !spiMasterIdle)
		{
		}

		if(!spiMasterIdle)
		{
			// Unset CR1 SSE bit to disable SPI.
			// SPI_SSPCR1_SSE_BITS is a bit mask.

			spi0_hw -> cr1 &= ~SPI_SSPCR1_SSE_BITS;

			while(outputBufferReadPosn < outputBufferWritePosn && spi0_hw -> sr & SPI_SSPSR_TNF_BITS)
			{
				spi0_hw -> dr = outputBuffer[outputBufferReadPosn++];
			}

			// Re-enable SPI.
			spi0_hw -> cr1 |= SPI_SSPCR1_SSE_BITS;
		}

		// If all data was written to tx fifo, reset output buffer so filling can start again from beginning.
		if(outputBufferReadPosn == outputBufferWritePosn)
		{
			outputBufferReadPosn = 0;
			outputBufferWritePosn = 0;
		}
	}
}

void __not_in_flash_func(spiGpioIrqCallback)(uint gpio, uint32_t event_mask)
{
	// This should just trigger the processing, not actually do it during the interupt.

	if(event_mask & GPIO_IRQ_EDGE_RISE)
	{
		spiMasterIdle = false;
		gpio_put(SPI_MASTER_CONTROL_ACTIVE_LED_GPIO_PIN, true);

		if(debugMsgActive) printf("SPI master is active.\n");

	} else if(event_mask & GPIO_IRQ_EDGE_FALL)
	{
		spiMasterIdle = true;
		gpio_put(SPI_MASTER_CONTROL_ACTIVE_LED_GPIO_PIN, false);

		if(debugMsgActive) printf("SPI master is inactive.\n");
	}
}

void spiStartSubsystem()
{
	// Note: The Pi Zero can't do anything other than 8 bit SPI transfers. So we are basically stuck with that size.

	// Setup the SPI pins.
	spi_init(spi0, SPI_BAUD);
    spi_set_slave(spi0, true);

	// Set the on the wire format.
	// Motorola SPI Format with SPO=0, SPH=1.
	spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(SPI_TX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_RX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SCK_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_CSN_GPIO_PIN, GPIO_FUNC_SPI);

	// Setup GPIO pin so that master can trigger this slave to read/write.
	gpio_init(SPI_MASTER_CONTROL_GPIO_PIN);
	gpio_set_dir(SPI_MASTER_CONTROL_GPIO_PIN, GPIO_IN);

	spiMasterIdle = !gpio_get(SPI_MASTER_CONTROL_GPIO_PIN);

	// Setup GPIO pin for LED to indicate master active.
	gpio_init(SPI_MASTER_CONTROL_ACTIVE_LED_GPIO_PIN);
	gpio_set_dir(SPI_MASTER_CONTROL_ACTIVE_LED_GPIO_PIN, GPIO_OUT);

	gpio_put(SPI_MASTER_CONTROL_ACTIVE_LED_GPIO_PIN, spiMasterIdle);

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
	outputBufferReadPosn = 0;
	outputBufferWritePosn = 0;
}

bool spiProcReq()
{
	return !spiMasterIdle;
}
