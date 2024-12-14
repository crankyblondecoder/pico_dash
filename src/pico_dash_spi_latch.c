#include <stdio.h>

#include "time.h"

#include "pico_dash_gpio.h"
#include "pico_dash_spi_latch.h"
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

/**
 * Whether the SPI master is currently actively procesing a latch command.
 * The master only asserts command active for a single command frame after which
 * it must de-assert and re-assert to start a new command.
 */
bool spiLatchCommandActive = true;

/**
 * Set whether this Pico is ready for a latch command.
 */
void __not_in_flash_func(setReadyForCommand)(bool ready)
{
	gpio_put(SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN, ready);
}

/**
 * Read commands from SPI and write responses.
 * @note Only one command/response "frame" is processed at a time.
 */
void __not_in_flash_func(processSpiCommandResponse)()
{
	// Stay in processing loop while master remains active.
	// Only process one command at a time.

	while(spiLatchCommandActive)
	{
		// Read a command from the SPI rx fifo. Assume rx data is padded with 0's while master is waiting for a reply
		// to the command.

		// Clear input buffer.
		inputBufferPosn = 0;

		// Clear the output buffer.
		outputBufferReadPosn = 0;
		outputBufferWritePosn = 0;

		bool commandComplete = false;

		while(!commandComplete && spiLatchCommandActive)
		{
			if(spi0_hw -> sr & SPI_SSPSR_RNE_BITS)
			{
				// Get rx fifo data from dr register.
				uint32_t dr = spi0_hw -> dr;

				// Assume input buffer is incomplete for a command at the beginning of the loop.

				// While looking for command code, zero is considered "filler" data.
				// First position in input buffer is always the command code.
				if(inputBufferPosn > 0 || dr > 0)
				{
					inputBuffer[inputBufferPosn++] = dr;

					switch(inputBuffer[0])
					{
						case GET_LATCHED_DATA_INDEX:

							if(inputBufferPosn == 5)
							{
								// Get latch data index command is complete.
								if(debugMsgActive) printf("Proc GET_LATCHED_DATA_INDEX\n");

								// Two bytes have to be output.

								// Null terminate the input string.
								inputBuffer[5] = 0;

								// Request id.
								outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

								// Return latched data index.
								outputBuffer[outputBufferWritePosn++] = getLatchedDataIndex(inputBuffer + 2);

								commandComplete = true;
							}
							break;

						case GET_LATCHED_DATA_RESOLUTION:

							if(inputBufferPosn == 3)
							{
								// Command is complete.
								if(debugMsgActive) printf("Proc GET_LATCHED_DATA_RESOLUTION\n");

								// Three bytes have to be output.

								int latchedDataIndex = inputBuffer[2];
								int latchedDataResolution = getLatchedDataResolution(latchedDataIndex);

								// Request id.
								outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

								// Latched data. Little endian byte order.
								outputBuffer[outputBufferWritePosn++] = latchedDataResolution & 0xFF;
								outputBuffer[outputBufferWritePosn++] = (latchedDataResolution >> 8) & 0xFF;

								commandComplete = true;
							}
							break;

						case GET_LATCHED_DATA:

							if(inputBufferPosn == 3)
							{
								// Command is complete.
								if(debugMsgActive) printf("Proc GET_LATCHED_DATA\n");

								// Five bytes have to be output.

								int latchedDataIndex = inputBuffer[2];
								int latchedDataVal = latchedData[latchedDataIndex];

								// Request id.
								outputBuffer[outputBufferWritePosn++] = inputBuffer[1];

								// Latched data. Little endian byte order.
								outputBuffer[outputBufferWritePosn++] = latchedDataVal & 0xFF;
								outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 8) & 0xFF;
								outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 16) & 0xFF;
								outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 24) & 0xFF;

								commandComplete = true;
							}
							break;

						default:

							if(inputBuffer[0] != 0)
							{
								// Bad command.
								outputBuffer[0] = 0xFF;

								commandComplete = true;

								if(debugMsgActive) ("Unknown SPI command 0x%X", inputBuffer[0]);
							}
							else
							{
								// No command. Just reset input buffer and try reading more data.
								inputBufferPosn = 0;
							}
					}
				}
			}
		}

		setReadyForCommand(!commandComplete);

		if(commandComplete)
		{
			// Write output buffer to SPI tx fifo.

			// Disable transmit until output buffer is written but wait until transmit buffer is empty before doing that.
			// This is so that the command replay is contiguous. A try counter is used so that this loop doesn't cause a lockup.

			int tryCounter = 1024;

			while(!(spi0_hw -> sr & SPI_SSPSR_TFE_BITS) && spiLatchCommandActive && tryCounter)
			{
				tryCounter--;
			}

			if(spiLatchCommandActive && tryCounter)
			{
				// Unset CR1 SSE bit to disable SPI.
				// SPI_SSPCR1_SSE_BITS is a bit mask.

				spi0_hw -> cr1 &= ~SPI_SSPCR1_SSE_BITS;

				while(outputBufferReadPosn < outputBufferWritePosn)
				{
					spi0_hw -> dr = outputBuffer[outputBufferReadPosn++];
				}

				// Re-enable SPI.
				spi0_hw -> cr1 |= SPI_SSPCR1_SSE_BITS;
			}

			// Clear the rx fifo from the command output being sent.
			// Note: The master will have to delay for long enough while this is happening so that new commands don't get discarded.
			while(spi0_hw -> sr & SPI_SSPSR_RNE_BITS)
			{
				// Get rx fifo data from dr register.
				uint32_t dr = spi0_hw -> dr;
			}
		}

		setReadyForCommand(true);
	}
}

void __not_in_flash_func(spiGpioIrqCallback)(uint gpio, uint32_t event_mask)
{
	// This should just trigger the processing, not actually do it during the interupt.

	if(event_mask & GPIO_IRQ_EDGE_RISE)
	{
		spiLatchCommandActive = true;
		gpio_put(SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN, true);

		if(debugMsgActive) printf("SPI master is active.\n");

	} else if(event_mask & GPIO_IRQ_EDGE_FALL)
	{
		spiLatchCommandActive = false;
		gpio_put(SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN, false);

		if(debugMsgActive) printf("SPI master is inactive.\n");
	}
}

void spiLatchStartSubsystem()
{
	// Note: The Pi Zero can't do anything other than 8 bit SPI transfers. So we are basically stuck with that size.

	// Setup the SPI pins for communication of latched data on SPI0.
	spi_init(spi0, SPI_BAUD);
    spi_set_slave(spi0, true);

	// Set the on the wire format.
	// Motorola SPI Format with SPO=0, SPH=1.
	spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_1, SPI_MSB_FIRST);

    gpio_set_function(SPI_TX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_RX_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_SCK_GPIO_PIN, GPIO_FUNC_SPI);
	gpio_set_function(SPI_CSN_GPIO_PIN, GPIO_FUNC_SPI);

	// Setup GPIO pin so that latch master can trigger this slave to read a command and write a response.
	gpio_init(SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN);
	gpio_set_dir(SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN, GPIO_IN);

	spiLatchCommandActive = gpio_get(SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN);

	// Setup GPIO pin for LED to indicate master active.
	gpio_init(SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN);
	gpio_set_dir(SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN, GPIO_OUT);
	gpio_put(SPI_LATCH_COMMAND_ACTIVE_LED_GPIO_PIN, spiLatchCommandActive);

	// Setup GPIO pin to indicate to SPI master that this pico is ready for a command.
	gpio_init(SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN);
	gpio_set_dir(SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN, GPIO_OUT);
	gpio_put(SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN, true);

	// Setup the gpio callback. This doesn't set the irq event though.
	setGpioIrqCallBack(SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN, spiGpioIrqCallback);

	// Enable the IRQ for the gpio pin.
	gpio_set_irq_enabled(SPI_LATCH_COMMAND_ACTIVE_GPIO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
}

void spiLatchProcessUntilIdle()
{
	while(spiLatchCommandActive)
	{
		processSpiCommandResponse();
	}
}

bool spiLatchProcReq()
{
	return spiLatchCommandActive;
}
