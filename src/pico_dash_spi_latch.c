#include <stdio.h>

#include "pico/time.h"

#include "pico_dash_gpio.h"
#include "pico_dash_spi_latch.h"
#include "pico_dash_latch.h"

extern int latchedData[];

extern bool debugMsgActive;

/** Input buffer to read into. */
uint8_t inputBuffer[SPI_COMMAND_RESPONSE_FRAME_SIZE];

/** Current position to read into. */
int inputBufferPosn = 0;

/** Output buffer to write out. */
uint8_t outputBuffer[SPI_COMMAND_RESPONSE_FRAME_SIZE];

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
		// Clear the rx fifo. Assume RFC (ready for command) is inactive and that causes the master to stall sending a
		// command.
		while(spi0_hw -> sr & SPI_SSPSR_RNE_BITS)
		{
			// Get rx fifo data from dr register.
			uint32_t dr = spi0_hw -> dr;
		}

		// Indicate ready for command.
		setReadyForCommand(true);

		// Read a command frame from the SPI rx fifo. Assume rx data is padded with 0's while master is waiting for a reply
		// to the command.
		inputBufferPosn = 0;

		// Used for timeout of read command.
		absolute_time_t timeoutTime = make_timeout_time_ms(1);

		bool timeout = false;

		while(spiLatchCommandActive && inputBufferPosn < SPI_COMMAND_RESPONSE_FRAME_SIZE && !timeout)
		{
			// Note: This loop could potentially deadlock with the master if it thinks it has sent the frame but the Pico
			//       hasn't received all the data.

			if(spi0_hw -> sr & SPI_SSPSR_RNE_BITS)
			{
				// Get rx fifo data from dr register.
				inputBuffer[inputBufferPosn++] = spi0_hw -> dr;
			}
			else
			{
				// Check for timeout.
				timeout = get_absolute_time() > timeoutTime;
			}
		}

		if(debugMsgActive && timeout) printf("Timeout during latch command read.");

		// Always reset output buffer read position.
		outputBufferReadPosn = 0;

		bool commandReplyReady = false;

		if(!timeout && inputBufferPosn == SPI_COMMAND_RESPONSE_FRAME_SIZE)
		{
			// Command frame was read.

			// Clear the output buffer.
			outputBufferWritePosn = SPI_COMMAND_RESPONSE_FRAME_SIZE;
			while(outputBufferWritePosn-- > 0) outputBuffer[outputBufferWritePosn] = 0;

			// As default, put the command back into the first position in the return buffer.
			outputBuffer[outputBufferWritePosn++] = inputBuffer[0];

			int latchedDataIndex;

			// Processs the command.
			switch(inputBuffer[0])
			{
				case GET_LATCHED_DATA_INDEX:

					// Get latch data index command is complete.
					if(debugMsgActive) printf("Proc cmd GET_LATCHED_DATA_INDEX\n");

					// Two bytes have to be output.

					// Null terminate the input string.
					inputBuffer[4] = 0;

					// Return latched data index.
					outputBuffer[outputBufferWritePosn++] = getLatchedDataIndex(inputBuffer + 1);

					break;

				case GET_LATCHED_DATA_RESOLUTION:

					// Command is complete.
					if(debugMsgActive) printf("Proc cmd GET_LATCHED_DATA_RESOLUTION\n");

					// Three bytes have to be output.

					latchedDataIndex = inputBuffer[1];
					int latchedDataResolution = getLatchedDataResolution(latchedDataIndex);

					// Latched data. Little endian byte order.
					outputBuffer[outputBufferWritePosn++] = latchedDataResolution & 0xFF;
					outputBuffer[outputBufferWritePosn++] = (latchedDataResolution >> 8) & 0xFF;

					break;

				case GET_LATCHED_DATA:

					// Command is complete.
					if(debugMsgActive) printf("Proc cmd GET_LATCHED_DATA\n");

					// Five bytes have to be output.

					latchedDataIndex = inputBuffer[1];
					int latchedDataVal = latchedData[latchedDataIndex];

					// Latched data. Little endian byte order.
					outputBuffer[outputBufferWritePosn++] = latchedDataVal & 0xFF;
					outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 8) & 0xFF;
					outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 16) & 0xFF;
					outputBuffer[outputBufferWritePosn++] = (latchedDataVal >> 24) & 0xFF;

					break;

				default:

					// Bad command.

					outputBufferWritePosn = 0;
					while(outputBufferWritePosn++ < SPI_COMMAND_RESPONSE_FRAME_SIZE) outputBuffer[outputBufferWritePosn] = 0xFF;

					if(debugMsgActive) ("Unknown SPI command 0x%X", inputBuffer[0]);
			}

			commandReplyReady = true;
		}
		else
		{
			// Command aborted. Wait for next command cycle.
			setReadyForCommand(false);
		}

		if(commandReplyReady)
		{
			if(debugMsgActive && !(spi0_hw -> sr & SPI_SSPSR_TFE_BITS))
			{
				printf("Warning: Latch command transmit FIFO was not empty.");
			}

			// Write as much as possible from output buffer to SPI tx fifo.
			while(outputBufferReadPosn < SPI_COMMAND_RESPONSE_FRAME_SIZE && spiLatchCommandActive &&
				spi0_hw -> sr & SPI_SSPSR_TNF_BITS)
			{
				spi0_hw -> dr = outputBuffer[outputBufferReadPosn++];
			}

			// This should indicate to the master that it can start to read the command response.
			setReadyForCommand(false);

			timeoutTime = make_timeout_time_ms(1);

			// Write any remaining data from output buffer to SPI tx fifo.
			while(outputBufferReadPosn < SPI_COMMAND_RESPONSE_FRAME_SIZE && spiLatchCommandActive && !timeout)
			{
				if(spi0_hw -> sr & SPI_SSPSR_TNF_BITS)
				{
					spi0_hw -> dr = outputBuffer[outputBufferReadPosn++];
				}
				else
				{
					// Check for timeout.
					timeout = get_absolute_time() > timeoutTime;
				}
			}

			if(debugMsgActive && timeout) printf("Timeout during latch command reply.");
		}
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
	gpio_put(SPI_LATCH_READY_FOR_COMMAND_GPIO_PIN, false);

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
