#include "pico/stdlib.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "hardware/regs/intctrl.h"

#include "pico_dash_gpio.h"
#include "pico_dash_latch.h"
#include "pico_dash_spi_latch.h"

/** Currently latched data. */
int latchedData[MAX_LATCHED_INDEXES];

bool debugMsgActive = true;

/**
 * Program for Pi Pico that interfaces to cars electrical signals and can either provide data to another display system
 * or drive physical gauges directly.
 */
int main()
{
	// Make sure disabled interrupts wake up WFE via the SEVONPEND flag of the SCR register.
	// See "M0PLUS: SCR Register" in RP2040 Datasheet.
	// SEVONPEND needs to be set. This allows WFE to wake up on pending interrupt even if disabled.
	uint32_t* scrRegAddr = (uint32_t*)(PPB_BASE + 0xed10);
	*scrRegAddr |= 0x10;

	// Must happen for serial stdout to work.
	stdio_init_all();

	// Must be done before SPI start.
	initGpioIrqSubsystem();

	// For now run the SPI comms on core 0.
	spiLatchStartSubsystem();

	// Main processing loop.
	while(1)
	{
		spiLatchProcess();

		// Disable GPIO interrupts so that SPI master inactive can't be flipped to active before it can trigger WFE
		// to be exited.

		// If any interrupts are pending the SEVONPEND flag will cause SEV to already be set and WFE will return immediately
		// after clearing SEV.

		// IO_IRQ_BANK0 (GPIO)
		irq_set_enabled(IO_IRQ_BANK0, false);

		if(!spiLatchProcReq())
		{
			// Wait for SEV event from GPIO interrupt pending.
			// It doesn't matter if this returns immediately.
			__wfe();
		}

		// IO_IRQ_BANK0 (GPIO)
		irq_set_enabled(IO_IRQ_BANK0, true);
	}
}
